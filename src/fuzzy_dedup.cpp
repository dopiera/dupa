#include "fuzzy_dedup.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <thread>
#include <unordered_set>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>

#include "hash_cache.h"
#include "log.h"
#include "scanner_int.h"
#include "synch_thread_pool.h"

FuzzyDedupRes fuzzy_dedup(std::string const &dir) {
  // Scan the directory and compute checksums for regular files
  std::pair<Node *, detail::Sum2Node> const root_and_sum_2_node =
      detail::ScanDirectory(dir);
  std::shared_ptr<Node> root_node(root_and_sum_2_node.first);
  if (!root_node) {
    // No files at all.
    return FuzzyDedupRes();
  }
  detail::Sum2Node const &sum_2_node = root_and_sum_2_node.second;

  // Create equivalence classes for all regular files
  EqClassesPtr eq_classes =
      detail::ClassifyDuplicateFiles(*root_node, sum_2_node);

  // Create equivalence classes for all empty directories
  {
    std::unique_ptr<EqClass> empty_dirs_class =
        detail::ClassifyEmptyDirs(*root_node);
    if (!empty_dirs_class->IsEmpty()) {
      eq_classes->push_back(std::move(empty_dirs_class));
    }
  }

  // Now we have all leaves covered, let's propagate that all the way up to
  // the root.
  detail::PropagateEquivalence(*root_node, eq_classes);

  // Let's sort the equivalence classes such that the most important ones are
  // in the front.
  detail::SortEqClasses(eq_classes);

  // Calculate how unique directories are.
  detail::CalculateUniqueness(*root_node);

  return std::make_pair(root_node, eq_classes);
}

std::vector<EqClass *> GetInteresingEqClasses(FuzzyDedupRes &all) {
  std::vector<EqClass *> res;
  for (auto &eq_class : *all.second) {
    assert(!eq_class->nodes.empty());
    if (eq_class->nodes.size() == 1) {
      continue;
    }
    bool all_parents_are_dups = true;
    for (Node *node : eq_class->nodes) {
      if (node->GetParent() == nullptr ||
          node->GetParent()->GetEqClass().IsSingle()) {
        all_parents_are_dups = false;
      }
    }
    if (all_parents_are_dups) {
      continue;
    }
    res.push_back(eq_class.get());
  }
  return res;
}

namespace detail {

//======== ScanDirectory =======================================================

struct TreeCtorProcessor : public ScanProcessor<Node *> {
  void File(boost::filesystem::path const &path, Node *const &parent,
            file_info const &f_info) override {
    Node *node = new Node(Node::FILE, path.filename().native(), f_info.size);
    parent->AddChild(node);
    sum2node_.insert(std::make_pair(f_info.sum, node));
  }

  Node *RootDir(boost::filesystem::path const &path) override {
    root_ = std::make_unique<Node>(Node::DIR, path.native());
    return root_.get();
  }

  Node *Dir(boost::filesystem::path const &path, Node *const &parent) override {
    Node *node = new Node(Node::DIR, path.filename().native());
    parent->AddChild(node);
    return node;
  }

  Sum2Node sum2node_;
  std::unique_ptr<Node> root_;
};

std::pair<Node *, Sum2Node> ScanDirectory(std::string const &dir) {
  std::pair<Node *, Sum2Node> res;

  TreeCtorProcessor processor;

  ScanDirectoryOrDb(dir, processor);

  res.second.swap(processor.sum2node_);
  res.first = processor.root_.release();
  return res;
}

//======== ClassifyEmptyDirs ===================================================

namespace {

struct EmptyDirsEqClassFill {
  EmptyDirsEqClassFill() : eq_class(new EqClass) {}

  void OnNode(Node *node) {
    if (node->IsEmptyDir()) {
      eq_class->AddNode(*node);
    }
  }
  std::unique_ptr<EqClass> eq_class;
};

} /* anonymous namespace */

std::unique_ptr<EqClass> ClassifyEmptyDirs(Node &node) {
  EmptyDirsEqClassFill empty_dir_classifier;
  node.Traverse(std::bind(&EmptyDirsEqClassFill::OnNode,
                          std::ref(empty_dir_classifier),
                          std::placeholders::_1));
  return std::move(empty_dir_classifier.eq_class);
}

//======== ClassifyDuplicateFiles ==============================================

EqClassesPtr ClassifyDuplicateFiles(Node & /*node*/,
                                    Sum2Node const &sum_2_node) {
  EqClassesPtr res(new EqClasses);
  for (auto range_start = sum_2_node.begin();
       range_start != sum_2_node.end();) {
    Sum2Node::const_iterator const range_end =
        sum_2_node.equal_range(range_start->first).second;
    // I am traversing the map twice, but it's so much more readable...

    if (range_start != range_end) {
      res->push_back(std::make_unique<EqClass>());
      for (; range_start != range_end; ++range_start) {
        res->back()->AddNode(*range_start->second);
      }
    }
  }
  return res;
}

//======== GetNodesReadyToEval =================================================

namespace {

struct GatherReadyToEval {
  void OnNode(Node *node) {
    if (node->IsReadyToEvaluate() && !node->IsEvaluated()) {
      ready_to_evaluate.push(node);
    }
  }
  std::queue<Node *> ready_to_evaluate;
};

} /* anonymous namespace */

std::queue<Node *> GetNodesReadyToEval(Node &node) {
  GatherReadyToEval gathered;
  node.Traverse(std::bind(&GatherReadyToEval::OnNode, std::ref(gathered),
                          std::placeholders::_1));
  return gathered.ready_to_evaluate;
}

//======== GetClosestNode ======================================================

std::pair<Node *, double> GetClosestNode(Node const &ref,
                                         Nodes const &candidates) {
  auto it = candidates.begin();
  auto min_it = it++;
  double min_dist = NodeDistance(ref, **min_it);

  for (; it != candidates.end(); ++it) {
    assert(*it != &ref);
    assert((*it)->IsEvaluated());
    double const distance = NodeDistance(ref, **it);
    assert(distance < 1.1); // actually <= 1, but it's double

    if (distance < min_dist) {
      min_it = it;
      min_dist = distance;
    }
  }
  return std::make_pair(*min_it, min_dist);
}

//======== PropagateEquivalence ================================================

void PropagateEquivalence(Node &root_node, const EqClassesPtr &eq_classes) {
  std::queue<Node *> ready_to_eval = detail::GetNodesReadyToEval(root_node);

  while (!ready_to_eval.empty()) {
    Node *node = ready_to_eval.front();
    ready_to_eval.pop();

    assert(node->IsReadyToEvaluate());
    assert(!node->IsEvaluated());
    assert(node->GetParent() == nullptr ||
           !node->GetParent()->IsReadyToEvaluate());

    Nodes possible_equivalents = node->GetPossibleEquivalents();
    if (!possible_equivalents.empty()) {
      std::pair<Node *, double> min_elem_and_dist =
          detail::GetClosestNode(*node, possible_equivalents);
      assert(min_elem_and_dist.first);
      if (min_elem_and_dist.second < Conf().tolerable_diff_pct / 100.) {
        min_elem_and_dist.first->GetEqClass().AddNode(*node);
      } else {
        eq_classes->push_back(std::make_unique<EqClass>());
        eq_classes->back()->AddNode(*node);
      }
    } else {
      eq_classes->push_back(std::make_unique<EqClass>());
      eq_classes->back()->AddNode(*node);
    }

    // By modifying this node we could have made the parent ready to
    // evaluate. It is clear it couldn't have been ready at the beginning of
    // this function because this element was not evaluated, so there is no
    // risk we're adding it to the queue twice.
    if (node->GetParent() != nullptr &&
        node->GetParent()->IsReadyToEvaluate()) {
      ready_to_eval.push(node->GetParent());
    }
  }
  assert(root_node.IsEvaluated());
}

//======== SortEqClasses =======================================================

void SortEqClasses(const EqClassesPtr &eq_classes) {
  std::sort(
      eq_classes->begin(), eq_classes->end(),
      [](const std::unique_ptr<EqClass> &a, const std::unique_ptr<EqClass> &b) {
        return a->weight > b->weight;
      });
}

//======== CalculateUniqueness =================================================

namespace {

bool HasDuplicateElsewhere(Node const &n,
                           std::unordered_set<Node const *> const &not_here) {
  // FIXME: perhaps it's worth sorting nodes in EqClasses and doing a binary
  // search here. I'll not do premature optimizations, though.
  for (Node *sibling : n.GetEqClass().nodes) {
    if (not_here.find(sibling) == not_here.end()) {
      return true;
    }
  }
  return false;
}

} // anonymous namespace

CNodes CalculateUniqueness(Node &node) {
  assert(node.IsEvaluated());
  switch (node.GetType()) {
  case Node::FILE:
    node.unique_fraction = node.GetEqClass().IsSingle() ? 1 : 0;
    return CNodes(1, &node);
  case Node::DIR:
    std::unordered_set<Node const *> descendant_nodes;
    for (Node *child : node.GetChildren()) {
      CNodes const &child_nodes = CalculateUniqueness(*child);
      std::copy(child_nodes.begin(), child_nodes.end(),
                std::inserter(descendant_nodes, descendant_nodes.end()));
    }
    double total_weight = 0;
    double unique_weight = 0;
    for (Node const *desc : descendant_nodes) {
      total_weight += desc->GetWeight();
      if (!HasDuplicateElsewhere(*desc, descendant_nodes)) {
        unique_weight += desc->GetWeight();
      }
    }
    node.unique_fraction = (total_weight == 0)
                               ? 0 // empty directory is not unique
                               : (unique_weight / total_weight);
    return CNodes(descendant_nodes.begin(), descendant_nodes.end());
  }
  assert(false); // We really shouldn't be here.
  return CNodes();
}

} /* namespace detail */

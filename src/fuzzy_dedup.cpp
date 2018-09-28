/*
 * (C) Copyright 2018 Marek Dopiera
 *
 * This file is part of dupa.
 *
 * dupa is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dupa is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dupa. If not, see http://www.gnu.org/licenses/.
 */

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

FuzzyDedupRes FuzzyDedup(const std::string &dir) {
  // Scan the directory and compute checksums for regular files
  const std::pair<Node *, detail::Sum2Node> root_and_sum_2_node =
      detail::ScanDirectory(dir);
  std::shared_ptr<Node> root_node(root_and_sum_2_node.first);
  if (!root_node) {
    // No files at all.
    return FuzzyDedupRes();
  }
  const detail::Sum2Node &sum_2_node = root_and_sum_2_node.second;

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
    assert(!eq_class->nodes_.empty());
    if (eq_class->nodes_.size() == 1) {
      continue;
    }
    bool all_parents_are_dups = true;
    for (Node *node : eq_class->nodes_) {
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

class TreeCtorProcessor : public ScanProcessor<Node *> {
 public:
  void File(const boost::filesystem::path &path, Node *const &parent,
            const FileInfo &f_info) override {
    Node *node = new Node(Node::FILE, path.filename().native(), f_info.size_);
    parent->AddChild(node);
    sum2node_.insert(std::make_pair(f_info.sum_, node));
  }

  Node *RootDir(const boost::filesystem::path &path) override {
    root_ = std::make_unique<Node>(Node::DIR, path.native());
    return root_.get();
  }

  Node *Dir(const boost::filesystem::path &path, Node *const &parent) override {
    Node *node = new Node(Node::DIR, path.filename().native());
    parent->AddChild(node);
    return node;
  }

  Sum2Node sum2node_;
  std::unique_ptr<Node> root_;
};

std::pair<Node *, Sum2Node> ScanDirectory(const std::string &dir) {
  std::pair<Node *, Sum2Node> res;

  TreeCtorProcessor processor;

  ScanDirectoryOrDb(dir, processor);

  res.second.swap(processor.sum2node_);
  res.first = processor.root_.release();
  return res;
}

//======== ClassifyEmptyDirs ===================================================

std::unique_ptr<EqClass> ClassifyEmptyDirs(Node &node) {
  auto eq_class = std::make_unique<EqClass>();
  node.Traverse([&eq_class](Node *node) {
    if (node->IsEmptyDir()) {
      eq_class->AddNode(*node);
    }
  });
  return std::move(eq_class);
}

//======== ClassifyDuplicateFiles ==============================================

EqClassesPtr ClassifyDuplicateFiles(Node & /*node*/,
                                    const Sum2Node &sum_2_node) {
  EqClassesPtr res(new EqClasses);
  for (auto range_start = sum_2_node.begin();
       range_start != sum_2_node.end();) {
    const Sum2Node::const_iterator range_end =
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

std::queue<Node *> GetNodesReadyToEval(Node &node) {
  std::queue<Node *> res;
  node.Traverse([&res](Node *node) {
    if (node->IsReadyToEvaluate() && !node->IsEvaluated()) {
      res.push(node);
    }
  });
  return res;
}

//======== GetClosestNode ======================================================

std::pair<Node *, double> GetClosestNode(const Node &ref,
                                         const Nodes &candidates) {
  auto it = candidates.begin();
  auto min_it = it++;
  double min_dist = NodeDistance(ref, **min_it);

  for (; it != candidates.end(); ++it) {
    assert(*it != &ref);
    assert((*it)->IsEvaluated());
    const double distance = NodeDistance(ref, **it);
    assert(distance < 1.1);  // actually <= 1, but it's double

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
      if (min_elem_and_dist.second < Conf().tolerable_diff_pct_ / 100.) {
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
        return a->weight_ > b->weight_;
      });
}

//======== CalculateUniqueness =================================================

namespace {

bool HasDuplicateElsewhere(const Node &n,
                           const std::unordered_set<const Node *> &not_here) {
  // FIXME: perhaps it's worth sorting nodes in EqClasses and doing a binary
  // search here. I'll not do premature optimizations, though.
  for (Node *sibling : n.GetEqClass().nodes_) {
    if (not_here.find(sibling) == not_here.end()) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

CNodes CalculateUniqueness(Node &node) {
  assert(node.IsEvaluated());
  switch (node.GetType()) {
    case Node::FILE:
      node.unique_fraction_ = node.GetEqClass().IsSingle() ? 1 : 0;
      return CNodes(1, &node);
    case Node::DIR:
      std::unordered_set<const Node *> descendant_nodes;
      for (Node *child : node.GetChildren()) {
        const CNodes &child_nodes = CalculateUniqueness(*child);
        std::copy(child_nodes.begin(), child_nodes.end(),
                  std::inserter(descendant_nodes, descendant_nodes.end()));
      }
      double total_weight = 0;
      double unique_weight = 0;
      for (const Node *desc : descendant_nodes) {
        total_weight += desc->GetWeight();
        if (!HasDuplicateElsewhere(*desc, descendant_nodes)) {
          unique_weight += desc->GetWeight();
        }
      }
      node.unique_fraction_ = (total_weight == 0)
                                  ? 0  // empty directory is not unique
                                  : (unique_weight / total_weight);
      return CNodes(descendant_nodes.begin(), descendant_nodes.end());
  }
  assert(false);  // We really shouldn't be here.
  return CNodes();
}

} /* namespace detail */

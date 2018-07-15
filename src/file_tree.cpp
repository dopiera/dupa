#include "file_tree.h"

#include <cstdint>

#include <algorithm>
#include <cassert>
#include <iostream>

#include <unordered_map>
#include <unordered_set>

#include "conf.h"

void Node::AddChild(Node *child) {
  assert(!IsEvaluated());
  assert(child->parent_ == nullptr);
  assert(!child->IsEvaluated()); // This is due to not_evaluated_children
  assert(type_ == DIR);
  child->parent_ = this;
  this->children_.push_back(child);
  ++this->not_evaluated_children_;
}

Node::~Node() {
  for (Node *n : this->children_) {
    delete n;
  }
}

void Node::SetEqClass(EqClass *eq_class) {
  assert(IsReadyToEvaluate());
  assert(!IsEvaluated());
  this->eq_class_ = eq_class;
  if (this->parent_) {
    assert(this->parent_->not_evaluated_children_);
    --this->parent_->not_evaluated_children_;
  }
}

double Node::GetWeight() const {
  if (IsEvaluated()) {
    return eq_class_->weight_;
  }

  switch (type_) {
  case FILE:
    if (Conf().use_size_) {
      return this->size_;
    } else {
      return 1;
    }
  case DIR: {
    std::unordered_set<EqClass *> eq_classes;
    double weight = 0;
    for (Node const *const n : this->children_) {
      assert(n->IsEvaluated());
      eq_classes.insert(&n->GetEqClass());
    }
    for (EqClass const *const eq_class : eq_classes) {
      weight += eq_class->weight_;
    }
    return weight;
  }
  }
  assert(false); // We shouldn't reach it;
  return 0;
}

boost::filesystem::path Node::BuildPath() const {
  if (parent_) {
    return parent_->BuildPath() /= boost::filesystem::path(name_);
  }
  return boost::filesystem::path(name_);
}

Nodes Node::GetPossibleEquivalents() const {
  assert(this->IsReadyToEvaluate());
  std::unordered_set<Node *> nodes;
  for (Node const *child : this->children_) {
    assert(child->IsEvaluated());
    for (Node const *equivalent : child->eq_class_->nodes_) {
      assert(equivalent->IsEvaluated());
      if (equivalent != child && equivalent->parent_ &&
          equivalent->parent_->IsEvaluated() && equivalent->parent_ != this) {
        nodes.insert(equivalent->parent_);
      }
    }
  }
  return Nodes(nodes.begin(), nodes.end());
}

void Node::Traverse(const std::function<void(Node *)> &callback) {
  for (Node *const child : this->children_) {
    child->Traverse(callback);
  }
  callback(this);
}

void Node::Traverse(const std::function<void(Node const *)> &callback) const {
  for (Node const *const child : this->children_) {
    child->Traverse(callback);
  }
  callback(this);
}

bool Node::IsAncestorOf(Node const &node) {
  Node const *n = &node;
  for (; n && n != this; n = n->parent_) {
    {
    }
  }
  return n != nullptr;
}

double NodeDistance(Node const &n1, Node const &n2) {
  assert(n1.IsReadyToEvaluate());
  assert(n2.IsReadyToEvaluate());
  // Not supported. All those comparisons should be done from outside using a
  // hash table for efficiency.
  assert(n1.type_ != Node::FILE || n2.type_ != Node::FILE);

  std::unordered_map<EqClass *, bool> eq_classes1;
  std::unordered_set<EqClass *> eq_classes_only_2;

  for (Node const *const n : n1.children_) {
    eq_classes1.insert(std::make_pair(n->eq_class_, false));
  }
  for (Node const *const n : n2.children_) {
    auto in1_it = eq_classes1.find(n->eq_class_);
    if (in1_it == eq_classes1.end()) {
      eq_classes_only_2.insert(n->eq_class_);
    } else {
      in1_it->second = true;
    }
  }

  uint64_t sum = 0;
  uint64_t sym_diff = 0;

  for (auto const &eq_class_and_intersect : eq_classes1) {
    sum += eq_class_and_intersect.first->weight_;
    if (!eq_class_and_intersect.second) {
      // only in n1
      sym_diff += eq_class_and_intersect.first->weight_;
    }
  }
  for (EqClass const *const eq_class : eq_classes_only_2) {
    sum += eq_class->weight_;
    sym_diff += eq_class->weight_;
  }
  if (sum == 0) {
    // both are empty directories, so they are the same
    return 0;
  }
  assert(sum >= sym_diff);
  return static_cast<double>(sym_diff) / sum;
}

void EqClass::AddNode(Node &node) {
  assert(find(nodes_.begin(), nodes_.end(), &node) == nodes_.end());
  nodes_.push_back(&node);
  weight_ = (weight_ * (nodes_.size() - 1) + node.GetWeight()) / nodes_.size();
  node.SetEqClass(this);
}

namespace {

struct NodePathOrder : public std::binary_function<Node *, Node *, bool> {
  bool operator()(Node const *n1, Node const *n2) const {
    return n1->BuildPath().native() < n2->BuildPath().native();
  }
};

struct UniquenessOrder : public std::binary_function<Node *, Node *, bool> {
  bool operator()(Node const *n1, Node const *n2) const {
    return n1->unique_fraction_ > n2->unique_fraction_;
  }
};

} // anonymous namespace

void PrintEqClassses(std::vector<EqClass *> const &eq_classes) {
  std::cout << "*** Classes of similar directories or files:" << std::endl;
  for (EqClass const *eq_class : eq_classes) {
    Nodes to_print(eq_class->nodes_);
    std::sort(to_print.begin(), to_print.end(), NodePathOrder());
    for (auto node_it = to_print.begin(); node_it != to_print.end();
         ++node_it) {
      std::cout << (*node_it)->BuildPath().native();
      if (--to_print.end() != node_it) {
        std::cout << " ";
      }
    }
    std::cout << std::endl;
  }
}

void PrintScatteredDirectories(Node const &root) {
  std::cout << "*** Directories consisting of mostly duplicates of files "
               "scattered elsewhere:"
            << std::endl;
  CNodes scattered_dirs;
  root.Traverse([&scattered_dirs](Node const *n) {
    if (n->GetType() == Node::DIR &&
        n->unique_fraction_ * 100 < Conf().tolerable_diff_pct_ &&
        n->GetEqClass().IsSingle()) {
      scattered_dirs.push_back(n);
    }
  });
  std::sort(scattered_dirs.begin(), scattered_dirs.end(), UniquenessOrder());
  for (Node const *dir : scattered_dirs) {
    std::cout << dir->BuildPath().native() << std::endl;
  }
}

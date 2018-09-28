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
  assert(!child->IsEvaluated());  // This is due to not_evaluated_children
  assert(type_ == DIR);
  child->parent_ = this;
  children_.push_back(child);
  ++not_evaluated_children_;
}

Node::~Node() {
  for (Node *n : children_) {
    delete n;
  }
}

void Node::SetEqClass(EqClass *eq_class) {
  assert(IsReadyToEvaluate());
  assert(!IsEvaluated());
  eq_class_ = eq_class;
  if (parent_) {
    assert(parent_->not_evaluated_children_);
    --parent_->not_evaluated_children_;
  }
}

double Node::GetWeight() const {
  if (IsEvaluated()) {
    return eq_class_->weight_;
  }

  switch (type_) {
    case FILE:
      if (Conf().use_size_) {
        return size_;
      } else {
        return 1;
      }
    case DIR: {
      std::unordered_set<EqClass *> eq_classes;
      double weight = 0;
      for (const Node *const n : children_) {
        assert(n->IsEvaluated());
        eq_classes.insert(&n->GetEqClass());
      }
      for (const EqClass *const eq_class : eq_classes) {
        weight += eq_class->weight_;
      }
      return weight;
    }
  }
  assert(false);  // We shouldn't reach it;
  return 0;
}

boost::filesystem::path Node::BuildPath() const {
  if (parent_) {
    return parent_->BuildPath() /= boost::filesystem::path(name_);
  }
  return boost::filesystem::path(name_);
}

Nodes Node::GetPossibleEquivalents() const {
  assert(IsReadyToEvaluate());
  std::unordered_set<Node *> nodes;
  for (const Node *child : children_) {
    assert(child->IsEvaluated());
    for (const Node *equivalent : child->eq_class_->nodes_) {
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
  for (Node *const child : children_) {
    child->Traverse(callback);
  }
  callback(this);
}

void Node::Traverse(const std::function<void(const Node *)> &callback) const {
  for (const Node *const child : children_) {
    child->Traverse(callback);
  }
  callback(this);
}

bool Node::IsAncestorOf(const Node &node) {
  const Node *n = &node;
  for (; n && n != this; n = n->parent_) {
    {
    }
  }
  return n != nullptr;
}

double NodeDistance(const Node &n1, const Node &n2) {
  assert(n1.IsReadyToEvaluate());
  assert(n2.IsReadyToEvaluate());
  // Not supported. All those comparisons should be done from outside using a
  // hash table for efficiency.
  assert(n1.type_ != Node::FILE || n2.type_ != Node::FILE);

  std::unordered_map<EqClass *, bool> eq_classes1;
  std::unordered_set<EqClass *> eq_classes_only_2;

  for (const Node *const n : n1.children_) {
    eq_classes1.insert(std::make_pair(n->eq_class_, false));
  }
  for (const Node *const n : n2.children_) {
    auto in1_it = eq_classes1.find(n->eq_class_);
    if (in1_it == eq_classes1.end()) {
      eq_classes_only_2.insert(n->eq_class_);
    } else {
      in1_it->second = true;
    }
  }

  uint64_t sum = 0;
  uint64_t sym_diff = 0;

  for (const auto &eq_class_and_intersect : eq_classes1) {
    sum += eq_class_and_intersect.first->weight_;
    if (!eq_class_and_intersect.second) {
      // only in n1
      sym_diff += eq_class_and_intersect.first->weight_;
    }
  }
  for (const EqClass *const eq_class : eq_classes_only_2) {
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

void PrintEqClassses(const std::vector<EqClass *> &eq_classes) {
  std::cout << "*** Classes of similar directories or files:" << std::endl;
  for (const EqClass *eq_class : eq_classes) {
    Nodes to_print(eq_class->nodes_);
    std::sort(to_print.begin(), to_print.end(),
              [](const Node *n1, const Node *n2) {
                return n1->BuildPath().native() < n2->BuildPath().native();
              });
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

void PrintScatteredDirectories(const Node &root) {
  std::cout << "*** Directories consisting of mostly duplicates of files "
               "scattered elsewhere:"
            << std::endl;
  CNodes scattered_dirs;
  root.Traverse([&scattered_dirs](const Node *n) {
    if (n->GetType() == Node::DIR &&
        n->unique_fraction_ * 100 < Conf().tolerable_diff_pct_ &&
        n->GetEqClass().IsSingle()) {
      scattered_dirs.push_back(n);
    }
  });
  std::sort(scattered_dirs.begin(), scattered_dirs.end(),
            [](const Node *n1, const Node *n2) {
              return n1->unique_fraction_ > n2->unique_fraction_;
            });
  for (const Node *dir : scattered_dirs) {
    std::cout << dir->BuildPath().native() << std::endl;
  }
}

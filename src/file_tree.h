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

#ifndef SRC_FILE_TREE_H_
#define SRC_FILE_TREE_H_

#include <cassert>
#include <functional>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "log.h"

class Node;
class EqClass;

using Nodes = std::vector<Node *>;
using CNodes = std::vector<const Node *>;

// FIXME: the tree structure should be separated from the things computed on it.
// The only reason why it is not is my laziness.
class Node {
 public:
  enum Type {
    DIR,
    FILE,
  };

  // Size is only meaningful for regular files.
  Node(Type type, std::string name, off_t size = 0)
      : name_(std::move(name)),
        type_(type),
        size_(size),
        parent_(nullptr),
        eq_class_(nullptr),
        not_evaluated_children_() {
    assert(!name_.empty());
    DLOG("Created file: '" << BuildPath().native() << "' with size " << size_
                           << " and type " << type_);
  }
  Node(const Node &n) = delete;
  Node &operator=(const Node &n) = delete;

  void AddChild(Node *child);  // takes ownership
  bool IsReadyToEvaluate() const { return not_evaluated_children_ == 0; }
  bool IsEvaluated() const { return eq_class_ != nullptr; }
  EqClass &GetEqClass() const {
    assert(eq_class_);
    return *eq_class_;
  }
  Type GetType() const { return type_; }
  bool IsEmptyDir() const { return GetType() == DIR && children_.empty(); }
  boost::filesystem::path BuildPath() const;
  double GetWeight() const;
  const std::string &GetName() const { return name_; }
  Node *GetParent() { return parent_; }
  // Return all nodes which are evaluated and share a child with this one.
  Nodes GetPossibleEquivalents() const;
  // Traverse the whole subtree (including this node) in a an unspecified
  // order and call callback on every node
  void Traverse(const std::function<void(Node *)> &callback);
  void Traverse(const std::function<void(const Node *)> &callback) const;
  const Nodes &GetChildren() const { return children_; }
  bool IsAncestorOf(const Node &node);

  ~Node();

 private:
  void SetEqClass(EqClass *eq_class);

  std::string name_;
  Type type_;
  off_t size_;
  Node *parent_;
  Nodes children_;
  EqClass *eq_class_;
  int not_evaluated_children_;

 public:
  // FIXME: this is a great indication that separation is broken here
  double unique_fraction_;

  friend double NodeDistance(const Node &n1, const Node &n2);
  friend class EqClass;
};

// 0 for identical, 1 for no overlap; (symmetrical diffrence) / (union)
double NodeDistance(const Node &n1, const Node &n2);

class EqClass {
 public:
  EqClass() = default;
  EqClass(const EqClass &) = delete;
  EqClass &operator=(const EqClass &) = delete;

  bool IsEmpty() const { return nodes_.empty(); }
  bool IsSingle() const { return nodes_.size() == 1; }
  double GetWeight() const { return weight_; }
  size_t GetNumNodes() const { return nodes_.size(); }

  void AddNode(Node &node);  // does not take ownership
  Nodes nodes_;
  double weight_{};
};

// Filter out only equivalence classes which have duplicates and are not already
// described by their parents being duplicates of something else.
void PrintEqClassses(const std::vector<EqClass *> &eq_classes);

// Print directories which have no duplicates but whose contents are mostly
// duplicated to file outside of it.
void PrintScatteredDirectories(const Node &root);

#endif  // SRC_FILE_TREE_H_

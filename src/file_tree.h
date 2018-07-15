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
  Node(Type type, std::string const &name, off_t size = 0)
      : name_(name),
        type_(type),
        size_(size),
        parent_(nullptr),
        eq_class_(nullptr),
        not_evaluated_children_() {
    assert(!name.empty());
    DLOG("Created file: '" << this->BuildPath().native() << "' with size "
                           << this->size_ << " and type " << this->type_);
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
  Type GetType() const { return this->type_; }
  bool IsEmptyDir() const { return GetType() == DIR && children_.empty(); }
  boost::filesystem::path BuildPath() const;
  double GetWeight() const;
  std::string const &GetName() const { return this->name_; }
  Node *GetParent() { return parent_; }
  // Return all nodes which are evaluated and share a child with this one.
  Nodes GetPossibleEquivalents() const;
  // Traverse the whole subtree (including this node) in a an unspecified
  // order and call callback on every node
  void Traverse(const std::function<void(Node *)> &callback);
  void Traverse(const std::function<void(Node const *)> &callback) const;
  Nodes const &GetChildren() const { return this->children_; }
  bool IsAncestorOf(Node const &node);

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

  friend double NodeDistance(Node const &n1, Node const &n2);
  friend class EqClass;
};

// 0 for identical, 1 for no overlap; (symmetrical diffrence) / (union)
double NodeDistance(Node const &n1, Node const &n2);

class EqClass {
 public:
  EqClass() = default;
  EqClass(const EqClass &) = delete;
  EqClass &operator=(const EqClass &) = delete;

  bool IsEmpty() const { return nodes_.empty(); }
  bool IsSingle() const { return nodes_.size() == 1; }
  double GetWeight() const { return this->weight_; }
  size_t GetNumNodes() const { return this->nodes_.size(); }

  void AddNode(Node &node);  // does not take ownership
  Nodes nodes_;
  double weight_{};
};

// Filter out only equivalence classes which have duplicates and are not already
// described by their parents being duplicates of something else.
void PrintEqClassses(std::vector<EqClass *> const &eq_classes);

// Print directories which have no duplicates but whose contents are mostly
// duplicated to file outside of it.
void PrintScatteredDirectories(Node const &root);

#endif  // SRC_FILE_TREE_H_

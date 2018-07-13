#ifndef FILE_TREE_H_4234
#define FILE_TREE_H_4234

#include <cassert>
#include <functional>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>

#include "log.h"

class Node;
class EqClass;

typedef std::vector<Node *> Nodes;
typedef std::vector<Node const *> CNodes;

// FIXME: the tree structure should be separated from the things computed on it.
// The only reason why it is not is my laziness.
class Node : private boost::noncopyable {
public:
  enum Type {
    DIR,
    FILE,
  };

  // Size is only meaningful for regular files.
  Node(Type type, std::string const &name, off_t size = 0)
      : name(name), type(type), size(size), parent(NULL), eq_class(NULL),
        not_evaluated_children() {
    assert(name != "");
    DLOG("Created file: '" << this->BuildPath().native() << "' with size "
                           << this->size << " and type " << this->type);
  }

  void AddChild(Node *child); // takes ownership
  bool IsReadyToEvaluate() const { return not_evaluated_children == 0; }
  bool IsEvaluated() const { return eq_class != NULL; }
  EqClass &GetEqClass() const {
    assert(eq_class);
    return *eq_class;
  }
  Type GetType() const { return this->type; }
  bool IsEmptyDir() const { return GetType() == DIR && children.empty(); }
  boost::filesystem::path BuildPath() const;
  double GetWeight() const;
  std::string const &GetName() const { return this->name; }
  Node *GetParent() { return parent; }
  // Return all nodes which are evaluated and share a child with this one.
  Nodes GetPossibleEquivalents() const;
  // Traverse the whole subtree (including this node) in a an unspecified
  // order and call callback on every node
  void Traverse(std::function<void(Node *)> callback);
  void Traverse(std::function<void(Node const *)> callback) const;
  Nodes const &GetChildren() const { return this->children; }
  bool IsAncestorOf(Node const &node);

  ~Node();

private:
  void SetEqClass(EqClass *eq_class);

  std::string name;
  Type type;
  off_t size;
  Node *parent;
  Nodes children;
  EqClass *eq_class;
  int not_evaluated_children;

public:
  // FIXME: this is a great indication that separation is broken here
  double unique_fraction;

  friend double NodeDistance(Node const &n1, Node const &n2);
  friend class EqClass;
};

// 0 for identical, 1 for no overlap; (symmetrical diffrence) / (union)
double NodeDistance(Node const &n1, Node const &n2);

class EqClass : private boost::noncopyable {
public:
  EqClass() : nodes(), weight() {}
  bool IsEmpty() const { return nodes.empty(); }
  bool IsSingle() const { return nodes.size() == 1; }
  double GetWeight() const { return this->weight; }
  size_t GetNumNodes() const { return this->nodes.size(); }

  void AddNode(Node &node); // does not take ownership
  Nodes nodes;
  double weight;
};

// Filter out only equivalence classes which have duplicates and are not already
// described by their parents being duplicates of something else.
void PrintEqClassses(std::vector<EqClass *> const &eq_classes);

// Print directories which have no duplicates but whose contents are mostly
// duplicated to file outside of it.
void PrintScatteredDirectories(Node const &root);

#endif /* FILE_TREE_H_4234 */

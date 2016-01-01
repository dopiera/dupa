#ifndef FILE_TREE_H_4234
#define FILE_TREE_H_4234

#include <cassert>
#include <boost/function.hpp>
#include <vector>
#include <string>

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>

class Node;
class EqClass;

typedef std::vector<Node*> Nodes;

class Node : private boost::noncopyable {
public:
	enum Type {
		DIR,
		FILE,
		OTHER
	};

	Node(Type type, std::string const & name) :
		name(name),
		type(type),
		parent(NULL),
		eq_class(NULL),
		not_evaluated_children()
	{
		assert(name != "");
	}

	void AddChild(Node *child); // takes ownership
	bool IsReadyToEvaluate() const { return not_evaluated_children == 0; }
	bool IsEvaluated() const { return eq_class != NULL; }
	EqClass &GetEqClass() const { assert(eq_class); return *eq_class; }
	Type GetType() const { return this->type; }
	bool IsEmptyDir() const { return GetType() == DIR && children.empty(); }
	boost::filesystem::path BuildPath() const;
	double GetWeight() const;
	Node * GetParent() { return parent; }
	// Return all nodes which are evaluated and share a child with this one.
	Nodes GetPossibleEquivalents() const;
	// Traverse the whole subtree (including this node) in a an unspecified
	// order and call callback on every node
	void Traverse(boost::function<void(Node*)> callback);

	~Node();

private:
	void SetEqClass(EqClass * eq_class);


	std::string name;
	Type type;
	Node *parent;
	Nodes children;
	EqClass *eq_class;
	int not_evaluated_children;

	friend double NodeDistance(Node const &n1, Node const &n2);
	friend class EqClass;
};

// 0 for identical, 1 for no overlap; (symmetrical diffrence) / (union)
double NodeDistance(Node const &n1, Node const &n2);

struct EqClass : private boost::noncopyable {
	EqClass(): nodes(), weight() {}
	bool IsEmpty() const { return nodes.empty(); }
	bool IsSingle() const { return nodes.size() == 1; }

	void AddNode(Node &node); // does not take ownership
	Nodes nodes;
	double weight;
};

#endif /* FILE_TREE_H_4234 */

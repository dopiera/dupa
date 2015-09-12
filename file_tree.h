#ifndef FILE_TREE_H_4234
#define FILE_TREE_H_4234

#include <vector>
#include <string>

class Node;
class EqClass;

typedef std::vector<Node*> Nodes;

class Node {
public:
	enum Type {
		DIR,
		FILE,
		OTHER
	};

	Node(Type type, std::string const & path, Node const * parent) :
		path(path), parent(parent), eq_class(NULL), not_evaluated_children()
	{
	}

	void AddChild(Node *child); // takes ownership
	bool IsReadyToEvaluate() { return not_evaluated_children == 0; }
	bool IsEvaluated() { return eq_class != NULL; }
	void SetEquivalenceCl(EqClass * eq_class);

	~Node();

private:
	std::string path;
	Node const *parent;
	Nodes children;
	EqClass *eq_class;
	int not_evaluated_children;
};

class EqClass {
	Nodes nodes;
	double diameter;
	int weight;
};

#endif /* FILE_TREE_H_4234 */

#include "file_tree.h"

#include <utility>

#include <boost/bind.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "gtest/gtest.h"

namespace boost {
namespace filesystem {

// Make boost::filesystem::path printable so that assertions have meaningful
// text.
void PrintTo(path const &p, std::ostream  *os) {
	(*os) << p.native();
}

} /* namespace filesystem */
} /* namespace boost */

void PrintTo(Node const*n, std::ostream  *os) {
	boost::filesystem::PrintTo(n->BuildPath(), os);
}

typedef boost::ptr_vector<EqClass> EqClasses;
typedef boost::shared_ptr<EqClasses> EqClassesPtr;
typedef std::pair<Node*, EqClassesPtr> NodeAndClasses;
static NodeAndClasses CreateNodeWithWeight(int weight) {
	EqClassesPtr eq_classes(new EqClasses);
	Node * n = new Node(Node::DIR, "dsa");
	for (int i = 0; i < weight; ++i) {
		Node *n_child = new Node(Node::FILE, "xyz");
		n->AddChild(n_child);
		EqClass *eq_class = new EqClass;
		eq_classes->push_back(eq_class);
		eq_class->AddNode(*n_child);
	}
	return make_pair(n, eq_classes);
}

TEST(NodeTest, EmptyDir) {
	Node n(Node::DIR, "aaa");
	ASSERT_TRUE(n.IsReadyToEvaluate());
	ASSERT_FALSE(n.IsEvaluated());
	EqClass dummy;
	dummy.AddNode(n);
	ASSERT_TRUE(n.IsEvaluated());
	ASSERT_EQ(&n.GetEqClass(), &dummy);
}

TEST(NodeTest, SingleFile) {
	Node n(Node::FILE, "aaa");
	ASSERT_TRUE(n.IsReadyToEvaluate());
	ASSERT_FALSE(n.IsEvaluated());
	EqClass dummy;
	dummy.AddNode(n);
	ASSERT_TRUE(n.IsEvaluated());
	ASSERT_EQ(&n.GetEqClass(), &dummy);
}

TEST(NodeTest, OneChild) {
	Node n(Node::DIR, "aaa");
	ASSERT_TRUE(n.IsReadyToEvaluate());
	ASSERT_FALSE(n.IsEvaluated());

	Node *child = new Node(Node::FILE, "bbb");
	n.AddChild(child);
	ASSERT_FALSE(n.IsReadyToEvaluate());
	ASSERT_FALSE(n.IsEvaluated());

	EqClass dummy;
	dummy.AddNode(*child);
	ASSERT_TRUE(child->IsEvaluated());
	ASSERT_TRUE(n.IsReadyToEvaluate());
	ASSERT_FALSE(n.IsEvaluated());

	dummy.AddNode(n);
	ASSERT_TRUE(n.IsEvaluated());
}

TEST(NodeTest, PathBuilding) {
	Node n(Node::DIR, "aaa");
	Node *child = new Node(Node::FILE, "bbb");
	n.AddChild(child);

	EqClass dummy;
	dummy.AddNode(*child);
	dummy.AddNode(n);

	namespace bf = boost::filesystem;
	ASSERT_EQ(n.BuildPath(), bf::path("aaa"));
	ASSERT_EQ(child->BuildPath(), bf::path("aaa") / bf::path("bbb"));
}

TEST(NodeTest, GetPossibleEquivalents) {
	//       n1         n2    n3 (not evaluated)
	//    /  |  \        |     |
	//  nc--nc0  nc1 -- nc2-- nc3 -- nc4

	Node n1(Node::DIR, "n1");
	Node n2(Node::DIR, "n2");
	Node n3(Node::DIR, "n3");
	Node *nc = new Node(Node::FILE, "nc");
	n1.AddChild(nc);
	Node *nc0 = new Node(Node::FILE, "nc0");
	n1.AddChild(nc0);
	Node *nc1 = new Node(Node::FILE, "nc1");
	n1.AddChild(nc1);
	Node *nc2 = new Node(Node::FILE, "nc2");
	n2.AddChild(nc2);
	Node *nc3 = new Node(Node::FILE, "nc3");
	n3.AddChild(nc3);
	Node nc4(Node::FILE, "nc4");

	EqClass lower_0;
	lower_0.AddNode(*nc);
	lower_0.AddNode(*nc0);

	EqClass lower_1;
	lower_1.AddNode(*nc1);
	lower_1.AddNode(*nc2);
	lower_1.AddNode(*nc3);
	lower_1.AddNode(nc4);

	EqClass upper_n2;
	upper_n2.AddNode(n2);

	Nodes expected;
	expected.push_back(&n2);
	ASSERT_EQ(n1.GetPossibleEquivalents(), expected);
}

struct NodeGatherer {
	void OnNode(Node *node) {
		nodes.push_back(node);
	}

	Nodes nodes;
};

TEST(NodeTest, Traverse) {
	Node n1(Node::DIR, "n1");
	Node *n2 = new Node(Node::DIR, "n2");
	n1.AddChild(n2);
	Node *n3 = new Node(Node::DIR, "n3");
	n2->AddChild(n3);
	Node *n4 = new Node(Node::DIR, "n4");
	n2->AddChild(n4);

	Nodes expected;
	expected.push_back(&n1);
	expected.push_back(n2);
	expected.push_back(n3);
	expected.push_back(n4);
	std::sort(expected.begin(), expected.end());

	NodeGatherer gatherer;
	n1.Traverse(boost::bind(&NodeGatherer::OnNode, boost::ref(gatherer), _1));
	std::sort(gatherer.nodes.begin(), gatherer.nodes.end());
	ASSERT_EQ(gatherer.nodes, expected);
}

TEST(NodeWeight, File) {
	Node n(Node::FILE, "abc");
	ASSERT_DOUBLE_EQ(n.GetWeight(), 1);
}

TEST(NodeWeight, EmptyDir) {
	Node n(Node::DIR, "abc");
	ASSERT_DOUBLE_EQ(n.GetWeight(), 0);
}

TEST(NodeWeight, MultipleFiles) {
	Node n(Node::DIR, "abc");
	ASSERT_DOUBLE_EQ(n.GetWeight(), 0);
	Node *n_child = new Node(Node::FILE, "xyz");
	EqClass eq_class;
	n.AddChild(n_child);
	eq_class.AddNode(*n_child);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 1);
	Node *n_child2 = new Node(Node::FILE, "xyz");
	EqClass eq_class2;
	n.AddChild(n_child2);
	eq_class2.AddNode(*n_child2);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 2);
	Node *n_child3 = new Node(Node::FILE, "xyz");
	EqClass eq_class3;
	n.AddChild(n_child3);
	eq_class3.AddNode(*n_child3);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 3);
	// Child in the same equivalence class doesn't change the result:
	Node *n_child4 = new Node(Node::FILE, "xyz");
	n.AddChild(n_child4);
	eq_class3.AddNode(*n_child4);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 3);
}

TEST(EqClassWeight, Empty) {
	ASSERT_DOUBLE_EQ(EqClass().weight, 0);
}

TEST(EqClassWeight, SingleNode) {
	EqClass eq_class;
	Node n(Node::FILE, "abc");
	eq_class.AddNode(n);
	ASSERT_DOUBLE_EQ(eq_class.weight, 1);
}

TEST(EqClassWeight, Simple) {
	Node n(Node::DIR, "abc");
	ASSERT_DOUBLE_EQ(n.GetWeight(), 0);
	Node *n_child = new Node(Node::FILE, "xyz");
	n.AddChild(n_child);
	EqClass eq_class1;
	eq_class1.AddNode(*n_child);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 1);
	Node *n_child2 = new Node(Node::FILE, "xyz");
	n.AddChild(n_child2);
	EqClass eq_class2;
	eq_class2.AddNode(*n_child2);
	ASSERT_DOUBLE_EQ(n.GetWeight(), 2);
	EqClass top_class;
	top_class.AddNode(n);
	ASSERT_DOUBLE_EQ(top_class.weight, 2);
}

TEST(EqClassWeight, AvgWorks) {
	NodeAndClasses n1 = CreateNodeWithWeight(3);
	ASSERT_DOUBLE_EQ(n1.first->GetWeight(), 3);
	NodeAndClasses n2 = CreateNodeWithWeight(7);
	ASSERT_DOUBLE_EQ(n2.first->GetWeight(), 7);
	NodeAndClasses n3 = CreateNodeWithWeight(11);
	ASSERT_DOUBLE_EQ(n3.first->GetWeight(), 11);
	EqClass eq_class;
	eq_class.AddNode(*n1.first);
	ASSERT_DOUBLE_EQ(eq_class.weight, 3);
	eq_class.AddNode(*n2.first);
	ASSERT_DOUBLE_EQ(eq_class.weight, 5);
	ASSERT_DOUBLE_EQ(n1.first->GetWeight(), 5);
	ASSERT_DOUBLE_EQ(n2.first->GetWeight(), 5);
	ASSERT_DOUBLE_EQ(n3.first->GetWeight(), 11);
	eq_class.AddNode(*n3.first);
	ASSERT_DOUBLE_EQ(eq_class.weight, 7);
	ASSERT_DOUBLE_EQ(n1.first->GetWeight(), 7);
	ASSERT_DOUBLE_EQ(n2.first->GetWeight(), 7);
	ASSERT_DOUBLE_EQ(n3.first->GetWeight(), 7);
	delete n3.first;
	delete n2.first;
	delete n1.first;
}

TEST(NodeDistance, Empty) {
	Node n(Node::DIR, "aaa");
	Node n2(Node::DIR, "bbb");
	ASSERT_DOUBLE_EQ(NodeDistance(n, n2), 0);
}

TEST(NodeDistance, EmptyNonEmpty) {
	Node n(Node::DIR, "aaa");
	ASSERT_DOUBLE_EQ(NodeDistance(n, n), 0);
	Node n2(Node::DIR, "bbb");
	Node *n2_child = new Node(Node::FILE, "abc");
	n2.AddChild(n2_child);
	EqClass eq_class;
	eq_class.AddNode(*n2_child);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n2), 0);
	ASSERT_DOUBLE_EQ(NodeDistance(n, n2), 1);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n), 1);
}

TEST(NodeDistance, IdenticalOneElemDirs) {
	Node n(Node::DIR, "aaa");
	Node *n_child = new Node(Node::FILE, "xyz");
	Node n2(Node::DIR, "bbb");
	Node *n2_child = new Node(Node::FILE, "abc");
	n.AddChild(n_child);
	n2.AddChild(n2_child);

	EqClass eq_class;
	eq_class.AddNode(*n_child);
	eq_class.AddNode(*n2_child);

	ASSERT_DOUBLE_EQ(NodeDistance(n, n2), 0);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n), 0);
}

TEST(NodeDistance, DifferentOneElemDirs) {
	Node n(Node::DIR, "aaa");
	Node *n_child = new Node(Node::FILE, "xyz");
	Node n2(Node::DIR, "bbb");
	Node *n2_child = new Node(Node::FILE, "abc");
	n.AddChild(n_child);
	n2.AddChild(n2_child);

	EqClass eq_class;
	EqClass eq_class2;
	eq_class.AddNode(*n_child);
	eq_class2.AddNode(*n2_child);
	ASSERT_DOUBLE_EQ(NodeDistance(n, n2), 1);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n), 1);
}

TEST(NodeDistance, StrictlyLarger) {
	Node n1(Node::DIR, "dsa");
	NodeAndClasses n_child1 = CreateNodeWithWeight(9);
	NodeAndClasses n_child2 = CreateNodeWithWeight(1);
	n1.AddChild(n_child1.first);
	n1.AddChild(n_child2.first);

	Node n2(Node::DIR, "asd");
	NodeAndClasses n_child3 = CreateNodeWithWeight(9);
	n2.AddChild(n_child3.first);

	EqClass eq_class_sz_1;
	eq_class_sz_1.AddNode(*n_child2.first);

	EqClass eq_class_sz_9;
	eq_class_sz_9.AddNode(*n_child1.first);
	eq_class_sz_9.AddNode(*n_child3.first);

	ASSERT_DOUBLE_EQ(NodeDistance(n1, n2), 0.1);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n1), 0.1);
}

TEST(NodeDistance, SomeOverlap) {
	Node n1(Node::DIR, "dsa");
	NodeAndClasses n_child1 = CreateNodeWithWeight(9);
	NodeAndClasses n_child2 = CreateNodeWithWeight(1);
	n1.AddChild(n_child1.first);
	n1.AddChild(n_child2.first);

	Node n2(Node::DIR, "asd");
	NodeAndClasses n_child3 = CreateNodeWithWeight(9);
	NodeAndClasses n_child4 = CreateNodeWithWeight(2);
	n2.AddChild(n_child3.first);
	n2.AddChild(n_child4.first);

	EqClass eq_class_sz_1;
	eq_class_sz_1.AddNode(*n_child2.first);

	EqClass eq_class_sz_9;
	eq_class_sz_9.AddNode(*n_child1.first);
	eq_class_sz_9.AddNode(*n_child3.first);

	EqClass eq_class_sz_2;
	eq_class_sz_2.AddNode(*n_child4.first);

	ASSERT_DOUBLE_EQ(NodeDistance(n1, n2), 0.25);
	ASSERT_DOUBLE_EQ(NodeDistance(n2, n1), 0.25);
}

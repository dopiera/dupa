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

#include <functional>
#include <memory>
#include <utility>

#include "gtest/gtest.h"
#include "test_common.h"

void PrintTo(const Node *n, std::ostream *os) {
  boost::filesystem::PrintTo(n->BuildPath(), os);
}

using EqClasses = std::vector<std::unique_ptr<EqClass>>;
using EqClassesPtr = std::shared_ptr<EqClasses>;
using NodeAndClasses = std::pair<Node *, EqClassesPtr>;
static NodeAndClasses CreateNodeWithWeight(int weight) {
  EqClassesPtr eq_classes(new EqClasses);
  Node *n = new Node(Node::DIR, "dsa");
  for (int i = 0; i < weight; ++i) {
    Node *n_child = new Node(Node::FILE, "xyz");
    n->AddChild(n_child);
    eq_classes->push_back(std::make_unique<EqClass>());
    eq_classes->back()->AddNode(*n_child);
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

  namespace fs = boost::filesystem;
  ASSERT_EQ(n.BuildPath(), fs::path("aaa"));
  ASSERT_EQ(child->BuildPath(), fs::path("aaa") / fs::path("bbb"));
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

  Nodes nodes;
  n1.Traverse([&nodes](Node *node) { nodes.push_back(node); });
  std::sort(nodes.begin(), nodes.end());
  ASSERT_EQ(nodes, expected);
}

TEST(NodeTest, AncestorTestIndependentFiles) {
  Node n1(Node::FILE, "n1");
  EqClass eq_class1;
  eq_class1.AddNode(n1);
  ASSERT_TRUE(n1.IsAncestorOf(n1));
  Node n2(Node::FILE, "n2");
  EqClass eq_class2;
  eq_class2.AddNode(n2);
  ASSERT_TRUE(n2.IsAncestorOf(n2));
  ASSERT_FALSE(n1.IsAncestorOf(n2));
  ASSERT_FALSE(n2.IsAncestorOf(n1));
}

TEST(NodeTest, AncestorTest1Level) {
  Node *n1 = new Node(Node::FILE, "n1");
  Node n2(Node::FILE, "n2");
  Node d1(Node::DIR, "d1");
  d1.AddChild(n1);

  EqClass dummy;
  dummy.AddNode(*n1);
  dummy.AddNode(n2);
  dummy.AddNode(d1);

  ASSERT_TRUE(d1.IsAncestorOf(*n1));
  ASSERT_FALSE(d1.IsAncestorOf(n2));
  ASSERT_FALSE(n1->IsAncestorOf(d1));
}

TEST(NodeTest, AncestorTest2Level) {
  Node *n1 = new Node(Node::FILE, "n1");
  Node n2(Node::FILE, "n2");
  Node *d1 = new Node(Node::DIR, "d1");
  Node d2(Node::DIR, "d2");
  d1->AddChild(n1);
  d2.AddChild(d1);

  EqClass dummy;
  dummy.AddNode(*n1);
  dummy.AddNode(n2);
  dummy.AddNode(*d1);
  dummy.AddNode(d2);

  ASSERT_TRUE(d1->IsAncestorOf(*n1));
  ASSERT_FALSE(d1->IsAncestorOf(n2));
  ASSERT_FALSE(n1->IsAncestorOf(*d1));
  ASSERT_TRUE(d2.IsAncestorOf(*d1));
  ASSERT_TRUE(d2.IsAncestorOf(*n1));
  ASSERT_FALSE(n1->IsAncestorOf(d2));
  ASSERT_FALSE(d1->IsAncestorOf(n2));
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

TEST(EqClassWeight, Empty) { ASSERT_DOUBLE_EQ(EqClass().weight_, 0); }

TEST(EqClassWeight, SingleNode) {
  EqClass eq_class;
  Node n(Node::FILE, "abc");
  eq_class.AddNode(n);
  ASSERT_DOUBLE_EQ(eq_class.weight_, 1);
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
  ASSERT_DOUBLE_EQ(top_class.weight_, 2);
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
  ASSERT_DOUBLE_EQ(eq_class.weight_, 3);
  eq_class.AddNode(*n2.first);
  ASSERT_DOUBLE_EQ(eq_class.weight_, 5);
  ASSERT_DOUBLE_EQ(n1.first->GetWeight(), 5);
  ASSERT_DOUBLE_EQ(n2.first->GetWeight(), 5);
  ASSERT_DOUBLE_EQ(n3.first->GetWeight(), 11);
  eq_class.AddNode(*n3.first);
  ASSERT_DOUBLE_EQ(eq_class.weight_, 7);
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

#include "file_tree.h"

#include <cassert>

void Node::AddChild(Node *child) {
	assert(child->parent == NULL);
	child->parent = this;
	this->children.push_back(child);
	++this->not_evaluated_children;
}

Node::~Node() {
	assert(IsEvaluated()); // we created it for nothing?
	for (
		Nodes::const_iterator it = this->children.begin();
	 	it != this->children.end();
		++it) {
		delete *it;
	}
}

void Node::SetEquivalenceCl(EqClass * eq_class) {
	assert(IsReadyToEvaluate());
	assert(!IsEvaluated());
	this->eq_class = eq_class;
}

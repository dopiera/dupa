#include "file_tree.h"

#include <stdint.h>

#include <cassert>

#include <tr1/unordered_map>
#include <tr1/unordered_set>

void Node::AddChild(Node *child) {
	assert(!IsEvaluated());
	assert(child->parent == NULL);
	assert(!child->IsEvaluated()); // This is due to not_evaluated_children
	assert(type == DIR);
	child->parent = this;
	this->children.push_back(child);
	++this->not_evaluated_children;
}

Node::~Node() {
	for (
		Nodes::const_iterator it = this->children.begin();
	 	it != this->children.end();
		++it) {
		delete *it;
	}
}

void Node::SetEqClass(EqClass * eq_class) {
	assert(IsReadyToEvaluate());
	assert(!IsEvaluated());
	this->eq_class = eq_class;
	if (this->parent) {
		assert(this->parent->not_evaluated_children);
		--this->parent->not_evaluated_children;
	}
}

double Node::GetWeight() const {
	if (IsEvaluated()) {
		return eq_class->weight;
	}

	switch (type) {
		case FILE:
			return 1;
		case DIR:
			{
				std::tr1::unordered_set<EqClass*> eq_classes;
				double weight = 0;
				for (
					Nodes::const_iterator it = children.begin();
					it != children.end();
					++it)
				{
					Node const * const n = *it;
					assert(n->IsEvaluated());
					eq_classes.insert(&n->GetEqClass());
				}
				for (
					std::tr1::unordered_set<EqClass*>::const_iterator it =
						eq_classes.begin();
					it != eq_classes.end();
					++it)
				{
					weight += (*it)->weight;
				}
				return weight;
			}
		case OTHER:
			assert(false); // I don't even know how to implement it
			return 0;
	}
	assert(false); // We shouldn't reach it;
	return 0;
}

boost::filesystem::path Node::BuildPath() const {
	if (parent) {
		return parent->BuildPath() /= boost::filesystem::path(name);
	} else {
		return boost::filesystem::path(name);
	}
}

std::vector<Node const*> Node::GetPossibleEquivalents() const {
	assert(this->IsReadyToEvaluate());
	std::tr1::unordered_set<Node const*> nodes;
	for (
			Nodes::const_iterator child_it = this->children.begin();
			child_it != this->children.end();
			++child_it)
	{
		Node const & child = **child_it;
		assert(child.IsEvaluated());
		for (
				Nodes::const_iterator equivalent_it =
					child.eq_class->nodes.begin();
				equivalent_it != child.eq_class->nodes.end();
				++equivalent_it)
		{
			Node const & equivalent = **equivalent_it;
			assert(equivalent.IsEvaluated());
			if (&equivalent != &child && equivalent.parent &&
					equivalent.parent->IsEvaluated() &&
					equivalent.parent != this) {
				nodes.insert(equivalent.parent);
			}
		}
	}
	return std::vector<Node const*>(nodes.begin(), nodes.end());
}

void Node::Traverse(boost::function<void(Node*)> callback) {
	for (
			Nodes::const_iterator child_it = this->children.begin();
			child_it != this->children.end();
			++child_it)
	{
		(*child_it)->Traverse(callback);
	}
	callback(this);
}

double NodeDistance(Node const &n1, Node const &n2) {
	assert(n1.IsReadyToEvaluate());
	assert(n2.IsReadyToEvaluate());
	// Not supported. All those comparisons should be done from outside using a
	// hash table for efficiency.
	assert(n1.type != Node::FILE || n2.type != Node::FILE);

	std::tr1::unordered_map<EqClass *, bool> eq_classes1;
	std::tr1::unordered_set<EqClass *> eq_classes_only_2;

	for (
		Nodes::const_iterator it = n1.children.begin();
		it != n1.children.end();
		++it)
	{
		Node const * const n = *it;
		eq_classes1.insert(std::make_pair(n->eq_class, false));
	}
	for (
		Nodes::const_iterator it = n2.children.begin();
		it != n2.children.end();
		++it)
	{
		Node const * const n = *it;
		std::tr1::unordered_map<EqClass *, bool>::iterator in1_it =
			eq_classes1.find(n->eq_class);
		if (in1_it == eq_classes1.end()) {
			eq_classes_only_2.insert(n->eq_class);
		}
		else
		{
			in1_it->second = true;
		}
	}

	uint64_t sum = 0;
	uint64_t sym_diff = 0;

	for (
		std::tr1::unordered_map<EqClass *, bool>::const_iterator it =
			eq_classes1.begin();
		it != eq_classes1.end();
		++it)
	{
		sum += it->first->weight;
		if (not it->second) {
			// only in n1
			sym_diff += it->first->weight;
		}
	}
	for (
		std::tr1::unordered_set<EqClass *>::const_iterator it =
			eq_classes_only_2.begin();
		it != eq_classes_only_2.end();
		++it)
	{
		sum += (*it)->weight;
		sym_diff += (*it)->weight;
	}
	if (sum == 0) {
		// both are empty directories, so they are the same
		return 0;
	}
	assert(sum >= sym_diff);
	return double(sym_diff) / sum;
}

void EqClass::AddNode(Node &node) {
	assert(find(nodes.begin(), nodes.end(), &node) == nodes.end());
	nodes.push_back(&node);
	weight = (weight * (nodes.size() - 1) + node.GetWeight()) / nodes.size();
	node.SetEqClass(this);
}

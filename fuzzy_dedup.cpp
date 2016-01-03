#include "fuzzy_dedup.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <thread>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>

#include "hash_cache.h"
#include "synch_thread_pool.h"

FuzzyDedupRes fuzzy_dedup(boost::filesystem::path const & dir)
{
	// Scan the directory and compute checksums for regular files
	std::pair<Node*, detail::Sum2Node> const root_and_sum_2_node =
		detail::ScanDirectory(dir);
	std::shared_ptr<Node> root_node(root_and_sum_2_node.first);
	detail::Sum2Node const &sum_2_node = root_and_sum_2_node.second;

	// Create equivalence classes for all regular files
	EqClassesPtr eq_classes =
		detail::ClassifyDuplicateFiles(*root_node, sum_2_node);

	// Create equivalence classes for all empty directories
	{
		std::unique_ptr<EqClass> empty_dirs_class =
			detail::ClassifyEmptyDirs(*root_node);
		if (!empty_dirs_class->IsEmpty()) {
			eq_classes->push_back(empty_dirs_class.release());
		}
	}

	// Now we have all leaves covered, let's propagate that all the way up to
	// the root.
	detail::PropagateEquivalence(*root_node, eq_classes);

	// Let's sort the equivalence classes such that the most important ones are
	// in the front.
	detail::SortEqClasses(eq_classes);

	return std::make_pair(root_node, eq_classes);
}

std::vector<EqClass*> GetInteresingEqClasses(FuzzyDedupRes &all) {
	std::vector<EqClass*> res;
	for (EqClass &eq_class : *all.second) {
		assert(eq_class.nodes.size() > 0);
		if (eq_class.nodes.size() == 1) {
			continue;
		}
		bool all_parents_are_dups = true;
		for (Node *node : eq_class.nodes) {
			if (node->GetParent() == NULL ||
					node->GetParent()->GetEqClass().IsSingle()) {
				all_parents_are_dups = false;
			}
		}
		if (all_parents_are_dups)
			continue;
		res.push_back(&eq_class);
	}
	return res;
}

namespace detail {

//======== ScanDirectory =======================================================

std::pair<Node*, Sum2Node> ScanDirectory(boost::filesystem::path const & dir) {
	std::pair<Node*, Sum2Node> res;

	std::stack<Node*> dirs_to_process;

	res.first = new Node(Node::DIR, dir.native());
	std::unique_ptr<Node> res_first_auto_deleter(res.first); // exception safety;
	dirs_to_process.push(res.first);

	SyncThreadPool pool(4);  // FIXME: make configurable
	std::mutex mutex;

	while (!dirs_to_process.empty()) {
		Node * const this_dir = dirs_to_process.top();
		dirs_to_process.pop();
		boost::filesystem::path const this_path = this_dir->BuildPath();

		using boost::filesystem::directory_iterator;
		for (directory_iterator it(this_path); it != directory_iterator(); ++it)
		{
			if (is_symlink(it->path()))
			{
				continue;
			}
			if (is_directory(it->status()))
			{
				Node * const new_dir = new Node(Node::DIR,
						it->path().filename().native());
				dirs_to_process.push(new_dir);
				std::lock_guard<std::mutex> lock(mutex);
				this_dir->AddChild(new_dir);
			}
			if (is_regular(it->path()))
			{
				boost::filesystem::path path = it->path();
				pool.Submit([path, this_dir, &mutex, &res] () mutable {
					cksum const sum = hash_cache::get()(path);
					if (sum) {
						Node * const file = new Node(
								Node::FILE,
								path.filename().native());
						std::lock_guard<std::mutex> lock(mutex);
						this_dir->AddChild(file);
						res.second.insert(std::make_pair(sum, file));
					}});
			}
		}
	}
	pool.Stop();
	res_first_auto_deleter.release();
	return res;
}

//======== ClassifyEmptyDirs ===================================================

namespace {

struct EmptyDirsEqClassFill {

	EmptyDirsEqClassFill() : eq_class(new EqClass) {
	}

	void OnNode(Node *node) {
		if (node->IsEmptyDir()) {
			eq_class->AddNode(*node);
		}
	}
	std::unique_ptr<EqClass> eq_class;
};

} /* anonymous namespace */

std::unique_ptr<EqClass> ClassifyEmptyDirs(Node &node) {
	EmptyDirsEqClassFill empty_dir_classifier;
	node.Traverse(std::bind(&EmptyDirsEqClassFill::OnNode,
				std::ref(empty_dir_classifier), std::placeholders::_1));
	return std::move(empty_dir_classifier.eq_class);
}

//======== ClassifyDuplicateFiles ==============================================

EqClassesPtr ClassifyDuplicateFiles(Node &node, Sum2Node const &sum_2_node) {
	EqClassesPtr res(new EqClasses);
	for (
			Sum2Node::const_iterator range_start = sum_2_node.begin();
			range_start != sum_2_node.end();
			)
	{
		Sum2Node::const_iterator const range_end =
			sum_2_node.equal_range(range_start->first).second;
		// I am traversing the map twice, but it's so much more readable...

		if (range_start != range_end) {
			EqClass * eq_class = new EqClass;
			res->push_back(eq_class);
			for (; range_start != range_end; ++range_start) {
				eq_class->AddNode(*range_start->second);
			}
		}
	}
	return res;
}

//======== GetNodesReadyToEval =================================================

namespace {

struct GatherReadyToEval {
	void OnNode(Node *node) {
		if (node->IsReadyToEvaluate() && !node->IsEvaluated()) {
			ready_to_evaluate.push(node);
		}
	}
	std::queue<Node*> ready_to_evaluate;
};

} /* anonymous namespace */

std::queue<Node*> GetNodesReadyToEval(Node &node)
{
	GatherReadyToEval gathered;
	node.Traverse(std::bind(&GatherReadyToEval::OnNode,
				std::ref(gathered), std::placeholders::_1));
	return gathered.ready_to_evaluate;
}

//======== GetClosestNode ======================================================

std::pair<Node*, double> GetClosestNode(Node const &ref,
		Nodes const &candidates) {
	double min_dist = 2;
	Node *min_elem = NULL;
	for (Node * const candidate : candidates) {
		assert(candidate != &ref);
		assert(candidate->IsEvaluated());
		double const distance = NodeDistance(ref, *candidate);
		assert(distance < 1.1);  // actually <= 1, but it's double
		if (distance < min_dist) {
			min_elem = candidate;
			min_dist = distance;
		}
	}
	return std::make_pair(min_elem, min_dist);
}

//======== PropagateEquivalence ================================================

void PropagateEquivalence(Node &root_node, EqClassesPtr eq_classes) {
	std::queue<Node*> ready_to_eval = detail::GetNodesReadyToEval(root_node);

	while (!ready_to_eval.empty()) {
		Node *node = ready_to_eval.front();
		ready_to_eval.pop();

		assert(node->IsReadyToEvaluate());
		assert(!node->IsEvaluated());
		assert(node->GetParent() == NULL ||
				!node->GetParent()->IsReadyToEvaluate());

		Nodes possible_equivalents =
			node->GetPossibleEquivalents();

		std::pair<Node*, double> min_elem_and_dist =
			detail::GetClosestNode(*node, possible_equivalents);

		// FIXME make configurable
		if (min_elem_and_dist.second < 0.3) {
			min_elem_and_dist.first->GetEqClass().AddNode(*node);
		} else {
			EqClass *eq_class = new EqClass;
			eq_classes->push_back(eq_class);
			eq_class->AddNode(*node);
		}

		// By modifying this node we could have made the parent ready to
		// evaluate. It is clear it couldn't have been ready at the beginning of
		// this function because this element was not evaluated, so there is no
		// risk we're adding it to the queue twice.
		if (node->GetParent() != NULL &&
			   node->GetParent()->IsReadyToEvaluate()) {
			ready_to_eval.push(node->GetParent());
		}
	}
	assert(root_node.IsEvaluated());
}

//======== SortEqClasses =======================================================

namespace {

struct EqClassWeightCmp : public std::binary_function<EqClass,EqClass,bool> {
	bool operator()(const EqClass &a, const EqClass &b) const
	{
		return a.weight > b.weight;
	}
};

} /* anonymous namespace */

void SortEqClasses(EqClassesPtr eq_classes) {
	eq_classes->sort(EqClassWeightCmp());
}

} /* namespace detail */

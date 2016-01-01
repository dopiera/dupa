#ifndef FUZZY_DEDUP_H_7234
#define FUZZY_DEDUP_H_7234

#include <memory>
#include <queue>
#include <utility>

#include <unordered_map>

#include <boost/ptr_container/ptr_vector.hpp>

#include "file_tree.h"
#include "hash_cache.h" // for cksum

typedef boost::ptr_vector<EqClass> EqClasses;
typedef std::shared_ptr<EqClasses> EqClassesPtr;
typedef std::pair<std::shared_ptr<Node>, EqClassesPtr> FuzzyDedupRes;

FuzzyDedupRes fuzzy_dedup(boost::filesystem::path const & start_dir);

// This shouldn't be public but is for testing.
namespace detail {

typedef std::unordered_multimap<cksum, Node*> Sum2Node;

// Recursively scan directory dir. Return the directory's hierarchy and a
// multimap from checksums to Nodes in the hierarchy for all regular files.
std::pair<Node*, Sum2Node> ScanDirectory(boost::filesystem::path const & dir);

// Create an equivalence class and assign all empty directories to it.
std::unique_ptr<EqClass> ClassifyEmptyDirs(Node &node);

// Create equivalence class for every hash and assign FILE nodes to them
// accordingly.
EqClassesPtr ClassifyDuplicateFiles(Node &node, Sum2Node const &um_2_node);

// Get all child nodes (possibly includeing the argument) for which
// IsReadyToEvaluate() && !IsEvaluated()
std::queue<Node*> GetNodesReadyToEval(Node &node);

// Out of the list of "candidates" return the closest node to "ref" and the
// distance between it and "ref". Iff "candidates" is empty, (NULL, 2) is
// returned.
std::pair<Node*, double> GetClosestNode(Node const &ref,
		Nodes const &candidates);

// Assuming that all regular files and empty directories in the hierarchy
// described by root_node are evaluated and nothing else, evaluate everything
// else. Newly created equivalence clesses will be appended to eq_classes.
void PropagateEquivalence(Node &root_node, EqClassesPtr eq_classes);

// Sort equivalence classes according to weight. Largest first.
void SortEqClasses(EqClassesPtr eq_classes);

} /* namespace detail */

#endif /* FUZZY_DEDUP_H_7234 */

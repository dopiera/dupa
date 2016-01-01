#include <fstream>
#include <string>
#include <thread>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/program_options.hpp>

#include "log.h"
#include "file_tree.h"
#include "fuzzy_dedup.h"
#include "hash_cache.h"

using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::multi_index;
using namespace boost::program_options;

BOOST_STATIC_ASSERT(sizeof(off_t) == 8);

struct path_hash
{
	path_hash(string const & path, cksum hash) :
		path(path),
		hash(hash)
	{
	}

	string path;
	cksum hash;
};

struct by_path {};
struct by_hash {};
typedef multi_index_container<
  path_hash,
  indexed_by<
    ordered_unique<tag<by_path>, member<path_hash, std::string, &path_hash::path> >,
    ordered_non_unique<tag<by_hash>, member<path_hash, cksum, &path_hash::hash> >
  > 
> path_hashes;
typedef path_hashes::index<by_path>::type path_hashes_by_path;
typedef path_hashes::index<by_hash>::type path_hashes_by_hash;

struct NodePathOrder : public std::binary_function<Node*, Node*, bool> {
	bool operator()(Node* n1, Node* n2) const {
		return n1->BuildPath().native() < n2->BuildPath().native();
	}
};

void print_fuzzy_dups(FuzzyDedupRes const &res) {
	for (EqClass const &eq_class : *res.second) {
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
		Nodes to_print(eq_class.nodes);
		std::sort(to_print.begin(), to_print.end(), NodePathOrder());
		for (
				Nodes::const_iterator node_it = to_print.begin();
				node_it != to_print.end();
				++node_it) {
			cout << (*node_it)->BuildPath().native();
			if (--to_print.end() != node_it) {
				cout << " ";
			}
		}
		cout << endl;
	}
}

void fill_path_hashes(path const & dir, path_hashes & hashes, string const & dir_string)
{
	for (directory_iterator it(dir); it != directory_iterator(); ++it)
	{
		if (is_symlink(it->path()))
		{
			continue;
		}
		if (is_directory(it->status()))
		{
			fill_path_hashes(it->path(), hashes, dir_string + it->path().filename().native() + "/");
		}
		if (is_regular(it->path()))
		{
			cksum const sum = hash_cache::get()(it->path());
			if (sum)
				hashes.insert(path_hash(dir_string + it->path().filename().native(), sum));
		}
	}
}

typedef vector<string> paths;

template <class S>
S & operator<<(S & stream, paths const & p)
{
	stream << "[";
	for (paths::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		if (it != p.begin())
			stream << " ";
		stream << *it;
	}
	stream << "]";
	return stream;
}

paths get_paths_for_hash(path_hashes_by_hash & ps, cksum hash)
{
	paths res;
	for (auto r = ps.equal_range(hash); r.first != r.second; ++r.first) {
		res.push_back(r.first->path);
	}
	return res;
}

void dir_compare(path const & dir1, path const & dir2)
{
	path_hashes hashes1, hashes2;

	std::thread h1filler(std::bind(
				fill_path_hashes, std::ref(dir1), std::ref(hashes1), ""));
	std::thread h2filler(
			std::bind(fill_path_hashes, std::ref(dir2), std::ref(hashes2), ""));
	h1filler.join();
	h2filler.join();

	path_hashes_by_path & hashes1p(hashes1.get<by_path>());
	path_hashes_by_path & hashes2p(hashes2.get<by_path>());
	path_hashes_by_hash & hashes1h(hashes1.get<by_hash>());
	path_hashes_by_hash & hashes2h(hashes2.get<by_hash>());

	for (auto const &path_and_hash : hashes1p)
	{
		string const & p1 = path_and_hash.path;
		cksum h1 = path_and_hash.hash;
		path_hashes_by_path::const_iterator const same_path = hashes2p.find(p1);
		if (same_path != hashes2p.end())
		{
			//this path exists in second dir
			cksum const h2 = same_path->hash;
			if (h1 == h2)
			{
				//cout << "NOT_CHANGED: " << p1 << endl;
			}
			else
			{
				paths ps = get_paths_for_hash(hashes1h, h2);
				if (not ps.empty())
				{
					//rename from somewhere:
					cout << "OVERWRITTEN_BY: " << p1 << " CANDIDATES: " << ps << endl;
				}
				else
				{
					cout << "CONTENT_CHANGED: " << p1 << endl;
				}
			}
		}
		else
		{
			paths ps = get_paths_for_hash(hashes2h, h1);
			if (not ps.empty())
			{
				cout << "RENAME: " << p1 << " -> " << ps << endl;
			}
			else
			{
				cout << "REMOVED: " << p1 << endl;
			}
		}
	}
	for (auto const &path_and_hash : hashes2p)
	{
		string const & p2 = path_and_hash.path;
		cksum h2 = path_and_hash.hash;
		if (hashes1p.find(p2) != hashes1p.end())
		{
			//path exists in both, so it has already been handled by the first
			//loop
			continue;
		}
		else
		{
			paths ps = get_paths_for_hash(hashes1h, h2);
			if (not ps.empty())
			{
				paths ps2;
				for (auto const & copy_candidate : ps)
				{
					if (hashes2p.find(copy_candidate) != hashes2p.end())
					{
						ps2.push_back(copy_candidate);
					}
					//otherwise it's probably renamed from that file, so it's
					//already mentioned
				}
				if (not ps2.empty())
				{
					cout << "COPIED_FROM: " << p2 << " CANDIDATES: " << ps2<< endl;
				}
			}
			else
			{
				cout << "NEW_FILE: " << p2 << endl;
			}
		}
	}
}

static void print_compilation_profile_warning() {
	LogLevel ll = stderr_loglevel;
	stderr_loglevel = DEBUG;
	DLOG("This is a debug build, performance might suck.");
	stderr_loglevel = ll;
}

int main(int argc, char **argv)
{
	print_compilation_profile_warning();
	string read_cache_from, dump_cache_to;
	vector<string> dirs;

	variables_map vm;
	options_description hidden_desc("Hidden options");
	hidden_desc.add_options()
		("directory,d", value<vector<string> >(&dirs)->composing(),
		 "directory to analyze");
	options_description desc("usage: dup_ident dir1 [dir2]");
	desc.add_options()
		("help,h", "produce help message")
		("read_cache_from,c", value<string>(&read_cache_from),
		 "path to the file from which to read checksum cache")
		("dump_cache_to,C", value<string>(&dump_cache_to),
		 "path to which to dump the checksum cache")
		("cache_only,1", "only generate checksums cache")
		("verbose,v", "be verbose");

	try {
		options_description effective_desc;
		effective_desc.add(hidden_desc).add(desc);
		positional_options_description p;
		p.add("directory", 2);
		store(command_line_parser(argc, argv).options(effective_desc)
				.positional(p).run(), vm);
		notify(vm);    
	} catch (program_options::error const &e) {
		cout << e.what() << endl;
		cout << desc << endl;
		return 1;
	}

	if (vm.count("verbose")) {
		stderr_loglevel = DEBUG;
	}

	if (vm.count("help"))
	{
		cout << desc << endl;
		return 0;
	}

	try {
		hash_cache::initializer hash_cache_init(read_cache_from, dump_cache_to);

		if (vm.count("cache_only")) {
			for (auto const &dir : dirs) {
				path_hashes hashes;
				fill_path_hashes(dir, hashes, "");
			}
			return 0;
		}
		if (dirs.size() == 1)
		{
			FuzzyDedupRes res = fuzzy_dedup(dirs[0]);
			print_fuzzy_dups(res);
		}
		if (dirs.size() == 2)
		{
			dir_compare(dirs[0], dirs[1]);
			return 0;
		}
		else
		{
			if (dirs.empty())
			{
				cout << desc << endl;
				return 1;
			}
		}
	}
	catch (ios_base::failure const & ex)
	{
		cerr << "Failure: " << ex.what() << endl;
		throw;
	}

	return 0;
}

#include <fstream>
#include <string>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
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

typedef multimap<cksum, string> cksum_map;

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

void dup_detect(path const & dir, cksum_map & cksums)
{
	for (directory_iterator it(dir); it != directory_iterator(); ++it)
	{
		if (is_symlink(it->path()))
		{
			continue;
		}
		if (is_directory(it->status()))
		{
			dup_detect(it->path(), cksums);
		}
		if (is_regular(it->path()))
		{
			cksum const sum = hash_cache::get()(it->path());
			if (sum)
				cksums.insert(make_pair(sum, it->path().native()));
		}
	}
}

void print_dups(cksum_map & cksums)
{
	cksum_map::const_iterator range_start;
	for (cksum_map::const_iterator it = cksums.begin(); it != cksums.end(); ++it)
	{
		if (it == cksums.begin())
		{
			range_start = it;
			continue;
		}
		if (it->first != range_start->first)
		{
			//next hash
			cksum_map::const_iterator tmp = range_start;
			++tmp;

			if (tmp != it)
			{
				//more than one file, we've got duplicates
				vector<string> dup_paths;
				for (; range_start != it; ++range_start)
				{
					dup_paths.push_back(range_start->second);
				}
				sort(dup_paths.begin(), dup_paths.end());
				for (vector<string>::const_iterator d = dup_paths.begin(); d != dup_paths.end(); ++d)
				{
					cout << *d << "\t";
				}
				cout << endl;
			}
			range_start = it;
		}
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
	for (
		path_hashes_by_hash::const_iterator it = ps.lower_bound(hash);
		it->hash == hash;
		++it
		)
	{
		res.push_back(it->path);
	}
	return res;
}

void dir_compare(path const & dir1, path const & dir2)
{
	path_hashes hashes1, hashes2;

	boost::thread h1filler(boost::bind(fill_path_hashes, ref(dir1), ref(hashes1), ""));
	boost::thread h2filler(boost::bind(fill_path_hashes, ref(dir2), ref(hashes2), ""));
	h1filler.join();
	h2filler.join();

	path_hashes_by_path & hashes1p(hashes1.get<by_path>());
	path_hashes_by_path & hashes2p(hashes2.get<by_path>());
	path_hashes_by_hash & hashes1h(hashes1.get<by_hash>());
	path_hashes_by_hash & hashes2h(hashes2.get<by_hash>());

	for (
		path_hashes_by_path::const_iterator it = hashes1p.begin();
		it != hashes1p.end();
		++it
		)
	{
		string const & p1 = it->path;
		cksum h1 = it->hash;
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
	for (
		path_hashes_by_path::const_iterator it = hashes2p.begin();
		it != hashes2p.end();
		++it
		)
	{
		string const & p2 = it->path;
		cksum h2 = it->hash;
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
				for (
					paths::const_iterator copy_candidate = ps.begin();
					copy_candidate != ps.end();
					++copy_candidate
					)
				{
					if (hashes2p.find(*copy_candidate) != hashes2p.end())
					{
						ps2.push_back(*copy_candidate);
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

int main(int argc, char **argv)
{
	DLOG("This is a debug build, performance might suck.");
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
		 "path to which to dump the checksum cache");

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

	if (vm.count("help"))
	{
		cout << desc << endl;
		return 0;
	}

	try {
		hash_cache::initializer hash_cache_init(read_cache_from, dump_cache_to);

		if (dirs.size() == 1)
		{
			//cksum_map cksums;
			//dup_detect(dirs[0], cksums);
			//print_dups(cksums);
			FuzzyDedupRes res = fuzzy_dedup(dirs[0]);
			for (
					EqClasses::const_iterator it = res.second->begin();
					it != res.second->end();
					++it) {
				assert(it->nodes.size() > 0);
				if (it->nodes.size() == 1) {
					continue;
				}
				for (
						Nodes::const_iterator it2 = it->nodes.begin();
						it2 != it->nodes.end();
						++it2) {
					cout << (*it2)->BuildPath().native() << " ";
				}
				cout << endl;
			}
			return 0;
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

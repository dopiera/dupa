#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <openssl/sha.h>

#include <exception>
#include <string>
#include <map>

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::multi_index;

typedef uint64_t cksum;

BOOST_STATIC_ASSERT(sizeof(off_t) == 8);

int files_read;

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


struct fs_exception : std::exception
{
	fs_exception(int err, string const operation) :
		msg(fs_exception::msg_from_errno(err, operation))
	{
	}

	~fs_exception() throw()
	{
	}


    virtual char const * what() const throw()
	{
		return msg.c_str();
	}

private:
	static string msg_from_errno(int err, string const & operation)
	{
		size_t const buf_len = 128;
		char buf[buf_len];
		char const * const msg = strerror_r(err, buf, buf_len);
		return operation + ": " + lexical_cast<string>(err) + " (" + msg + ")";
	}

	string msg;
};


static cksum sha1(path const & p)
{
	string const native = p.native();
	int fd = open(native.c_str(), O_RDONLY);
	if (fd == -1)
		throw fs_exception(errno, "open '" + native + "'");

	size_t const buf_size = 131072;
	char buf[buf_size];

	union {
		u_char complete[20];
		cksum prefix;
	} sha_res;

	SHA_CTX sha;
	SHA_Init(&sha);
	while (true)
	{
		ssize_t res = read(fd, buf, buf_size);
		if (res < 0)
			throw fs_exception(errno, "read '" + native + "'");
		if (res == 0)
			break;
		SHA_Update(&sha, (u_char *)buf, res);
	}
	SHA_Final(sha_res.complete, &sha);

	fd = close(fd);
	if (fd < 0)
		throw fs_exception(errno, "close '" + native + "'");
	if (++files_read % 1000 == 0)
	{
		cerr << files_read << endl;
	}
	return sha_res.prefix;
}


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
			cksum const sum = sha1(it->path());
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
				for (; range_start != it; ++range_start)
				{
					cout << range_start->second << "\t";
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
			cksum const sum = sha1(it->path());
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
	if (argc == 2)
	{
		cksum_map cksums;
		dup_detect(argv[1], cksums);
		print_dups(cksums);
		return 0;
	}
	if (argc == 3)
	{
		dir_compare(argv[1], argv[2]);
		return 0;
	}
	else
	{
		cerr << "usage: " << argv[0] << " <dir>" << endl;
		cerr << " or" << endl;
		cerr << "usage: " << argv[0] << " <dir1> <dir2>" << endl;
		return 1;
	}

	return 0;
}

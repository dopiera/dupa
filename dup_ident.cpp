#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <openssl/sha.h>

#include <exception>
#include <fstream>
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
#include <boost/program_options.hpp>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "dup_ident.pb.h"

using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::multi_index;
using namespace boost::program_options;

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

struct proto_exception : std::exception
{
	proto_exception(string const & reason) :
		reason(reason)
	{
	}

	~proto_exception() throw()
	{
	}
	
	virtual char const * what() const throw()
	{
		return this->reason.c_str();
	}
private:
	string reason;
};

class hash_cache
{
public:
	struct initializer {
		initializer(
			string const & read_cache_from,
			string const & dump_cache_to
			);
		~initializer();
	};
	static hash_cache & get();
	cksum operator()(path const & p);
private:
	hash_cache(
		string const & read_cache_from,
		string const & dump_cache_to
		);
	~hash_cache();
	cksum compute_cksum(path const & p);
	void store_cksums();
	void read_cksums(string const & path);
	static void initialize(
		string const & read_cache_from,
		string const & dump_cache_to);
	static void finalize();

	static hash_cache * instance;

	typedef multimap<string, cksum> cache_map;
	cache_map cache;
	int out_fd;
};
hash_cache * hash_cache::instance;

hash_cache::initializer::initializer(
	string const & read_cache_from,
	string const & dump_cache_to
	)
{
	hash_cache::initialize(read_cache_from, dump_cache_to);
}

hash_cache::initializer::~initializer()
{
	hash_cache::finalize();
}

hash_cache::hash_cache(
	string const & read_cache_from,
	string const & dump_cache_to
	) :
	out_fd(-1)
{
	if (!read_cache_from.empty())
	{
		this->read_cksums(read_cache_from.c_str());
	}
	if (!dump_cache_to.empty())
	{
		this->out_fd = open(
			dump_cache_to.c_str(),
			O_WRONLY | O_EXCL | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
			);
		if (this->out_fd == -1)
		{
			throw fs_exception(errno, "open '" + dump_cache_to + "'");
		}
	}
}

hash_cache::~hash_cache()
{
	this->store_cksums();
}

void hash_cache::store_cksums()
{
	if (this->out_fd < 0)
	{
		return;
	}
	Paths paths;
	for (cache_map::const_iterator cksum_it = this->cache.begin();
			cksum_it != this->cache.end();
			++cksum_it)
	{
		Path &p = *paths.add_paths();
		p.set_path(cksum_it->first);
		p.set_cksum(cksum_it->second);
	}
	if (!paths.SerializeToFileDescriptor(this->out_fd))
	{
		throw proto_exception("Failed to serialize cache; TODO: add more logging");
	}
	this->out_fd = close(this->out_fd);
	if (this->out_fd < 0)
	{
		throw fs_exception(errno, "close dumped cache file");
	}
	
}

void hash_cache::read_cksums(string const & path)
{
	int fd;
	fd = open(path.c_str(), O_RDONLY);
	if (fd == -1)
		throw fs_exception(errno, "open '" + path + "'");

	Paths paths;
	google::protobuf::io::FileInputStream file_input(fd);
	google::protobuf::io::CodedInputStream coded_input(&file_input);
	coded_input.SetTotalBytesLimit(512 * 1024 * 1024, 512 * 1024 * 1024);
	if (!paths.ParseFromCodedStream(&coded_input) ||
		file_input.GetErrno() != 0)
	{
		throw proto_exception("parsing failed; TODO: reasonable message here");
	}
	this->cache.clear();
	for (int i = 0; i < paths.paths_size(); ++i)
	{
		Path const & p = paths.paths(i);
		this->cache.insert(make_pair(p.path(), p.cksum()));
	}
	fd = close(fd);
	if (fd < 0)
	{
		throw fs_exception(errno, "close '" + path + "'");
	}
}

cksum hash_cache::operator()(path const & p)
{
	cache_map::const_iterator it = this->cache.find(p.native());
	if (it != this->cache.end())
	{
		return it->second;
	}
	else
	{
		cksum res = this->compute_cksum(p);
		this->cache.insert(make_pair(p.native(), res));
		return res;
	}
}

cksum hash_cache::compute_cksum(path const & p)
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
	size_t size = 0;
	while (true)
	{
		ssize_t res = read(fd, buf, buf_size);
		if (res < 0)
			throw fs_exception(errno, "read '" + native + "'");
		if (res == 0)
			break;
		size += res;
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
	if (size)
		return sha_res.prefix;
	else
		return 0;
}

void hash_cache::initialize(
	string const & read_cache_from,
	string const & dump_cache_to)
{
	assert(!instance);
	hash_cache::instance = new hash_cache(read_cache_from, dump_cache_to);
}

void hash_cache::finalize()
{
	assert(instance);
	delete hash_cache::instance;
	hash_cache::instance = NULL;
}

hash_cache & hash_cache::get()
{
	assert(hash_cache::instance);
	return *hash_cache::instance;
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
		store(command_line_parser(argc, argv).options(effective_desc).positional(p).run(), vm);
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
			cksum_map cksums;
			dup_detect(dirs[0], cksums);
			print_dups(cksums);
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

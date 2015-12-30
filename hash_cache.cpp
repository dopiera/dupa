#include "hash_cache.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>

#include <openssl/sha.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "exceptions.h"
#include "dup_ident.pb.h"

hash_cache * hash_cache::instance;

hash_cache::initializer::initializer(
	std::string const & read_cache_from,
	std::string const & dump_cache_to
	)
{
	hash_cache::initialize(read_cache_from, dump_cache_to);
}

hash_cache::initializer::~initializer()
{
	hash_cache::finalize();
}

hash_cache::hash_cache(
	std::string const & read_cache_from,
	std::string const & dump_cache_to
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
		throw proto_exception(
				"Failed to serialize cache; TODO: add more logging");
	}
	this->out_fd = close(this->out_fd);
	if (this->out_fd < 0)
	{
		throw fs_exception(errno, "close dumped cache file");
	}
	
}

void hash_cache::read_cksums(std::string const & path)
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

cksum hash_cache::operator()(boost::filesystem::path const & p)
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
		if (this->cache.size() % 1000 == 0)
		{
			std::cerr << this->cache.size() << std::endl;
		}
		return res;
	}
}

cksum hash_cache::compute_cksum(boost::filesystem::path const & p)
{
	std::string const native = p.native();
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
	if (size)
		return sha_res.prefix;
	else
		return 0;
}

void hash_cache::initialize(
	std::string const & read_cache_from,
	std::string const & dump_cache_to)
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



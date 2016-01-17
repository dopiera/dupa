#include "hash_cache.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

#include <openssl/sha.h>

#include <boost/functional/hash/hash.hpp>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "exceptions.h"
#include "dup_ident.pb.h"
#include "log.h"
#include "sql_lib.h"

namespace {

// This is not stored because it is likely to have false positive matches when
// inodes are reused. It is also not populated on hash_cache deserialization
// because it's doubtful to bring much gain and is guaranteed to cost a lot if
// we stat all the files.
class inode_cache {
public:
	typedef std::pair<dev_t, ino_t> uuid;

	std::pair<bool, cksum> get(uuid ino) {
		std::lock_guard<std::mutex> lock(this->mutex);
		CacheMap::const_iterator it =
			this->cache_map.find(ino);
		if (it != this->cache_map.end()) {
			return std::make_pair(true, it->second);
		} else {
			return std::make_pair(false, 0);
		}
	}

	void update(uuid ino, cksum sum) {
		std::lock_guard<std::mutex> lock(this->mutex);
		this->cache_map.insert(std::make_pair(ino, sum));
	}

	static uuid get_inode(int fd, std::string const &path_for_errors) {
		struct stat st;
		int res = fstat(fd, &st);
		if (res != 0) {
			throw fs_exception(errno, "stat on '" + path_for_errors + "'");
		}
		if (!S_ISREG(st.st_mode)) {
			throw fs_exception(errno, "'" + path_for_errors +
					"' is not a regular file");
		}
		return std::make_pair(st.st_dev, st.st_ino);
	}

private:
	typedef std::unordered_map<uuid, cksum, boost::hash<uuid> > CacheMap;
	CacheMap cache_map;
	std::mutex mutex;
};

// I'm asking for trouble, but I'm lazy.
static inode_cache ino_cache;

} /* anonymous namespace */

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
		) {
	if (!read_cache_from.empty())
	{
		this->read_cksums(read_cache_from.c_str());
	}
	if (!dump_cache_to.empty())
	{
		this->db_holder.reset(new SqliteScopedOpener(dump_cache_to));
	}
}

hash_cache::~hash_cache()
{
	this->store_cksums();
}

static void create_or_empty_table(sqlite3 *db) {
	char const sql[] =
		"DROP TABLE IF EXISTS Cache;"
		"CREATE TABLE Cache("
			"path           TEXT    UNIQUE NOT NULL,"
			"cksum          INTEGER NOT NULL);";
	char *err_msg_raw;
	int res = sqlite3_exec(db, sql, NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, std::string("Creating results tables: " ) +
				err_msg.get());
	}
}

void hash_cache::store_cksums()
{
	std::lock_guard<std::mutex> lock(this->mutex);
	if (!this->db_holder)
	{
		return;
	}

	sqlite3 *db = this->db_holder->db;
	create_or_empty_table(db);
	char const sql[] =
		"INSERT INTO Cache(path, cksum) VALUES(?, ?)";
	StmtPtr stmt(PrepareStmt(db, sql));
	StartTransaction(db);
	for (auto const & cksum_and_path : this->cache)
	{
		SqliteBind(
				*stmt,
				cksum_and_path.first,
				cksum_and_path.second);
		int res = sqlite3_step(stmt.get());
		if (res != SQLITE_DONE)
			throw sqlite_exception(db, "Inserting EqClass");
		res = sqlite3_clear_bindings(stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
		res = sqlite3_reset(stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
	}
	EndTransaction(db);
}

void hash_cache::read_cksums(std::string const & path)
{
	std::lock_guard<std::mutex> lock(this->mutex);
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
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		cache_map::const_iterator it = this->cache.find(p.native());
		if (it != this->cache.end())
		{
			return it->second;
		}
	}
	cksum res = this->compute_cksum(p);

	std::lock_guard<std::mutex> lock(this->mutex);
	// If some other thread inserted a checksum for the same file in the
	// meantime, it's not a big deal.
	this->cache.insert(make_pair(p.native(), res));
	if (this->cache.size() % 1000 == 0)
	{
		LOG(INFO, this->cache.size());
	}
	return res;
}

namespace {

struct auto_fd_closer {
	auto_fd_closer(int fd) : fd(fd) {}
	~auto_fd_closer() {
		int res = close(fd);
		if (res < 0) {
			// This is unlikely to happen and if it happens it is likely to be
			// while already handling some other more important exception, so
			// let's not cover the original error.
			LOG(WARNING, "Failed to close descriptor " << fd);
		}
	}
private:
	int fd;
};

} /* anonymous namespace */

cksum hash_cache::compute_cksum(boost::filesystem::path const & p)
{
	std::string const native = p.native();
	int fd = open(native.c_str(), O_RDONLY);
	if (fd == -1)
		throw fs_exception(errno, "open '" + native + "'");
	auto_fd_closer closer(fd);

	inode_cache::uuid uuid = ino_cache.get_inode(fd, native);
	{
		std::pair<bool, cksum> sum = ino_cache.get(uuid);
		if (sum.first) {
			DLOG(native << " shares an inode with something already computed!");
			return sum.second;
		}
	}

	size_t const buf_size = 1024 * 1024;
	std::unique_ptr<char[]> buf(new char[buf_size]);

	union {
		u_char complete[20];
		cksum prefix;
	} sha_res;

	SHA_CTX sha;
	SHA_Init(&sha);
	size_t size = 0;
	while (true)
	{
		ssize_t res = read(fd, buf.get(), buf_size);
		if (res < 0)
			throw fs_exception(errno, "read '" + native + "'");
		if (res == 0)
			break;
		size += res;
		SHA_Update(&sha, (u_char *)buf.get(), res);
	}
	SHA_Final(sha_res.complete, &sha);

	if (size) {
		ino_cache.update(uuid, sha_res.prefix);
		return sha_res.prefix;
	} else {
		ino_cache.update(uuid, 0);
		return 0;
	}
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



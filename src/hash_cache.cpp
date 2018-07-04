#include "hash_cache.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

#include <openssl/sha.h>

#include <boost/functional/hash/hash.hpp>

#include "exceptions.h"
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
	struct stat_result {
		stat_result(uuid id, off_t size, time_t mtime):
			id(id), size(size), mtime(mtime)
		{}

		uuid id;
		off_t size;
		time_t mtime;
	};

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

	static stat_result get_inode_info(
			int fd, std::string const &path_for_errors) {
		struct stat st;
		int res = fstat(fd, &st);
		if (res != 0) {
			throw fs_exception(errno, "stat on '" + path_for_errors + "'");
		}
		if (!S_ISREG(st.st_mode)) {
			throw fs_exception(errno, "'" + path_for_errors +
					"' is not a regular file");
		}
		return stat_result(
				std::make_pair(st.st_dev, st.st_ino),
				st.st_size,
				st.st_mtime);
	}

private:
	typedef std::unordered_map<uuid, cksum, boost::hash<uuid> > CacheMap;
	CacheMap cache_map;
	std::mutex mutex;
};

// I'm asking for trouble, but I'm lazy.
static inode_cache ino_cache;

cksum compute_cksum(
		int fd, inode_cache::uuid uuid, std::string const & path_for_errors)
{
	{
		std::pair<bool, cksum> sum = ino_cache.get(uuid);
		if (sum.first) {
			DLOG(path_for_errors << " shares an inode with something already "
					"computed!");
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
	SHA1_Init(&sha);
	size_t size = 0;
	while (true)
	{
		ssize_t res = read(fd, buf.get(), buf_size);
		if (res < 0)
			throw fs_exception(errno, "read '" + path_for_errors + "'");
		if (res == 0)
			break;
		size += res;
		SHA1_Update(&sha, (u_char *)buf.get(), res);
	}
	SHA1_Final(sha_res.complete, &sha);

	if (size) {
		ino_cache.update(uuid, sha_res.prefix);
		return sha_res.prefix;
	} else {
		ino_cache.update(uuid, 0);
		return 0;
	}
}

} /* anonymous namespace */

std::unordered_map<std::string, file_info> read_cache_from_db(
		std::string const & path) {
	std::unordered_map<std::string, file_info> cache;
	SqliteConnection db(path, SQLITE_OPEN_READONLY);
	char const sql[] = "SELECT path, cksum, size, mtime FROM Cache";
	db.SqliteExec(sql, [&] (sqlite3_stmt &row) {
			std::string const path(reinterpret_cast<const char*>(
								sqlite3_column_text(&row, 0)));
			cksum const sum = sqlite3_column_int64(&row, 1);
			off_t const size = sqlite3_column_int64(&row, 2);
			time_t const mtime = sqlite3_column_int64(&row, 3);
			DLOG("Read \"" << path << "\": " << sum << " " << size << " " <<
					mtime);

			file_info f_info(size, mtime, sum);

			cache.insert(std::make_pair(path, f_info));
			});
	return cache;
}

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
		auto cache = read_cache_from_db(read_cache_from);
		std::lock_guard<std::mutex> lock(this->mutex);
		this->cache.swap(cache);
	}
	if (!dump_cache_to.empty())
	{
		this->db.reset(new SqliteConnection(dump_cache_to));
	}
}

hash_cache::~hash_cache()
{
	this->store_cksums();
}

static void create_or_empty_table(SqliteConnection &db) {
	db.SqliteExec(
		"DROP TABLE IF EXISTS Cache;"
		"CREATE TABLE Cache("
			"path           TEXT    UNIQUE NOT NULL,"
			"cksum          INTEGER NOT NULL,"
			"size           INTEGER NOT NULL,"
			"mtime          INTEGER NOT NULL);");
}

void hash_cache::store_cksums()
{
	std::lock_guard<std::mutex> lock(this->mutex);
	if (!this->db)
	{
		return;
	}
	SqliteConnection &db(*this->db);
	create_or_empty_table(db);
	char const sql[] =
		"INSERT INTO Cache(path, cksum, size, mtime) VALUES(?, ?, ?, ?)";
	SqliteConnection::StmtPtr stmt(db.PrepareStmt(sql));
	db.StartTransaction();
	for (auto const & cksum_and_info : this->cache)
	{
		SqliteBind(
				*stmt,
				cksum_and_info.first,
				cksum_and_info.second.sum,
				cksum_and_info.second.size,
				cksum_and_info.second.mtime);
		int res = sqlite3_step(stmt.get());
		if (res != SQLITE_DONE)
			db.Fail("Inserting EqClass");
		res = sqlite3_clear_bindings(stmt.get());
		if (res != SQLITE_OK)
			db.Fail("Clearing EqClass bindings");
		res = sqlite3_reset(stmt.get());
		if (res != SQLITE_OK)
			db.Fail("Clearing EqClass bindings");
	}
	db.EndTransaction();
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

file_info hash_cache::operator()(boost::filesystem::path const & p)
{
	std::string const native = p.native();
	int fd = open(native.c_str(), O_RDONLY);
	if (fd == -1)
		throw fs_exception(errno, "open '" + native + "'");
	auto_fd_closer closer(fd);

	inode_cache::stat_result stat_res =
		ino_cache.get_inode_info(fd, native);
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		cache_map::const_iterator it = this->cache.find(p.native());
		if (it != this->cache.end())
		{
			file_info const & cached = it->second;
			if (cached.size == stat_res.size && cached.mtime == stat_res.mtime)
				return it->second;
		}
	}
	cksum cksum = compute_cksum(fd, stat_res.id, native);
	file_info res(stat_res.size, stat_res.mtime, cksum);

	std::lock_guard<std::mutex> lock(this->mutex);
	// If some other thread inserted a checksum for the same file in the
	// meantime, it's not a big deal.
	this->cache[p.native()] = res;
	if (this->cache.size() % 1000 == 0)
		LOG(INFO, "Cache size: " << this->cache.size());
	return res;
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



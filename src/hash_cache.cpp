#include "hash_cache.h"

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include <openssl/sha.h>

#include <boost/functional/hash/hash.hpp>

#include "exceptions.h"
#include "log.h"
#include "sql_lib_impl.h"

namespace {

// This is not stored because it is likely to have false positive matches when
// inodes are reused. It is also not populated on hash_cache deserialization
// because it's doubtful to bring much gain and is guaranteed to cost a lot if
// we stat all the files.
class inode_cache {
public:
  using uuid = std::pair<dev_t, ino_t>;
  struct stat_result {
    stat_result(uuid id, off_t size, time_t mtime)
        : id(std::move(id)), size(size), mtime(mtime) {}

    uuid id;
    off_t size;
    time_t mtime;
  };

  std::pair<bool, cksum> get(uuid ino) {
    std::lock_guard<std::mutex> lock(this->mutex);
    auto it = this->cache_map.find(ino);
    if (it != this->cache_map.end()) {
      return std::make_pair(true, it->second);
    }
    return std::make_pair(false, 0);
  }

  void update(uuid ino, cksum sum) {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->cache_map.insert(std::make_pair(ino, sum));
  }

  static stat_result get_inode_info(int fd,
                                    std::string const &path_for_errors) {
    struct stat st;
    int res = fstat(fd, &st);
    if (res != 0) {
      throw fs_exception(errno, "stat on '" + path_for_errors + "'");
    }
    if (!S_ISREG(st.st_mode)) {
      throw fs_exception(errno,
                         "'" + path_for_errors + "' is not a regular file");
    }
    return stat_result(std::make_pair(st.st_dev, st.st_ino), st.st_size,
                       st.st_mtime);
  }

private:
  using CacheMap = std::unordered_map<uuid, cksum, boost::hash<uuid>>;
  CacheMap cache_map;
  std::mutex mutex;
};

// I'm asking for trouble, but I'm lazy.
inode_cache ino_cache;

cksum compute_cksum(int fd, inode_cache::uuid uuid,
                    std::string const &path_for_errors) {
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
  while (true) {
    ssize_t res = read(fd, buf.get(), buf_size);
    if (res < 0) {
      throw fs_exception(errno, "read '" + path_for_errors + "'");
    }
    if (res == 0) {
      break;
    }
    size += res;
    SHA1_Update(&sha, reinterpret_cast<u_char *>(buf.get()), res);
  }
  SHA1_Final(sha_res.complete, &sha);

  if (size) {
    ino_cache.update(uuid, sha_res.prefix);
    return sha_res.prefix;
  }
  ino_cache.update(uuid, 0);
  return 0;
}

} /* anonymous namespace */

std::unordered_map<std::string, file_info>
read_cache_from_db(std::string const &path) {
  std::unordered_map<std::string, file_info> cache;
  SqliteConnection db(path, SQLITE_OPEN_READONLY);
  for (const auto &[path, sum, size, mtime] :
       db.Query<std::string, cksum, off_t, time_t>(
           "SELECT path, cksum, size, mtime FROM Cache")) {
    DLOG("Read \"" << path << "\": " << sum << " " << size << " " << mtime);

    file_info f_info(size, mtime, sum);

    cache.insert(std::make_pair(path, f_info));
  }
  return cache;
}

hash_cache *hash_cache::instance;

hash_cache::initializer::initializer(std::string const &read_cache_from,
                                     std::string const &dump_cache_to) {
  hash_cache::initialize(read_cache_from, dump_cache_to);
}

hash_cache::initializer::~initializer() { hash_cache::finalize(); }

hash_cache::hash_cache(std::string const &read_cache_from,
                       std::string const &dump_cache_to) {
  if (!read_cache_from.empty()) {
    auto cache = read_cache_from_db(read_cache_from);
    std::lock_guard<std::mutex> lock(this->mutex);
    this->cache.swap(cache);
  }
  if (!dump_cache_to.empty()) {
    this->db = std::make_unique<SqliteConnection>(dump_cache_to);
  }
}

hash_cache::~hash_cache() { this->store_cksums(); }

static void create_or_empty_table(SqliteConnection &db) {
  db.SqliteExec("DROP TABLE IF EXISTS Cache;"
                "CREATE TABLE Cache("
                "path           TEXT    UNIQUE NOT NULL,"
                "cksum          INTEGER NOT NULL,"
                "size           INTEGER NOT NULL,"
                "mtime          INTEGER NOT NULL);");
}

void hash_cache::store_cksums() {
  std::lock_guard<std::mutex> lock(this->mutex);
  if (!this->db) {
    return;
  }
  SqliteConnection &db(*this->db);
  create_or_empty_table(db);
  SqliteTransaction trans(db);
  auto out = db.BatchInsert<std::string, cksum, off_t, time_t>(
      "INSERT INTO Cache(path, cksum, size, mtime) VALUES(?, ?, ?, ?)");
  std::transform(this->cache.begin(), this->cache.end(), out->begin(),
                 [](const std::pair<std::string, file_info> &file) {
                   return std::make_tuple(file.first, file.second.sum,
                                          file.second.size, file.second.mtime);
                 });
  trans.Commit();
}

namespace {

struct auto_fd_closer {
  explicit auto_fd_closer(int fd) : fd(fd) {}
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

file_info hash_cache::operator()(boost::filesystem::path const &p) {
  std::string const &native = p.native();
  int fd = open(native.c_str(), O_RDONLY);
  if (fd == -1) {
    throw fs_exception(errno, "open '" + native + "'");
  }
  auto_fd_closer closer(fd);

  inode_cache::stat_result stat_res = inode_cache::get_inode_info(fd, native);
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    auto it = this->cache.find(p.native());
    if (it != this->cache.end()) {
      file_info const &cached = it->second;
      if (cached.size == stat_res.size && cached.mtime == stat_res.mtime) {
        return it->second;
      }
    }
  }
  cksum cksum = compute_cksum(fd, stat_res.id, native);
  file_info res(stat_res.size, stat_res.mtime, cksum);

  std::lock_guard<std::mutex> lock(this->mutex);
  // If some other thread inserted a checksum for the same file in the
  // meantime, it's not a big deal.
  this->cache[p.native()] = res;
  if (this->cache.size() % 1000 == 0) {
    LOG(INFO, "Cache size: " << this->cache.size());
  }
  return res;
}

void hash_cache::initialize(std::string const &read_cache_from,
                            std::string const &dump_cache_to) {
  assert(!instance);
  hash_cache::instance = new hash_cache(read_cache_from, dump_cache_to);
}

void hash_cache::finalize() {
  assert(instance);
  delete hash_cache::instance;
  hash_cache::instance = nullptr;
}

hash_cache &hash_cache::get() {
  assert(hash_cache::instance);
  return *hash_cache::instance;
}

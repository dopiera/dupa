#include "hash_cache.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

#include <memory>
#include <utility>

#include <openssl/sha.h>

#include <boost/functional/hash/hash.hpp>

#include "db_lib_impl.h"
#include "exceptions.h"
#include "log.h"

namespace {

// This is not stored because it is likely to have false positive matches when
// inodes are reused. It is also not populated on HashCache deserialization
// because it's doubtful to bring much gain and is guaranteed to cost a lot if
// we stat all the files.
class InodeCache {
 public:
  using Uuid = std::pair<dev_t, ino_t>;
  struct StatResult {
    StatResult(Uuid id, off_t size, time_t mtime)
        : id_(std::move(id)), size_(size), mtime_(mtime) {}

    Uuid id_;
    off_t size_;
    time_t mtime_;
  };

  std::pair<bool, Cksum> Get(Uuid ino) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(ino);
    if (it != cache_map_.end()) {
      return std::make_pair(true, it->second);
    }
    return std::make_pair(false, 0);
  }

  void Update(Uuid ino, Cksum sum) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_map_.insert(std::make_pair(ino, sum));
  }

  static StatResult GetInodeInfo(int fd, const std::string &path_for_errors) {
    struct stat st;
    int res = fstat(fd, &st);
    if (res != 0) {
      throw FsException(errno, "stat on '" + path_for_errors + "'");
    }
    if (!S_ISREG(st.st_mode)) {
      throw FsException(errno,
                        "'" + path_for_errors + "' is not a regular file");
    }
    return StatResult(std::make_pair(st.st_dev, st.st_ino), st.st_size,
                      st.st_mtime);
  }

 private:
  using CacheMap = std::unordered_map<Uuid, Cksum, boost::hash<Uuid>>;
  CacheMap cache_map_;
  std::mutex mutex_;
};

// I'm asking for trouble, but I'm lazy.
InodeCache ino_cache;

Cksum ComputeCksum(int fd, InodeCache::Uuid uuid,
                   const std::string &path_for_errors) {
  {
    std::pair<bool, Cksum> sum = ino_cache.Get(uuid);
    if (sum.first) {
      DLOG(path_for_errors << " shares an inode with something already "
                              "computed!");
      return sum.second;
    }
  }

  const size_t buf_size = 1024 * 1024;
  std::unique_ptr<char[]> buf(new char[buf_size]);

  union {
    u_char complete_[20];
    Cksum prefix_;
  } sha_res;

  SHA_CTX sha;
  SHA1_Init(&sha);
  size_t size = 0;
  while (true) {
    ssize_t res = read(fd, buf.get(), buf_size);
    if (res < 0) {
      throw FsException(errno, "read '" + path_for_errors + "'");
    }
    if (res == 0) {
      break;
    }
    size += res;
    SHA1_Update(&sha, reinterpret_cast<u_char *>(buf.get()), res);
  }
  SHA1_Final(sha_res.complete_, &sha);

  if (size) {
    ino_cache.Update(uuid, sha_res.prefix_);
    return sha_res.prefix_;
  }
  ino_cache.Update(uuid, 0);
  return 0;
}

} /* anonymous namespace */

std::unordered_map<std::string, FileInfo> ReadCacheFromDb(
    const std::string &path) {
  std::unordered_map<std::string, FileInfo> cache;
  DBConnection db(path, SQLITE_OPEN_READONLY);
  for (const auto &[path, sum, size, mtime] :
       db.Query<std::string, Cksum, off_t, time_t>(
           "SELECT path, cksum, size, mtime FROM FileList")) {
    DLOG("Read \"" << path << "\": " << sum << " " << size << " " << mtime);

    FileInfo f_info(size, mtime, sum);

    cache.insert(std::make_pair(path, f_info));
  }
  return cache;
}

HashCache *HashCache::instance_;

HashCache::Initializer::Initializer(const std::string &read_cache_from,
                                    const std::string &dump_cache_to) {
  HashCache::Initialize(read_cache_from, dump_cache_to);
}

HashCache::Initializer::~Initializer() { HashCache::Finalize(); }

HashCache::HashCache(const std::string &read_cache_from,
                     const std::string &dump_cache_to) {
  if (!read_cache_from.empty()) {
    auto cache = ReadCacheFromDb(read_cache_from);
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.swap(cache);
  }
  if (!dump_cache_to.empty()) {
    db_ = std::make_unique<DBConnection>(dump_cache_to);
  }
}

HashCache::~HashCache() { StoreCksums(); }

static void CreateOrEmptyTable(DBConnection &db) {
  db.Exec(
      "DROP TABLE IF EXISTS FileList;"
      "CREATE TABLE FileList("
      "path           TEXT    UNIQUE NOT NULL,"
      "cksum          INTEGER NOT NULL,"
      "size           INTEGER NOT NULL,"
      "mtime          INTEGER NOT NULL);");
}

void HashCache::StoreCksums() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) {
    return;
  }
  DBConnection &db(*db_);
  CreateOrEmptyTable(db);
  DBTransaction trans(db);
  auto out = db.Prepare<std::string, Cksum, off_t, time_t>(
      "INSERT INTO FileList(path, cksum, size, mtime) VALUES(?, ?, ?, ?)");
  std::transform(cache_.begin(), cache_.end(), out->begin(),
                 [](const std::pair<std::string, FileInfo> &file) {
                   return std::make_tuple(file.first, file.second.sum_,
                                          file.second.size_,
                                          file.second.mtime_);
                 });
  trans.Commit();
}

namespace {

class AutoFdCloser {
 public:
  explicit AutoFdCloser(int fd) : fd_(fd) {}
  ~AutoFdCloser() {
    int res = close(fd_);
    if (res < 0) {
      // This is unlikely to happen and if it happens it is likely to be
      // while already handling some other more important exception, so
      // let's not cover the original error.
      LOG(WARNING, "Failed to close descriptor " << fd_);
    }
  }

 private:
  int fd_;
};

} /* anonymous namespace */

FileInfo HashCache::operator()(const boost::filesystem::path &p) {
  const std::string &native = p.native();
  int fd = open(native.c_str(), O_RDONLY);
  if (fd == -1) {
    throw FsException(errno, "open '" + native + "'");
  }
  AutoFdCloser closer(fd);

  InodeCache::StatResult stat_res = InodeCache::GetInodeInfo(fd, native);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(p.native());
    if (it != cache_.end()) {
      const FileInfo &cached = it->second;
      if (cached.size_ == stat_res.size_ && cached.mtime_ == stat_res.mtime_) {
        return it->second;
      }
    }
  }
  Cksum cksum = ComputeCksum(fd, stat_res.id_, native);
  FileInfo res(stat_res.size_, stat_res.mtime_, cksum);

  std::lock_guard<std::mutex> lock(mutex_);
  // If some other thread inserted a checksum for the same file in the
  // meantime, it's not a big deal.
  cache_[p.native()] = res;
  if (cache_.size() % 1000 == 0) {
    LOG(INFO, "Cache size: " << cache_.size());
  }
  return res;
}

void HashCache::Initialize(const std::string &read_cache_from,
                           const std::string &dump_cache_to) {
  assert(!instance_);
  HashCache::instance_ = new HashCache(read_cache_from, dump_cache_to);
}

void HashCache::Finalize() {
  assert(instance_);
  delete HashCache::instance_;
  HashCache::instance_ = nullptr;
}

HashCache &HashCache::Get() {
  assert(HashCache::instance_);
  return *HashCache::instance_;
}

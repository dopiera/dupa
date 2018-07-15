#ifndef SRC_HASH_CACHE_H_
#define SRC_HASH_CACHE_H_

#include <cstdint>

#include <memory>
#include <mutex>
#include <string>

#include <unordered_map>

#include <boost/filesystem/path.hpp>

#include "sql_lib.h"

using Cksum = uint64_t;

struct FileInfo {
  FileInfo() = default;
  FileInfo(off_t size, time_t mtime, Cksum sum)
      : size_(size), mtime_(mtime), sum_(sum) {}

  off_t size_;
  time_t mtime_;
  Cksum sum_;
};

std::unordered_map<std::string, FileInfo> ReadCacheFromDb(
    std::string const &path);

class HashCache {
 public:
  struct Initializer {
    Initializer(std::string const &read_cache_from,
                std::string const &dump_cache_to);
    ~Initializer();
  };
  static HashCache &Get();
  FileInfo operator()(boost::filesystem::path const &p);

 private:
  HashCache(std::string const &read_cache_from,
            std::string const &dump_cache_to);
  ~HashCache();
  void StoreCksums();
  static void Initialize(std::string const &read_cache_from,
                         std::string const &dump_cache_to);
  static void Finalize();

  static HashCache *instance_;

  using CacheMap = std::unordered_map<std::string, FileInfo>;
  CacheMap cache_;
  std::unique_ptr<SqliteConnection> db_;
  std::mutex mutex_;
};

#endif  // SRC_HASH_CACHE_H_

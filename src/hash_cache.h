#ifndef SRC_HASH_CACHE_H_
#define SRC_HASH_CACHE_H_

#include <cstdint>

#include <memory>
#include <mutex>
#include <string>

#include <unordered_map>

#include <boost/filesystem/path.hpp>

#include "db_lib.h"

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
    const std::string &path);

class HashCache {
 public:
  class Initializer {
   public:
    Initializer(const std::string &read_cache_from,
                const std::string &dump_cache_to);
    ~Initializer();
  };
  static HashCache &Get();
  FileInfo operator()(const boost::filesystem::path &p);

 private:
  HashCache(const std::string &read_cache_from,
            const std::string &dump_cache_to);
  ~HashCache();
  void StoreCksums();
  static void Initialize(const std::string &read_cache_from,
                         const std::string &dump_cache_to);
  static void Finalize();

  static HashCache *instance_;

  using CacheMap = std::unordered_map<std::string, FileInfo>;
  CacheMap cache_;
  std::unique_ptr<DBConnection> db_;
  std::mutex mutex_;
};

#endif  // SRC_HASH_CACHE_H_

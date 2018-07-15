#ifndef HASH_CACHE_H_6332
#define HASH_CACHE_H_6332

#include <cstdint>

#include <memory>
#include <mutex>
#include <string>

#include <unordered_map>

#include <boost/filesystem/path.hpp>

#include "sql_lib.h"

using cksum = uint64_t;

struct file_info {
  file_info() = default;
  file_info(off_t size, time_t mtime, cksum sum)
      : size(size), mtime(mtime), sum(sum) {}

  off_t size;
  time_t mtime;
  cksum sum;
};

std::unordered_map<std::string, file_info>
read_cache_from_db(std::string const &path);

class hash_cache {
public:
  struct initializer {
    initializer(std::string const &read_cache_from,
                std::string const &dump_cache_to);
    ~initializer();
  };
  static hash_cache &get();
  file_info operator()(boost::filesystem::path const &p);

private:
  hash_cache(std::string const &read_cache_from,
             std::string const &dump_cache_to);
  ~hash_cache();
  void store_cksums();
  static void initialize(std::string const &read_cache_from,
                         std::string const &dump_cache_to);
  static void finalize();

  static hash_cache *instance;

  using cache_map = std::unordered_map<std::string, file_info>;
  cache_map cache;
  std::unique_ptr<SqliteConnection> db;
  std::mutex mutex;
};

#endif /* HASH_CACHE_H_6332 */

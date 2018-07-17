#ifndef SRC_SCANNER_INT_H_
#define SRC_SCANNER_INT_H_

#include "scanner.h"

#include <map>
#include <stack>
#include <thread>
#include <utility>

#include <boost/filesystem/convenience.hpp>

#include "conf.h"
#include "hash_cache.h"
#include "log.h"
#include "synch_thread_pool.h"

namespace detail {

boost::filesystem::path CommonPathPrefix(const boost::filesystem::path &p1,
                                         const boost::filesystem::path &p2);

}  // namespace detail

template <class DIR_HANDLE>
void ScanDirectory(const boost::filesystem::path &root,
                   ScanProcessor<DIR_HANDLE> &processor) {
  using boost::filesystem::path;

  std::stack<std::pair<path, DIR_HANDLE>> dirs_to_process;
  dirs_to_process.push(std::make_pair(root, processor.RootDir(root)));

  SyncThreadPool pool(Conf().concurrency_);
  std::mutex mutex;

  while (!dirs_to_process.empty()) {
    const path dir = dirs_to_process.top().first;
    const DIR_HANDLE handle = dirs_to_process.top().second;

    dirs_to_process.pop();

    using boost::filesystem::directory_iterator;
    for (directory_iterator it(dir); it != directory_iterator(); ++it) {
      if (is_symlink(it->path())) {
        continue;
      }
      const path new_path = it->path();
      if (is_directory(it->status())) {
        std::lock_guard<std::mutex> lock(mutex);
        dirs_to_process.push(
            std::make_pair(new_path, processor.Dir(new_path, handle)));
      }
      if (boost::filesystem::is_regular(new_path)) {
        pool.Submit([new_path, handle, &mutex, &processor]() mutable {
          const FileInfo f_info = HashCache::Get()(new_path);
          if (f_info.sum_) {
            std::lock_guard<std::mutex> lock(mutex);
            processor.File(new_path, handle, f_info);
          }
        });
      }
    }
  }
}

template <class DIR_HANDLE>
void ScanDb(std::unordered_map<std::string, FileInfo> db,
            ScanProcessor<DIR_HANDLE> &processor) {
  using boost::filesystem::path;

  if (db.empty()) {
    return;
  }

  path common_prefix = path(db.begin()->first).parent_path();
  for (const auto &path_and_fi : db) {
    common_prefix = detail::CommonPathPrefix(
        common_prefix, path(path_and_fi.first).parent_path());
  }
  const size_t prefix_len =
      std::distance(common_prefix.begin(), common_prefix.end());

  std::map<path, DIR_HANDLE> created_dirs;
  created_dirs[common_prefix] = processor.RootDir(common_prefix);
  for (const auto &path_and_fi : db) {
    LOG(INFO, path_and_fi.first);
    const path analyzed(path_and_fi.first);
    const path dir(analyzed.parent_path());

    path::const_iterator it = dir.begin();
    for (size_t i = 0; i < prefix_len; ++i) {
      ++it;
    }

    path parent = common_prefix;
    DIR_HANDLE parent_handle = created_dirs[common_prefix];

    for (; it != dir.end(); ++it) {
      const path to_insert = parent / *it;
      auto created_dir_it = created_dirs.find(to_insert);
      if (created_dir_it == created_dirs.end()) {
        created_dir_it = created_dirs
                             .insert(std::make_pair(
                                 to_insert, processor.Dir(*it, parent_handle)))
                             .first;
      }
      parent = to_insert;
      parent_handle = created_dir_it->second;
    }
    processor.File(analyzed.filename(), parent_handle, path_and_fi.second);
  }
}

template <class DIR_HANDLE>
void ScanDb(const boost::filesystem::path &db_path,
            ScanProcessor<DIR_HANDLE> &processor) {
  using boost::filesystem::path;

  auto db = ReadCacheFromDb(db_path.native());
  ScanDb(db, processor);
}

// Will call one of the 2 above.
template <class DIR_HANDLE>
void ScanDirectoryOrDb(const std::string &path,
                       ScanProcessor<DIR_HANDLE> &processor) {
  const std::string db_prefix = "db:";
  if (!Conf().ignore_db_prefix_ && path.find(db_prefix) == 0) {
    ScanDb(boost::filesystem::path(path.substr(db_prefix.length())), processor);
  } else {
    ScanDirectory(path, processor);
  }
}

#endif  // SRC_SCANNER_INT_H_

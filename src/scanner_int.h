/*
 * (C) Copyright 2018 Marek Dopiera
 *
 * This file is part of dupa.
 *
 * dupa is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dupa is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dupa. If not, see http://www.gnu.org/licenses/.
 */

#ifndef SRC_SCANNER_INT_H_
#define SRC_SCANNER_INT_H_

#include "scanner.h"

#include <map>
#include <optional>
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

  // The path to directory and handle to its parent. In case of root directory,
  // empty option is held.
  std::stack<std::pair<path, std::optional<DIR_HANDLE>>> dirs_to_process;
  dirs_to_process.push(std::make_pair(root, std::optional<DIR_HANDLE>()));

  SyncThreadPool pool(Conf().concurrency_);
  std::mutex mutex;

  while (!dirs_to_process.empty()) {
    const path dir = dirs_to_process.top().first;
    const auto maybe_parent_handle = dirs_to_process.top().second;

    dirs_to_process.pop();

    using boost::filesystem::directory_iterator;
    try {
      // Create the iterator before the loop so that we get an exception here if
      // we have no access to the directory.
      directory_iterator it(dir);
      // Add this directory only after we made sure we can browse it.
      DIR_HANDLE handle;
      {
        std::lock_guard<std::mutex> lock(mutex);
        handle =
            maybe_parent_handle.has_value()
                ? processor.Dir(dir.filename(), maybe_parent_handle.value())
                : processor.RootDir(dir);
      }
      for (; it != directory_iterator(); ++it) {
        try {
          if (is_symlink(it->path())) {
            continue;
          }
          const path new_path = it->path();
          if (is_directory(it->status())) {
            std::lock_guard<std::mutex> lock(mutex);
            dirs_to_process.push(std::make_pair(new_path, handle));
          }
          if (boost::filesystem::is_regular(new_path)) {
            pool.Submit([new_path, handle, &mutex, &processor]() mutable {
              try {
                const FileInfo f_info = HashCache::Get()(new_path);
                if (f_info.sum_) {
                  std::lock_guard<std::mutex> lock(mutex);
                  processor.File(new_path.filename(), handle, f_info);
                }
              } catch (const std::exception &e) {
                LOG(ERROR, "skipping \"" << new_path.native()
                                         << "\" because analyzing it yielded "
                                         << e.what());
              }
            });
          }
        } catch (const std::exception &e) {
          LOG(ERROR, "skipping \"" << it->path().native()
                                   << "\" because analyzing it yielded "
                                   << e.what());
        }
      }
    } catch (const std::exception &e) {
      LOG(ERROR, "skipping \"" << dir.native()
                               << "\" because descending into it yielded "
                               << e.what());
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

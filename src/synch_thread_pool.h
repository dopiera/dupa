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

#ifndef SRC_SYNCH_THREAD_POOL_H_
#define SRC_SYNCH_THREAD_POOL_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include <boost/filesystem/path.hpp>

class SyncCounter {
 public:
  explicit SyncCounter(size_t initial = 0);
  void Increment();
  void Decrement();
  void WaitForZero();

 private:
  std::mutex m_;
  std::condition_variable cv_;
  volatile size_t cntr_;
};

class SyncThreadPool {
 public:
  explicit SyncThreadPool(int concurrency);

  // All Submit() calls should finish before this can be called; This needs to
  // be called before destroying this object.
  void Stop();
  // Will block if concurrency + 1 items are already scheduled.
  void Submit(const std::function<void()> &fun);
  ~SyncThreadPool();

 private:
  void ThreadLoop();

  volatile bool closing_;
  volatile size_t outstanding_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable user_cv_;
  std::queue<std::function<void()>> q_;
  std::vector<std::unique_ptr<std::thread>> threads_;
};

#endif  // SRC_SYNCH_THREAD_POOL_H_

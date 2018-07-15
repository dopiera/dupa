#ifndef SRC_SYNCH_THREAD_POOL_H_
#define SRC_SYNCH_THREAD_POOL_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include <boost/filesystem/path.hpp>

struct SyncCounter {
  explicit SyncCounter(size_t initial = 0);
  void Increment();
  void Decrement();
  void WaitForZero();

 private:
  std::mutex m_;
  std::condition_variable cv_;
  volatile size_t cntr_;
};

struct SyncThreadPool {
  explicit SyncThreadPool(int concurrency);

  // All Submit() calls should finish before this can be called; This needs to
  // be called before destroying this object.
  void Stop();
  // Will block if concurrency + 1 items are already scheduled.
  void Submit(std::function<void()> const &fun);
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

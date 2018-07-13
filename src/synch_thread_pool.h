#ifndef SYNCH_THREAD_POOL_991234
#define SYNCH_THREAD_POOL_991234

#include <condition_variable>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include <boost/filesystem/path.hpp>

struct SyncCounter {
  SyncCounter(size_t initial = 0);
  void Increment();
  void Decrement();
  void WaitForZero();

private:
  std::mutex m;
  std::condition_variable cv;
  volatile size_t cntr;
};

struct SyncThreadPool {
  SyncThreadPool(int concurrency);

  // All Submit() calls should finish before this can be called; This needs to
  // be called before destroying this object.
  void Stop();
  // Will block if concurrency + 1 items are already scheduled.
  void Submit(std::function<void()> const &fun);
  ~SyncThreadPool();

private:
  void ThreadLoop();

  volatile bool closing;
  volatile size_t outstanding;
  std::mutex mutex;
  std::condition_variable cv;
  std::condition_variable user_cv;
  std::queue<std::function<void()>> q;
  std::vector<std::unique_ptr<std::thread>> threads;
};

#endif /* SYNCH_THREAD_POOL_991234 */

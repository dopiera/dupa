#include "synch_thread_pool.h"

#include <memory>

SyncThreadPool::SyncThreadPool(int concurrency)
    : closing_(false), outstanding_(0) {
  assert(concurrency > 0);
  for (int i = 0; i < concurrency; ++i) {
    threads_.emplace_back(std::make_unique<std::thread>(
        std::bind(&SyncThreadPool::ThreadLoop, this)));
  }
}

void SyncThreadPool::Stop() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    user_cv_.wait(lock, [this] { return outstanding_ == 0; });
    closing_ = true;
  }
  cv_.notify_all();
  for (auto &thread : threads_) {
    thread->join();
  }
  threads_.clear();
}

void SyncThreadPool::Submit(const std::function<void()> &fun) {
  std::unique_lock<std::mutex> lock(mutex_);
  assert(!closing_);
  user_cv_.wait(lock, [this] { return outstanding_ <= threads_.size(); });
  assert(!closing_);
  q_.push(fun);
  ++outstanding_;
  cv_.notify_one();
}

SyncThreadPool::~SyncThreadPool() {
  Stop();
  assert(closing_);
  assert(threads_.empty());
}

void SyncThreadPool::ThreadLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (closing_) {
        return;
      }
      cv_.wait(lock, [this] { return !q_.empty() || closing_; });
      if (closing_) {
        return;
      }
      task = q_.front();
      q_.pop();
      --outstanding_;
      user_cv_.notify_one();
    }
    task();
  }
}

SyncCounter::SyncCounter(size_t initial) : cntr_(initial) {}

void SyncCounter::Increment() {
  std::lock_guard<std::mutex> lock(m_);
  ++cntr_;
}

void SyncCounter::Decrement() {
  std::lock_guard<std::mutex> lock(m_);
  assert(cntr_ > 0);
  if (cntr_-- == 0) {
    cv_.notify_all();
  }
}

void SyncCounter::WaitForZero() {
  std::unique_lock<std::mutex> lock(m_);
  cv_.wait(lock, [this] { return cntr_ == 0; });
  assert(cntr_ == 0);
}

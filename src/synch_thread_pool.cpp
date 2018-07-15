#include "synch_thread_pool.h"

#include <memory>

SyncThreadPool::SyncThreadPool(int concurrency)
    : closing_(false), outstanding_(0) {
  assert(concurrency > 0);
  for (int i = 0; i < concurrency; ++i) {
    this->threads_.emplace_back(std::make_unique<std::thread>(
        std::bind(&SyncThreadPool::ThreadLoop, this)));
  }
}

void SyncThreadPool::Stop() {
  {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->user_cv_.wait(lock, [this] { return this->outstanding_ == 0; });
    this->closing_ = true;
  }
  this->cv_.notify_all();
  for (auto &thread : this->threads_) {
    thread->join();
  }
  this->threads_.clear();
}

void SyncThreadPool::Submit(std::function<void()> const &fun) {
  std::unique_lock<std::mutex> lock(this->mutex_);
  assert(!this->closing_);
  this->user_cv_.wait(
      lock, [this] { return this->outstanding_ <= this->threads_.size(); });
  assert(!this->closing_);
  this->q_.push(fun);
  ++this->outstanding_;
  this->cv_.notify_one();
}

SyncThreadPool::~SyncThreadPool() {
  this->Stop();
  assert(this->closing_);
  assert(this->threads_.empty());
}

void SyncThreadPool::ThreadLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(this->mutex_);
      if (this->closing_) {
        return;
      }
      this->cv_.wait(lock, [this] { return !this->q_.empty() || this->closing_; });
      if (this->closing_) {
        return;
      }
      task = q_.front();
      q_.pop();
      --this->outstanding_;
      this->user_cv_.notify_one();
    }
    task();
  }
}

SyncCounter::SyncCounter(size_t initial) : cntr_(initial) {}

void SyncCounter::Increment() {
  std::lock_guard<std::mutex> lock(this->m_);
  ++this->cntr_;
}

void SyncCounter::Decrement() {
  std::lock_guard<std::mutex> lock(this->m_);
  assert(this->cntr_ > 0);
  if (this->cntr_-- == 0) {
    this->cv_.notify_all();
  }
}

void SyncCounter::WaitForZero() {
  std::unique_lock<std::mutex> lock(this->m_);
  this->cv_.wait(lock, [this] { return this->cntr_ == 0; });
  assert(this->cntr_ == 0);
}

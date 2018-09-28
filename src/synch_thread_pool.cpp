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
    cv_.notify_all();
  }
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

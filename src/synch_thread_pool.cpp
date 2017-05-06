#include "synch_thread_pool.h"

SyncThreadPool::SyncThreadPool(int concurrency) : closing(false), outstanding(0) {
	assert(concurrency > 0);
	for (int i = 0; i < concurrency; ++i) {
		this->threads.emplace_back(
				std::move(std::unique_ptr<std::thread>(
					new std::thread(
						std::bind(&SyncThreadPool::ThreadLoop, this)))));
	}
}

void SyncThreadPool::Stop() {
	{
		std::unique_lock<std::mutex> lock(this->mutex);
		this->user_cv.wait(
				lock,
				[this] { return this->outstanding == 0; });
		this->closing = true;
	}
	this->cv.notify_all();
	for (auto &thread : this->threads) {
		thread->join();
	}
	this->threads.clear();
}

void SyncThreadPool::Submit(std::function<void()> const &fun) {
	std::unique_lock<std::mutex> lock(this->mutex);
	assert(!this->closing);
	this->user_cv.wait(
			lock,
			[this] { return this->outstanding <= this->threads.size(); });
	assert(!this->closing);
	this->q.push(fun);
	++this->outstanding;
	this->cv.notify_one();
}

SyncThreadPool::~SyncThreadPool() {
	this->Stop();
	assert(this->closing);
	assert(this->threads.empty());
}

void SyncThreadPool::ThreadLoop() {
	while (true) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(this->mutex);
			if (this->closing) {
				return;
			}
			this->cv.wait(
					lock,
					[this] { return !this->q.empty() || this->closing; });
			if (this->closing) {
				return;
			}
			task = q.front();
			q.pop();
			--this->outstanding;
			this->user_cv.notify_one();
		}
		task();
	}
}


SyncCounter::SyncCounter(size_t initial) : cntr(initial) {
}

void SyncCounter::Increment() {
	std::lock_guard<std::mutex> lock(this->m);
	++this->cntr;
}


void SyncCounter::Decrement() {
	std::lock_guard<std::mutex> lock(this->m);
	assert(this->cntr > 0);
	if (this->cntr-- == 0) {
		this->cv.notify_all();
	}
}


void SyncCounter::WaitForZero() {
	std::unique_lock<std::mutex> lock(this->m);
	this->cv.wait(lock, [this] { return this->cntr == 0; });
	assert(this->cntr == 0);
}


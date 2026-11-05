#include "jobqueue.h"

#include <exception>
#include <utility>

#include "log.h"

// A small thread pool for OCR (the Analysis and the Drags). Jobs start in the order they were posted.
// work_ wakes a worker when a job arrives or the queue closes; idle_ wakes WaitIdle when a job ends or
// the queue is emptied.

JobQueue::JobQueue(unsigned threads) {
    if (threads == 0) threads = 1;
    for (unsigned i = 0; i < threads; ++i) threads_.emplace_back([this] { WorkLoop(); });
}

// Jobs that have not started are dropped; the destructor waits for the running ones to end.
JobQueue::~JobQueue() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_ = true;
        jobs_.clear();
    }
    work_.notify_all();
    for (std::thread& t : threads_) t.join();
}

void JobQueue::Post(Job job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push_back(std::move(job));
    }
    work_.notify_one();
}

// Drops the jobs that have not started (a running job goes on to its end).
void JobQueue::CancelPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.clear();
    idle_.notify_all();
}

void JobQueue::WaitIdle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return jobs_.empty() && running_ == 0; });
}

void JobQueue::WorkLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_.wait(lock, [this] { return closing_ || !jobs_.empty(); });
            if (closing_) return;  // the destructor already cleared the jobs
            job = std::move(jobs_.front());
            jobs_.pop_front();
            ++running_;
        }

        // The boundary of the background thread: an exception that got out of here would end
        // the program with no message (std::terminate).
        try {
            job();
        } catch (const std::exception& e) {
            LogError("A background job failed: ", e.what());
        } catch (...) {
            LogError("A background job failed with an unknown error.");
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            --running_;
        }
        idle_.notify_all();
    }
}

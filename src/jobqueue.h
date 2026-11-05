#pragma once
// A small pool of background threads that run jobs from a queue, in the order they were posted.
// Win32-free so the tests can use it. Recognition jobs (the Analysis, a Drag) run here so the UI
// thread keeps pumping messages; a job reports back by posting a window message itself.
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class JobQueue {
public:
    using Job = std::function<void()>;

    // `threads` jobs can run at once. Two, by default: the Analysis of the Shot and a Drag made
    // while it is still running are both OCR, and OcrEngine::ReadLines may be called from
    // several threads.
    explicit JobQueue(unsigned threads = 2);
    ~JobQueue();  // drops the queued jobs and waits for the running ones
    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    // A std::exception thrown by a job is logged (its what(), never the job's data) and the
    // thread carries on with the next job.
    void Post(Job job);

    void CancelPending();  // drops the jobs that have not started
    void WaitIdle();       // returns when nothing is queued or running

private:
    void WorkLoop();

    std::mutex mutex_;
    std::condition_variable work_;  // a job was posted, or the queue is closing
    std::condition_variable idle_;  // a job finished
    std::deque<Job> jobs_;
    unsigned running_ = 0;
    bool closing_ = false;
    std::vector<std::thread> threads_;
};

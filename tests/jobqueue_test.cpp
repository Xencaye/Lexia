// Unit tests for jobqueue.h: order, exceptions, CancelPending, WaitIdle.
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "jobqueue.h"
#include "log.h"

TEST_CASE("JobQueue: one thread runs the jobs in the order they were posted") {
    JobQueue q(1);
    std::mutex m;
    std::vector<int> seen;
    for (int i = 0; i < 20; ++i) {
        q.Post([&, i] {
            std::lock_guard<std::mutex> lock(m);
            seen.push_back(i);
        });
    }
    q.WaitIdle();
    REQUIRE(seen.size() == 20);
    for (int i = 0; i < 20; ++i) CHECK(seen[static_cast<size_t>(i)] == i);
}

TEST_CASE("JobQueue: a job that throws is logged and the next job still runs") {
    // The failure goes to the log; keep the real one clean.
    SetLogFile(std::filesystem::temp_directory_path() / "lexia_jobqueue_test.log");
    JobQueue q(1);
    std::atomic<int> ran{0};
    q.Post([] { throw std::runtime_error("boom"); });
    q.Post([] { throw 42; });
    q.Post([&] { ++ran; });
    q.WaitIdle();
    CHECK(ran == 1);
}

TEST_CASE("JobQueue: CancelPending drops the jobs that have not started, not the running one") {
    JobQueue q(1);
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<bool> started{false};
    std::atomic<int> ran{0};
    q.Post([&] {
        started = true;
        gate.wait();
        ++ran;
    });
    q.Post([&] { ++ran; });
    q.Post([&] { ++ran; });
    while (!started) std::this_thread::yield();

    q.CancelPending();
    release.set_value();
    q.WaitIdle();
    CHECK(ran == 1);
}

TEST_CASE("JobQueue: WaitIdle waits for a running job") {
    JobQueue q(2);
    std::atomic<bool> done{false};
    q.Post([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done = true;
    });
    q.WaitIdle();
    CHECK(done);
}

TEST_CASE("JobQueue: two threads run two jobs at the same time") {
    JobQueue q(2);
    // Each job waits until the other has started: with a single thread this would time out.
    std::promise<void> a, b;
    auto fa = a.get_future(), fb = b.get_future();
    std::atomic<bool> ok{true};
    q.Post([&] {
        a.set_value();
        if (fb.wait_for(std::chrono::seconds(5)) != std::future_status::ready) ok = false;
    });
    q.Post([&] {
        b.set_value();
        if (fa.wait_for(std::chrono::seconds(5)) != std::future_status::ready) ok = false;
    });
    q.WaitIdle();
    CHECK(ok);
}

TEST_CASE("JobQueue: the destructor waits for the running job and drops the rest") {
    std::atomic<int> ran{0};
    {
        JobQueue q(1);
        std::atomic<bool> started{false};
        q.Post([&] {
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ++ran;
        });
        q.Post([&] { ++ran; });
        while (!started) std::this_thread::yield();
    }
    CHECK(ran == 1);
}

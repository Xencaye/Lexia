#pragma once
// An AudioSink for tests: records what happens to it, and "plays" only when the test calls
// Advance. Like a real sink, it plays nothing while paused, and Stop drops what is queued and
// clears the pause. Safe to use from two threads, as the contract says.
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>

#include "tts/audio_sink.h"

class FakeAudioSink : public AudioSink {
public:
    void Write(const float*, size_t count, int sampleRate) override {
        if (paused) ++writesWhilePaused;
        // Before the audio lands and outside m_: the hook may call back into the queue, which
        // stops this sink (as another thread pressing a key during a slow Write would).
        if (onWrite) onWrite();
        std::lock_guard<std::mutex> lk(m_);
        pending_ += static_cast<double>(count) / sampleRate;
        written_ += static_cast<double>(count) / sampleRate;
        ++writes;
    }
    void Pause() override {
        paused = true;
        ++pauses;
    }
    void Resume() override {
        paused = false;
        ++resumes;
    }
    void Stop() override {
        {
            std::lock_guard<std::mutex> lk(m_);
            pending_ = 0;
        }
        paused = false;
        ++stops;
    }
    double PendingSeconds() const override {
        std::lock_guard<std::mutex> lk(m_);
        return pending_;
    }

    // The sound card plays for `seconds`.
    void Advance(double seconds) {
        std::lock_guard<std::mutex> lk(m_);
        if (!paused) pending_ = std::max(0.0, pending_ - seconds);
    }
    // Plays out everything queued.
    void Drain() { Advance(PendingSeconds()); }
    double TotalWrittenSeconds() const {
        std::lock_guard<std::mutex> lk(m_);
        return written_;
    }

    std::atomic<bool> paused{false};
    std::atomic<int> writes{0}, pauses{0}, resumes{0}, stops{0};
    std::atomic<int> writesWhilePaused{0};
    // Runs inside every Write. Set it from the test's thread only while no other thread writes.
    std::function<void()> onWrite;

private:
    mutable std::mutex m_;
    double pending_ = 0;
    double written_ = 0;
};

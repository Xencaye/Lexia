#pragma once
// AudioSink over the Windows waveOut API (winmm): no extra dependency, works from any thread.
#include <memory>

#include "tts/audio_sink.h"

class WaveOutSink : public AudioSink {
public:
    WaveOutSink();
    ~WaveOutSink() override;
    WaveOutSink(const WaveOutSink&) = delete;
    WaveOutSink& operator=(const WaveOutSink&) = delete;

    void Write(const float* samples, size_t count, int sampleRate) override;
    void Pause() override;
    void Resume() override;
    void Stop() override;
    double PendingSeconds() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

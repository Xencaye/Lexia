#pragma once
// Where synthesised audio goes. Lexia plays its own audio (ADR-0004), so the Reading queue talks
// to this interface: the Windows implementation plays through the sound card, the fake in the
// tests just records. Every method may be called from any thread. Win32-free.
#include <cstddef>

class AudioSink {
public:
    virtual ~AudioSink() = default;

    // Queues mono samples (-1..1) behind whatever is already queued and returns at once, also
    // while paused. Audio at a different sampleRate from the queued audio is held back, and
    // starts when that has played out; it counts in PendingSeconds meanwhile.
    virtual void Write(const float* samples, size_t count, int sampleRate) = 0;

    // Holds playback where it is, and carries on from there. Audio written while paused is
    // queued and stays silent until Resume.
    virtual void Pause() = 0;
    virtual void Resume() = 0;

    // Drops everything queued and playing, and clears Pause.
    virtual void Stop() = 0;

    // Seconds of audio queued and not yet played: 0 means the sink is idle.
    virtual double PendingSeconds() const = 0;
};

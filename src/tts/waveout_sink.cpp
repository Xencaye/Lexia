#include "tts/waveout_sink.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "log.h"
#include "tts/wav.h"

// The AudioSink that plays to the speakers, with winmm's waveOut API (16-bit mono PCM). Each Write
// becomes one buffer queued on the device; the device plays the buffers in order and marks each one
// done (WHDR_DONE), which Reap() checks. No callback thread: the speech thread and PendingSeconds
// poll. A mutex guards it all, as the speech thread writes while the UI thread may pause or stop.
struct WaveOutSink::Impl {
    // A buffer must stay in memory, at the same address, until the device has played it: hence a
    // unique_ptr per buffer.
    struct Buffer {
        WAVEHDR hdr{};
        std::vector<int16_t> pcm;
    };

    // Audio at another sample rate than the device's, waiting for the device to play out.
    struct Held {
        std::vector<int16_t> pcm;
        int rate = 0;
    };

    ~Impl() { Close(); }

    // Frees the buffers the device has finished with, then sends on the held audio that can go:
    // at the device's rate at once, at another rate once the device is empty (it is reopened).
    // Caller holds m.
    void Reap() {
        while (!bufs.empty() && (bufs.front()->hdr.dwFlags & WHDR_DONE)) {
            waveOutUnprepareHeader(dev, &bufs.front()->hdr, sizeof(WAVEHDR));
            bufs.pop_front();
        }
        while (!held.empty()) {
            Held& h = held.front();
            if (!dev || rate != h.rate) {
                if (!bufs.empty()) return;  // the old rate is still playing
                Close();
                if (!Open(h.rate)) {
                    held.clear();  // no device: dropped, as Write drops it
                    return;
                }
            }
            Submit(std::move(h.pcm));
            held.pop_front();
        }
    }

    // Queues `pcm` on the open device. Caller holds m.
    void Submit(std::vector<int16_t> pcm) {
        auto buf = std::make_unique<Buffer>();
        buf->pcm = std::move(pcm);
        buf->hdr.lpData = reinterpret_cast<LPSTR>(buf->pcm.data());
        buf->hdr.dwBufferLength = static_cast<DWORD>(buf->pcm.size() * sizeof(int16_t));
        if (waveOutPrepareHeader(dev, &buf->hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) return;
        if (waveOutWrite(dev, &buf->hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(dev, &buf->hdr, sizeof(WAVEHDR));
            return;
        }
        bufs.push_back(std::move(buf));
    }

    // Caller holds m, or is ~Impl (then no other thread can reach the sink any more).
    void Close() {
        if (!dev) return;
        waveOutReset(dev);  // marks every queued buffer done
        for (auto& b : bufs) waveOutUnprepareHeader(dev, &b->hdr, sizeof(WAVEHDR));
        bufs.clear();
        waveOutClose(dev);
        dev = nullptr;
    }

    bool Open(int sampleRate) {  // caller holds m
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 1;
        fmt.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 2;
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * 2;
        if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            dev = nullptr;
            if (!openFailureLogged) LogWarn("[audio] Could not open the sound device; nothing will be heard.");
            openFailureLogged = true;  // once per series of failures, not once per chunk
            return false;
        }
        rate = sampleRate;
        openFailureLogged = false;
        if (paused) waveOutPause(dev);
        return true;
    }

    mutable std::mutex m;
    HWAVEOUT dev = nullptr;
    int rate = 0;
    bool paused = false;
    bool openFailureLogged = false;
    std::deque<std::unique_ptr<Buffer>> bufs;
    std::deque<Held> held;  // in order, behind bufs
};

WaveOutSink::WaveOutSink() : impl_(std::make_unique<Impl>()) {}
WaveOutSink::~WaveOutSink() = default;

void WaveOutSink::Write(const float* samples, size_t count, int sampleRate) {
    if (count == 0 || sampleRate <= 0) return;
    std::vector<int16_t> pcm(count);
    for (size_t i = 0; i < count; ++i) {
        pcm[i] = FloatToPcm16(samples[i]);
    }
    Impl& s = *impl_;
    std::lock_guard<std::mutex> lk(s.m);
    // A new sample rate (a Voice with another rate) needs the device reopened, which can only
    // happen once the old audio has played out. It is held, not waited for: while paused that
    // could be a long time, and the speech thread must stay free (Announcements, Stop).
    s.held.push_back({std::move(pcm), sampleRate});
    s.Reap();
}

void WaveOutSink::Pause() {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->paused = true;
    if (impl_->dev) waveOutPause(impl_->dev);
}

void WaveOutSink::Resume() {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->paused = false;
    if (impl_->dev) waveOutRestart(impl_->dev);
}

// Cuts the sound off: drops the held audio and every queued buffer (waveOutReset), and leaves the
// device open and not paused for the next Write.
void WaveOutSink::Stop() {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->paused = false;
    impl_->held.clear();
    if (!impl_->dev) return;
    waveOutReset(impl_->dev);
    waveOutRestart(impl_->dev);
    impl_->Reap();
}

// How much audio is still to be heard: the buffers the device has not finished (counted whole, so a
// half-played buffer counts in full) plus the held audio. The Reading queue uses it to stay only a
// little ahead of the speakers (see reading_queue.h).
double WaveOutSink::PendingSeconds() const {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->Reap();
    double seconds = 0.0;
    if (impl_->rate > 0) {
        size_t samples = 0;
        for (const auto& b : impl_->bufs) samples += b->pcm.size();
        seconds = static_cast<double>(samples) / impl_->rate;
    }
    for (const auto& h : impl_->held) seconds += static_cast<double>(h.pcm.size()) / h.rate;
    return seconds;
}

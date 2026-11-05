#include "tts/reading_queue.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "common.h"
#include "log.h"

namespace {

bool HasText(const Reading& r) {
    for (const std::string& b : r) {
        if (!Trim(b).empty()) return true;
    }
    return false;
}

// Seconds of slack when comparing the sink's pending time with the length of words.
constexpr double kEpsilon = 1e-6;

}  // namespace

ReadingQueue::ReadingQueue(AudioSink& sink, BlockSynthesizer synthesize, double aheadSeconds,
                           double sinkSeconds)
    : sink_(sink),
      synthesize_(std::move(synthesize)),
      aheadSeconds_(aheadSeconds),
      sinkSeconds_(sinkSeconds < 0 ? aheadSeconds : sinkSeconds) {}

void ReadingQueue::Add(Reading reading) {
    if (!HasText(reading)) return;
    std::lock_guard<std::mutex> lk(m_);
    queue_.push_back(std::move(reading));
    ReportActiveLocked();
}

void ReadingQueue::Announce(AnnouncementSpeech speak) {
    if (!speak) return;
    std::lock_guard<std::mutex> lk(m_);
    ++annGen_;  // whatever was waiting or being made is out of date
    if (annState_ == AnnState::Playing) {
        sink_.Stop();     // cut the old one off; also clears the sink's pause
        paused_ = false;  // ... the person's pause on it (a pause held over the Reading stays)
    }
    annState_ = AnnState::Waiting;
    annSpeak_ = std::move(speak);
    annAudio_.clear();
    ReportActiveLocked();
}

void ReadingQueue::SetActiveListener(std::function<void(bool)> listener) {
    std::lock_guard<std::mutex> lk(m_);
    activeListener_ = std::move(listener);
    reportedActive_ = ActiveLocked();
}

void ReadingQueue::ReportActiveLocked() {
    const bool active = ActiveLocked();
    if (active == reportedActive_) return;
    reportedActive_ = active;
    if (activeListener_) activeListener_(active);
}

void ReadingQueue::Pause() {
    std::lock_guard<std::mutex> lk(m_);
    // Only something audible can be held: a Reading, or an Announcement that is playing.
    if ((queue_.empty() && annState_ != AnnState::Playing) || paused_ || heldPause_) return;
    paused_ = true;
    sink_.Pause();
}

void ReadingQueue::Resume() {
    std::lock_guard<std::mutex> lk(m_);
    if (heldPause_) {  // the Announcement is audible already: only the Reading's pause ends
        heldPause_ = false;
        return;
    }
    if (!paused_) return;
    paused_ = false;
    sink_.Resume();
}

void ReadingQueue::Skip() {
    std::lock_guard<std::mutex> lk(m_);
    if (annState_ != AnnState::None) {
        DropAnnouncementLocked();
        return;
    }
    if (queue_.empty()) return;
    CutOffLocked();  // also ends a pause: skipping resumes
    AdvanceLocked();
}

void ReadingQueue::StopLocked() {
    CutOffLocked();
    queue_.clear();
    block_ = 0;
    ++annGen_;
    annState_ = AnnState::None;
    annSpeak_ = nullptr;
    annAudio_.clear();
    heldPause_ = false;
    ReportActiveLocked();
}

void ReadingQueue::Stop() {
    std::lock_guard<std::mutex> lk(m_);
    StopLocked();
}

void ReadingQueue::SessionStarted() {
    std::lock_guard<std::mutex> lk(m_);
    if (paused_ || heldPause_) StopLocked();
}

bool ReadingQueue::Active() const {
    std::lock_guard<std::mutex> lk(m_);
    return ActiveLocked();
}

bool ReadingQueue::Paused() const {
    std::lock_guard<std::mutex> lk(m_);
    return paused_ || heldPause_;
}

void ReadingQueue::AdvanceLocked() {
    // An empty queue has no Block to drop (a Stop or Skip got there first); the rest still holds.
    if (!queue_.empty() && ++block_ >= queue_.front().size()) {
        queue_.erase(queue_.begin());
        block_ = 0;
    }
    // A Pause that came just as the last Block ended has nothing left to hold: it must not
    // swallow the next Pick.
    if (queue_.empty() && paused_ && annState_ != AnnState::Playing) {
        paused_ = false;
        sink_.Resume();
    }
    ReportActiveLocked();
}

void ReadingQueue::CutOffLocked() {
    ++epoch_;
    speaking_ = false;
    paused_ = false;
    buffer_.clear();
    bufferedSeconds_ = 0;
    sinkPieces_.clear();
    sinkPiecesSeconds_ = 0;
    sink_.Stop();  // also clears the sink's pause
}

// ------------------------------------------------------------------ the Reading ---

void ReadingQueue::AppendChunkLocked(const SpeechChunk& chunk) {
    const size_t n = chunk.samples.size();
    if (n == 0 || chunk.sampleRate <= 0) return;
    // A word ends where the next ' ' phoneme starts (the silence of the space then belongs to the
    // start of the next piece, so it is kept when the Reading resumes).
    std::vector<size_t> cuts{0};
    for (const PhonemeTiming& p : chunk.phonemes) {
        if (p.phoneme != U' ' || p.startSample <= 0) continue;
        const size_t at = static_cast<size_t>(p.startSample);
        if (at > cuts.back() && at < n) cuts.push_back(at);
    }
    cuts.push_back(n);
    for (size_t i = 0; i + 1 < cuts.size(); ++i) {
        Piece piece;
        piece.samples.assign(chunk.samples.begin() + static_cast<std::ptrdiff_t>(cuts[i]),
                             chunk.samples.begin() + static_cast<std::ptrdiff_t>(cuts[i + 1]));
        piece.sampleRate = chunk.sampleRate;
        bufferedSeconds_ += piece.Seconds();
        buffer_.push_back(std::move(piece));
    }
}

void ReadingQueue::PruneSinkPiecesLocked() {
    const double pending = sink_.PendingSeconds();
    // A word is played once the audio behind it is all that is pending.
    while (!sinkPieces_.empty() &&
           sinkPiecesSeconds_ - sinkPieces_.front().Seconds() >= pending - kEpsilon) {
        sinkPiecesSeconds_ -= sinkPieces_.front().Seconds();
        sinkPieces_.pop_front();
    }
    if (sinkPieces_.empty()) sinkPiecesSeconds_ = 0;
}

bool ReadingQueue::FeedLocked(std::unique_lock<std::mutex>& lk, unsigned long long epoch) {
    bool fed = false;
    // Nothing while paused: the sink would only hold it, and a sink may wait inside Write for a
    // paused sound card (WaveOutSink does when the sample rate changes), which would keep the
    // speech thread from making an Announcement. The words wait in buffer_ instead.
    while (epoch_ == epoch && annState_ == AnnState::None && !paused_ && !buffer_.empty()) {
        PruneSinkPiecesLocked();
        if (sink_.PendingSeconds() >= sinkSeconds_) break;
        Piece piece = std::move(buffer_.front());
        buffer_.pop_front();
        bufferedSeconds_ = std::max(0.0, bufferedSeconds_ - piece.Seconds());
        // Not under m_: a sink may wait inside Write (WaveOutSink does when the sample rate
        // changes), and Stop, Pause and Skip must not wait behind that.
        lk.unlock();
        sink_.Write(piece.samples.data(), piece.samples.size(), piece.sampleRate);
        lk.lock();
        if (epoch_ != epoch) {
            // A Stop or Skip came in during the Write, whose audio may have landed after the
            // sink was cut off. Only the speech thread writes, so cutting the sink off again is safe.
            sink_.Stop();
            return fed;
        }
        sinkPiecesSeconds_ += piece.Seconds();
        sinkPieces_.push_back(std::move(piece));
        fed = true;
    }
    return fed;
}

void ReadingQueue::TakeBackFromSinkLocked() {
    const double pending = sink_.PendingSeconds();
    // The words not yet played are the last ones whose length adds up to what the sink still has
    // pending; the oldest of them may be half heard (it is taken back whole).
    size_t count = 0;
    double sum = 0;
    while (count < sinkPieces_.size() && sum < pending - kEpsilon) {
        sum += sinkPieces_[sinkPieces_.size() - 1 - count].Seconds();
        ++count;
    }
    for (size_t i = 0; i < count; ++i) {
        bufferedSeconds_ += sinkPieces_.back().Seconds();
        buffer_.push_front(std::move(sinkPieces_.back()));
        sinkPieces_.pop_back();
    }
    sinkPieces_.clear();
    sinkPiecesSeconds_ = 0;
}

// -------------------------------------------------------------- the Announcement ---

bool ReadingQueue::PumpAnnouncement(std::unique_lock<std::mutex>& lk) {
    switch (annState_) {
    case AnnState::Waiting: {
        const AnnouncementSpeech speak = annSpeak_;
        const unsigned long long gen = annGen_;
        lk.unlock();
        std::vector<SpeechChunk> audio;
        try {
            speak([&](const SpeechChunk& chunk) {
                {
                    std::lock_guard<std::mutex> guard(m_);
                    if (annGen_ != gen) return false;  // replaced, skipped or stopped meanwhile
                }
                audio.push_back(chunk);
                return true;
            });
        } catch (const std::exception& e) {  // as if it failed: nothing is heard
            LogError("[voice] An Announcement could not be spoken: ", e.what());
            audio.clear();
        } catch (...) {
            LogError("[voice] An Announcement could not be spoken: unknown error.");
            audio.clear();
        }
        lk.lock();
        if (annGen_ != gen) return true;
        annAudio_ = std::move(audio);
        annState_ = AnnState::Ready;
        return true;
    }
    case AnnState::Ready:
        if (annAudio_.empty()) {  // it could not be spoken: nothing to interrupt the Reading for
            EndAnnouncementLocked();
            return true;
        }
        // A playing Reading goes on to its word boundary, which is where the sink runs dry (the
        // queue gives it nothing more). A paused one is already quiet.
        if (!paused_ && sink_.PendingSeconds() > 0) return false;
        BeginAnnouncement(lk);
        return true;
    case AnnState::Playing:
        if (sink_.PendingSeconds() > 0) return false;  // (a paused sink keeps its audio pending)
        EndAnnouncementLocked();
        return true;
    case AnnState::None:
        break;
    }
    return false;
}

void ReadingQueue::BeginAnnouncement(std::unique_lock<std::mutex>& lk) {
    if (paused_ && !queue_.empty()) {
        // The sink holds the Reading's audio, paused. It can't play the Announcement behind that,
        // so take the audio back (the Reading resumes from the start of the word it was paused in).
        TakeBackFromSinkLocked();
        paused_ = false;
        heldPause_ = true;
        sink_.Stop();  // also clears the sink's pause
    } else if (paused_) {
        paused_ = false;
        sink_.Resume();
    }
    annState_ = AnnState::Playing;
    const std::vector<SpeechChunk> audio = std::move(annAudio_);
    annAudio_.clear();
    const unsigned long long gen = annGen_;
    lk.unlock();
    for (const SpeechChunk& chunk : audio) {
        sink_.Write(chunk.samples.data(), chunk.samples.size(), chunk.sampleRate);
    }
    lk.lock();
    // A Skip, Stop or newer Announcement came in during the Writes: whatever landed after its
    // cut-off must go. Only the speech thread writes, so cutting the sink off again is safe.
    if (annGen_ != gen) sink_.Stop();
}

void ReadingQueue::EndAnnouncementLocked() {
    annState_ = AnnState::None;
    annSpeak_ = nullptr;
    annAudio_.clear();
    if (heldPause_) {  // the Reading was paused before: it is again, and the person resumes it
        heldPause_ = false;
        paused_ = true;
        sink_.Pause();
    } else if (paused_ && queue_.empty()) {
        // A Pause that came just as the Announcement ended, with no Reading behind it, has
        // nothing left to hold: it must not swallow the next Pick (as in AdvanceLocked).
        paused_ = false;
        sink_.Resume();
    }
    ReportActiveLocked();
}

void ReadingQueue::DropAnnouncementLocked() {
    const bool playing = annState_ == AnnState::Playing;
    ++annGen_;
    annState_ = AnnState::None;
    annSpeak_ = nullptr;
    annAudio_.clear();
    heldPause_ = false;
    if (playing) sink_.Stop();  // also clears the sink's pause
    if (paused_) {              // Skip resumes a paused queue
        paused_ = false;
        if (!playing) sink_.Resume();
    }
    ReportActiveLocked();
}

// --------------------------------------------------------------------- the pump ---

bool ReadingQueue::Pump() {
    std::unique_lock<std::mutex> lk(m_);
    if (annState_ != AnnState::None) return PumpAnnouncement(lk);
    if (queue_.empty()) return false;

    if (speaking_) {
        const unsigned long long feedEpoch = epoch_;
        const bool fed = FeedLocked(lk, feedEpoch);
        // A Stop or Skip came in during a Write: it has already moved the queue on.
        if (epoch_ != feedEpoch) return true;
        // Heard to the end? Then on to the next Block.
        if (annState_ != AnnState::None || !buffer_.empty() || sink_.PendingSeconds() > 0.0) {
            return fed || annState_ != AnnState::None;
        }
        speaking_ = false;
        sinkPieces_.clear();
        sinkPiecesSeconds_ = 0;
        AdvanceLocked();
        return true;
    }
    if (paused_) return false;

    const std::string text = queue_.front()[block_];
    if (Trim(text).empty()) {
        AdvanceLocked();
        return true;
    }
    speaking_ = true;
    const unsigned long long epoch = epoch_;
    lk.unlock();

    try {
        synthesize_(text, [&](const SpeechChunk& chunk) {
            std::unique_lock<std::mutex> l(m_);
            if (epoch_ != epoch) return false;
            AppendChunkLocked(chunk);
            FeedLocked(l, epoch);
            // Synthesis is faster than speech: stay a few seconds ahead, not a whole Block. An
            // Announcement is waiting for this Block's synthesis to end (the Voice makes one thing
            // at a time), so then there is no waiting.
            while (epoch_ == epoch && annState_ == AnnState::None &&
                   bufferedSeconds_ + sink_.PendingSeconds() > aheadSeconds_) {
                l.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                l.lock();
                if (epoch_ != epoch) return false;
                FeedLocked(l, epoch);
            }
            return epoch_ == epoch;
        });
    } catch (const std::exception& e) {  // as if it failed (an uncaught one would end Lexia)
        LogError("[voice] A Block could not be spoken: ", e.what());
    } catch (...) {
        LogError("[voice] A Block could not be spoken: unknown error.");
    }
    // Whatever the result (done, cancelled, failed or thrown), the Block is over once its audio,
    // if any, has played (the next Pump feeds it and sees the end); a cut-off in the meantime has
    // already reset the state.
    return true;
}

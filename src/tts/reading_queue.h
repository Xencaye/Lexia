#pragma once
// The Reading queue: the Readings not yet finished, spoken Block by Block in Pick order, with
// Pause, Skip and Stop, and the Announcements that interrupt it. Pure logic over an AudioSink
// and a synthesiser it is given, so the tests drive it with fakes. Win32-free.
//
// Two kinds of caller: the UI side calls Add / Announce / Pause / Resume / Skip / Stop /
// SessionStarted, and one speech thread calls Pump over and over. Every method is safe from
// any thread. All synthesis happens inside Pump, so it is one thread's work, as Piper needs.
// A synthesiser or Announcement that throws is taken as one that failed (logged): an exception
// never leaves Pump.
//
// While the Reading is paused the sink is given nothing more: the words it does not hold yet wait
// in the queue. (A sink's Write returns at once, also while paused and when the sample rate
// changes (AudioSink), so the speech thread is never stuck there and an Announcement can play.)
//
// Announcements (B3). A short status message spoken by Lexia, over the Reading:
//  - The Reading is held at a word boundary. The queue gives the sink the Block's audio one word
//    at a time and never more than `sinkSeconds` ahead; the rest of the Block waits in the queue.
//    So when an Announcement comes, what the sink holds ends at a word boundary: the Reading goes
//    quiet there (at most `sinkSeconds` plus one word later), the Announcement plays, and the
//    Reading carries on with the next word. Nothing is synthesised again and nothing is heard
//    twice. (Words are the runs of phonemes between Piper's ' ' phonemes, B1.) A chunk without
//    phoneme timings is one word.
//  - If the Reading is paused, it stays paused: the Announcement still plays, and when the person
//    resumes, the Reading goes on from the start of the word it was paused in (the sink cannot
//    play an Announcement while holding the Reading's audio, so the queue takes that audio back).
//  - The newest Announcement replaces any waiting or playing one.
//  - Skip ends only the Announcement (the Reading then resumes, as Skip resumes a paused queue).
//    Stop cuts it off too. A Session starting while paused empties everything, Announcement too.
//  - Pause and Resume act on what is audible: the Announcement while it plays. A Pause that
//    comes just as an Announcement with no Reading behind it ends has nothing to hold: it ends too.
//  - Active() is true while an Announcement waits or plays, even with no Reading, so the keys that
//    follow Active() stay registered.
//
// The heard word (G1, read-along). Each word piece the queue cuts from a chunk carries the id of
// its Reading, its Block and the byte range of the word in the Block's text (read_along.h:
// SpokenWordRanges). The pieces the sink holds (`sinkPieces_`) are in playing order, so the front
// one that is not yet played is the word being heard: HeardWordNow() says which. The tags travel
// with the piece, so a piece taken back from the sink for an Announcement keeps them. Nothing is
// heard while an Announcement plays, when the queue is idle, after Skip or Stop (they empty the
// sink's pieces), and for a piece with no word (silence, punctuation) or a range that could not be
// worked out. A paused Reading still has its word: it is the one the sink holds.
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "read_along.h"
#include "tts/audio_sink.h"
#include "tts/speech_chunk.h"

// One Reading: the text of each Block of one Pick, in order.
using Reading = std::vector<std::string>;

// Speaks one Block: hands the audio to `onChunk` as it is made and stops when `onChunk` returns
// false (then returns Cancelled). Called on the speech thread, with no lock held. Throwing counts
// as Failed.
using BlockSynthesizer = std::function<SynthResult(
    const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk)>;

// Speaks an Announcement: the same contract as BlockSynthesizer, but the caller has already
// chosen the text and the Voice.
using AnnouncementSpeech =
    std::function<SynthResult(const std::function<bool(const SpeechChunk&)>& onChunk)>;

class ReadingQueue {
public:
    // `aheadSeconds`: how much audio may be synthesised and not yet heard (in the queue and the
    // sink together) before synthesis waits for playback to catch up (synthesis is much faster
    // than speech). `sinkSeconds`: how much of that the sink may hold, handed to it a word at a
    // time; the rest waits in the queue so that an Announcement can interrupt at a word boundary.
    // Negative = same as `aheadSeconds` (the sink may hold it all).
    ReadingQueue(AudioSink& sink, BlockSynthesizer synthesize, double aheadSeconds = 3.0,
                 double sinkSeconds = -1.0);

    // A new Pick: joins the end of the queue. A Reading with no text in it is ignored. Returns the
    // Reading's id (1, 2, 3 ... never reused), or 0 if it was ignored. The id names the Reading in
    // HeardWordNow(); the Block index there is the index in `reading`.
    uint64_t Add(Reading reading);

    // An Announcement: replaces any other, and is spoken as described above. Ignored if `speak`
    // is empty. The audio is made on the speech thread by `speak`; if that fails (or throws),
    // nothing is heard and the Reading is not disturbed.
    void Announce(AnnouncementSpeech speak);

    // Holds what is audible where it is. Ignored when nothing is being spoken.
    void Pause();
    void Resume();

    // Jumps to the next Block, or past the last Block to the next Reading. A paused queue
    // carries on playing from there. While an Announcement is waiting or playing, ends only that.
    void Skip();

    // Cuts off the Block and the Announcement being spoken and empties the queue (a pause ends too).
    void Stop();

    // Takes one Reading out of the queue (K11: the Settings page's card button stops its own Reading and
    // nothing else). If it is the one being spoken it is cut off (a pause ends too, like Stop) and the next
    // Reading starts at its first Block; if it is still waiting it is just dropped; an id that is not in the
    // queue (played, stopped, never added, 0) changes nothing. An Announcement is left alone: one that
    // plays is not cut off, and a pause held over it for this Reading ends with the Reading.
    void StopReading(uint64_t id);

    // A Session starts: a playing queue is kept, a paused one is emptied like Stop.
    void SessionStarted();

    bool Active() const;  // something is being spoken or paused: a Reading is queued or an Announcement is
    bool Paused() const;  // the Reading is paused (also while an Announcement plays over it)

    // Is the Reading with this id still in the queue (waiting, being spoken or paused)? False once its
    // last Block has been played, and after Stop. Any thread. The click-through layer (G2) ends when
    // none of its Readings is.
    bool HasReading(uint64_t id) const;

    // The word that is audible now (or, while paused, the one the pause holds), or nothing: see
    // "The heard word" above. Any thread; cheap (the Overlay asks every 30 ms), and it does not
    // change the queue.
    std::optional<HeardWord> HeardWordNow() const;

    // Told whenever Active() changes (true = the queue just got something, false = it just
    // emptied), on whichever thread made the change and with the queue's lock held. So it must be
    // quick and must not call back into the queue: the App's one just posts a window message.
    // A change is reported once; Pause and Resume are not changes. Setting it reports nothing
    // for the state the queue is already in. nullptr removes it.
    void SetActiveListener(std::function<void(bool active)> listener);

    // Speech thread: does the next piece of work, if there is one. Starting a Block blocks
    // until it has been synthesised (the first words are already playing by then); noticing that
    // a Block has been heard is instant. False when there is nothing to do right now (empty,
    // paused, or the current Block is still playing), so the caller waits a moment.
    bool Pump();

private:
    // A stretch of audio handed to the sink in one piece: a word, or a whole chunk without timings.
    struct Piece {
        std::vector<float> samples;
        int sampleRate = 0;
        // Which word it is, for HeardWordNow (set only when the word's place in the text is known).
        bool hasWord = false;
        HeardWord word;
        double Seconds() const { return static_cast<double>(samples.size()) / sampleRate; }
    };
    // A Reading in the queue, with its id.
    struct QueuedReading {
        Reading blocks;
        uint64_t id = 0;
    };
    enum class AnnState {
        None,
        Waiting,  // not synthesised yet
        Ready,    // synthesised; waiting for the Reading to reach a word boundary
        Playing,
    };

    bool ActiveLocked() const { return !queue_.empty() || annState_ != AnnState::None; }
    // Stop with m_ already held.
    void StopLocked();
    // Drops the current Block and moves to the next one (nothing to drop if the queue is empty).
    // Caller holds m_.
    void AdvanceLocked();
    // Cuts off the Reading's audio and, unless `keepPlayingAnnouncement` is set and an Announcement is
    // playing, whatever the sink holds (and its pause). Caller holds m_.
    void CutOffLocked(bool keepPlayingAnnouncement = false);
    // Tells the listener if Active() differs from what it was last told. Caller holds m_.
    void ReportActiveLocked();

    // Splits a chunk into words and puts them at the end of buffer_, each tagged with the word of the
    // Block's text it says. Caller holds m_.
    void AppendChunkLocked(const SpeechChunk& chunk);
    // The Block that is being synthesised starts: forgets where its chunks were in its text.
    void StartBlockTextLocked();
    // Gives the sink words while it holds less than sinkSeconds_ (and no Announcement is due, and
    // the Reading is not paused). Caller holds `lk`; it is released around each Write. True if it
    // wrote anything. Stops the sink again if a cut-off came during a Write.
    bool FeedLocked(std::unique_lock<std::mutex>& lk, unsigned long long epoch);
    // Forgets words in sinkPieces_ that the sink has played. Caller holds m_.
    void PruneSinkPiecesLocked();
    // Takes back the words the sink has not finished playing (the one being heard included), to
    // the front of buffer_. Caller holds m_.
    void TakeBackFromSinkLocked();

    // Pump's work while an Announcement exists. Caller holds `lk`.
    bool PumpAnnouncement(std::unique_lock<std::mutex>& lk);
    void BeginAnnouncement(std::unique_lock<std::mutex>& lk);
    // It has been heard, or had nothing to say. Caller holds m_.
    void EndAnnouncementLocked();
    // Skip: ends the Announcement on the person's request, and lets the Reading resume.
    void DropAnnouncementLocked();

    AudioSink& sink_;
    BlockSynthesizer synthesize_;
    const double aheadSeconds_;
    const double sinkSeconds_;

    mutable std::mutex m_;
    std::vector<QueuedReading> queue_;  // front() is the Reading being spoken
    uint64_t nextId_ = 1;               // the id of the next Reading added
    size_t block_ = 0;                  // index of its current Block
    // Where the chunks of the Block being synthesised are in its text (AppendChunkLocked): the
    // stretches' byte ranges (found with the first chunk), the stretch the last chunk belonged to
    // and how many sentences of it have come. Reset by StartBlockTextLocked.
    bool stretchesKnown_ = false;
    std::vector<TextRange> stretches_;
    size_t stretchOffset_ = 0;
    size_t sentence_ = 0;
    bool speaking_ = false;       // the current Block has been started
    bool paused_ = false;
    std::deque<Piece> buffer_;      // the current Block's audio not yet given to the sink
    double bufferedSeconds_ = 0;    // its length
    std::deque<Piece> sinkPieces_;  // given to the sink, not all of it played yet (oldest first)
    double sinkPiecesSeconds_ = 0;  // its length

    AnnState annState_ = AnnState::None;
    AnnouncementSpeech annSpeak_;
    std::vector<SpeechChunk> annAudio_;
    unsigned long long annGen_ = 0;  // bumped whenever an Announcement is replaced or ends early
    bool heldPause_ = false;         // the Reading was paused when the Announcement began: it stays so

    std::function<void(bool)> activeListener_;
    bool reportedActive_ = false;   // what the listener was last told (or the state when it was set)
    unsigned long long epoch_ = 0;  // bumped by every cut-off; work for an older one is dropped
};

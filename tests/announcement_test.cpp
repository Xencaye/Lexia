// Unit tests for the Announcements in the Reading queue (src/tts/reading_queue.h, B3): a short
// status message that holds the Reading at a word boundary and lets it carry on afterwards. Fake
// sink, fake Voice, one thread (Pump called by the test), except the one test about synthesis
// that is waiting for playback.
//
// Audio is marked so the tests can say what was heard and in which order: every word is one
// second at 100 Hz, filled with a marker value. The Block "one" is the words 11 12 13 14, "two" is
// 21 22 23 24; an Announcement is the single word 91 (or 92, 93).
#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "fake_audio_sink.h"
#include "tts/reading_queue.h"

namespace {

using Strings = std::vector<std::string>;
using Ints = std::vector<int>;

// A sink that remembers the marker of every Write, in order.
class RecordingSink : public FakeAudioSink {
public:
    void Write(const float* s, size_t n, int rate) override {
        if (n > 0) heard.push_back(static_cast<int>(s[0] * 100.f + 0.5f));
        FakeAudioSink::Write(s, n, rate);
    }
    Ints heard;  // markers, in the order they were written
};

// One chunk of `words` words, one second each, marked firstMarker, firstMarker + 1, ... Between
// two words there is a ' ' phoneme (Piper's word separator) which starts the later word's piece.
SpeechChunk Words(int firstMarker, int words) {
    SpeechChunk c;
    c.sampleRate = 100;
    for (int w = 0; w < words; ++w) {
        c.samples.insert(c.samples.end(), 100, static_cast<float>(firstMarker + w) / 100.f);
        if (w > 0) c.phonemes.push_back({U' ', w * 100, 10});
        c.phonemes.push_back({U'a', w * 100 + (w > 0 ? 10 : 0), w > 0 ? 90 : 100});
    }
    return c;
}

struct Fixture {
    RecordingSink sink;
    int chunksOfBlock = 1;                       // a Block's four words come in this many chunks
    std::function<void()> duringBlock;           // runs before the Block's second chunk
    bool blockRefused = false;
    int chunksAccepted = 0;
    ReadingQueue queue;

    explicit Fixture(double aheadSeconds = 1e9, double sinkSeconds = 1.0)
        : queue(sink, [this](const std::string& text, const std::function<bool(const SpeechChunk&)>& on) {
              const int base = text == "one" ? 11 : 21;
              const int per = 4 / chunksOfBlock;
              for (int i = 0; i < chunksOfBlock; ++i) {
                  if (i == 1 && duringBlock) duringBlock();
                  if (!on(Words(base + i * per, per))) {
                      blockRefused = true;
                      return SynthResult::Cancelled;
                  }
                  ++chunksAccepted;
              }
              return SynthResult::Done;
          }, aheadSeconds, sinkSeconds) {}

    // Pumps until it has nothing to do without the sound card playing.
    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
    void Hear() {  // plays everything the sink holds to the end, then settles
        sink.Drain();
        Settle();
    }

    // An Announcement of one word with this marker; counts how often it was asked to speak.
    AnnouncementSpeech Say(int marker, int* spoken = nullptr) {
        return [marker, spoken](const std::function<bool(const SpeechChunk&)>& on) {
            if (spoken) ++*spoken;
            return on(Words(marker, 1)) ? SynthResult::Done : SynthResult::Cancelled;
        };
    }
};

struct Reports {
    std::vector<bool> seen;
    void Attach(ReadingQueue& q) {
        q.SetActiveListener([this](bool active) { seen.push_back(active); });
    }
};

}  // namespace

// ---------------------------------------------------- the Reading goes word by word ---

TEST_CASE("Announcements: the sink gets a Block one word at a time, a little ahead") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    CHECK(f.sink.heard == Ints{11});  // the sink holds one second: that is the limit
    f.sink.Advance(0.5);
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 12});  // under the limit again: the next word
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12, 13, 14});
    f.Hear();
    CHECK_FALSE(f.queue.Active());  // the Block ends once its last word has been heard
}

TEST_CASE("Announcements: the next Block starts only when every word of this one is heard") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12, 13, 14});  // 14 is playing: "two" has not started
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12, 13, 14, 21});
}

// ------------------------------------------------------ pausing at a word boundary ---

TEST_CASE("Announcements: the Reading goes quiet at the next word boundary, the Announcement plays, "
          "the Reading goes on with the next word") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();  // word 11 is playing
    f.sink.Advance(0.4);
    f.queue.Announce(f.Say(91));
    f.Settle();
    // The word being heard is finished first; the Announcement waits for it.
    CHECK(f.sink.heard == Ints{11});
    CHECK(f.sink.stops == 0);  // nothing is cut off mid-word
    f.sink.Advance(0.6);       // 11 has been heard: a word boundary
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91});  // the Announcement, not word 12
    f.Hear();                              // the Announcement is over
    CHECK(f.sink.heard == Ints{11, 91, 12});  // on with the next word: none repeated, none lost
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 12, 13, 14});
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Announcements: nothing is synthesised again for the Reading to resume") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();
    f.Hear();
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 12, 13, 14});
    CHECK(f.chunksAccepted == 1);  // the Block was synthesised once
}

TEST_CASE("Announcements: a Block that ends while an Announcement waits lets it play before the next Block") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.Hear();
    f.Hear();
    f.Hear();  // 14, the last word of the Block, is written
    CHECK(f.sink.heard == Ints{11, 12, 13, 14});
    f.queue.Announce(f.Say(91));
    f.Hear();  // 14 heard: the boundary
    CHECK(f.sink.heard == Ints{11, 12, 13, 14, 91});
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12, 13, 14, 91, 21});
}

TEST_CASE("Announcements: a chunk without timings is one word") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce([](const std::function<bool(const SpeechChunk&)>& on) {
        SpeechChunk c = Words(91, 3);  // three words, but the timings are missing
        c.phonemes.clear();
        return on(c) ? SynthResult::Done : SynthResult::Cancelled;
    });
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 12});
    CHECK(f.sink.TotalWrittenSeconds() == doctest::Approx(1.0 + 3.0 + 1.0));  // 91 went in as one piece
}

TEST_CASE("Announcements: with no Reading, an Announcement just plays") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Announce(f.Say(91));
    CHECK(f.queue.Active());  // the Pause and Speech keys are wanted from now on
    CHECK(r.seen == std::vector<bool>{true});
    f.Settle();
    CHECK(f.sink.heard == Ints{91});
    f.Hear();
    CHECK_FALSE(f.queue.Active());
    CHECK(r.seen == (std::vector<bool>{true, false}));
}

TEST_CASE("Announcements: an Announcement over a Reading adds no busy / idle reports") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();
    f.Hear();
    CHECK(r.seen == std::vector<bool>{true});  // busy since the Reading began, idle not yet
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(r.seen == (std::vector<bool>{true, false}));
}

TEST_CASE("Announcements: one that cannot be spoken leaves the Reading alone") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce([](const std::function<bool(const SpeechChunk&)>&) { return SynthResult::Failed; });
    f.Settle();
    f.sink.Advance(0.5);
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 12});  // no wait for a boundary, no cut
    CHECK(f.sink.stops == 0);
}

TEST_CASE("Announcements: one whose Voice throws is not heard and leaves the Reading alone") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce([](const std::function<bool(const SpeechChunk&)>&) -> SynthResult {
        throw std::runtime_error("the Voice broke");
    });
    f.Settle();  // the throw must not leave Pump
    f.sink.Advance(0.5);
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 12});
    CHECK(f.sink.stops == 0);
    f.queue.Announce(f.Say(91));  // the next one is heard as usual
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12, 91});
}

TEST_CASE("Announcements: a failed one with no Reading ends at once") {
    Fixture f;
    f.queue.Announce([](const std::function<bool(const SpeechChunk&)>&) { return SynthResult::Failed; });
    f.Settle();
    CHECK_FALSE(f.queue.Active());
}

// ------------------------------------------------------------------------ paused ---

TEST_CASE("Announcements: over a paused Reading the Announcement plays and the Reading stays paused") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.sink.Advance(0.4);  // word 11 is 40% heard
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91});  // plays at once: a paused Reading is already quiet
    CHECK(f.queue.Paused());              // ... and is still paused
    CHECK_FALSE(f.sink.paused);           // the sink must play the Announcement
    f.Hear();
    CHECK(f.queue.Paused());
    CHECK(f.sink.paused);                 // paused again, as the person left it
    CHECK(f.sink.heard == Ints{11, 91});  // the word it was in waits in the queue, not in the sink
    f.sink.Drain();
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91});  // nothing moves on while paused
    CHECK(f.sink.writesWhilePaused == 0);
}

TEST_CASE("Announcements: Resume after an Announcement over a pause goes on from the start of the word") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.sink.Advance(0.4);
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();  // the Announcement is playing
    f.Hear();    // ... and heard: the Reading is paused again, word 11 waiting at the start
    f.queue.Resume();
    f.Settle();  // word 11 goes to the sink
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    CHECK(f.sink.PendingSeconds() == doctest::Approx(1.0));  // word 11 again, whole
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 11, 12, 13});
}

// R39: StopReading of the Reading an Announcement plays over. The Announcement is left alone (its
// audio is the only thing in the sink), and the pause that the stopped Reading held ends with it.
TEST_CASE("Announcements: StopReading over a paused Reading leaves the Announcement and does not keep the "
          "next Reading silent") {
    Fixture f;
    const uint64_t a = f.queue.Add({"one"});
    const uint64_t b = f.queue.Add({"two"});
    f.Settle();
    f.sink.Advance(0.4);
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();  // the Announcement plays over the paused Reading
    REQUIRE(f.sink.heard == Ints{11, 91});
    f.queue.StopReading(a);
    CHECK(f.sink.PendingSeconds() == doctest::Approx(1.0));  // the Announcement was not cut off
    CHECK_FALSE(f.sink.paused);
    CHECK_FALSE(f.queue.Paused());  // the pause belonged to Reading a
    f.Hear();                       // the Announcement ends ...
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.queue.HasReading(b));
    f.Settle();                     // ... and Reading b is spoken, not held
    CHECK(f.sink.heard == Ints{11, 91, 21});
}

TEST_CASE("Announcements: StopReading of the last Reading while an Announcement plays over its pause "
          "leaves nothing paused") {
    Fixture f;
    const uint64_t a = f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.StopReading(a);
    CHECK(f.queue.Active());  // the Announcement still plays
    f.Hear();
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());  // F7 is not registered with the queue empty: nothing may be held
    f.queue.Add({"two"});
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 21});
}

TEST_CASE("Announcements: StopReading over an unpaused Reading does not cut the Announcement off") {
    Fixture f;
    const uint64_t a = f.queue.Add({"one"});
    f.queue.Add({"two"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.sink.Advance(1.0);  // word 11 heard: the boundary
    f.Settle();
    REQUIRE(f.sink.heard == Ints{11, 91});
    f.queue.StopReading(a);
    CHECK(f.sink.PendingSeconds() == doctest::Approx(1.0));
    f.Hear();
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 21});
}

TEST_CASE("Announcements: StopReading keeps the person's pause on the Announcement itself") {
    Fixture f;
    const uint64_t a = f.queue.Add({"one"});
    f.Settle();
    f.sink.Advance(1.0);
    f.queue.Announce(f.Say(91));
    f.Settle();  // the Announcement plays over the Reading's end
    f.queue.Pause();  // ... and the person pauses the Announcement
    REQUIRE(f.sink.paused);
    f.queue.StopReading(a);
    CHECK(f.sink.paused);  // still holding the Announcement
    CHECK(f.queue.Paused());
    f.queue.Resume();
    f.Hear();
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Announcements: Pause while an Announcement waits makes it play at once") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Settle();  // synthesised, waiting for the boundary
    CHECK(f.sink.heard == Ints{11});
    f.sink.Advance(0.3);
    f.queue.Pause();  // the Reading is now frozen mid-word: no boundary will come
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91});
    CHECK(f.queue.Paused());
}

TEST_CASE("Announcements: Resume while an Announcement plays over a pause lets the Reading go on after it") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.Resume();  // the Announcement is already audible: nothing to resume yet
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.resumes == 0);
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 11, 12});
}

TEST_CASE("Announcements: Pause during an Announcement holds the Announcement") {
    Fixture f;
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.Pause();
    CHECK(f.sink.paused);
    f.sink.Drain();  // a paused sink plays nothing
    f.Settle();
    CHECK(f.queue.Active());
    f.queue.Resume();
    f.Hear();
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Announcements: a paused Reading gives the sink nothing more, so an Announcement can still play") {
    // A sink may wait inside Write while it is paused (WaveOutSink does when the next word has
    // another sample rate), and then the speech thread could not make the Announcement.
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();           // word 11 in the sink
    f.sink.Advance(0.4);  // room in the sink for word 12
    f.queue.Pause();
    f.Settle();
    CHECK(f.sink.writesWhilePaused == 0);  // word 12 waits in the queue
    CHECK(f.sink.heard == Ints{11});
    f.queue.Announce(f.Say(91));
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91});
    f.Hear();  // heard: the Reading is paused again
    CHECK(f.queue.Paused());
    CHECK(f.sink.writesWhilePaused == 0);
    f.queue.Resume();
    f.Settle();  // from the start of word 11
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 11, 12});
}

TEST_CASE("Announcements: a Pause as the last Announcement ends with no Reading does not swallow the next Pick") {
    Fixture f;
    f.queue.Announce(f.Say(91));
    f.Settle();       // 91 is playing
    f.sink.Drain();   // heard, but Pump has not noticed yet
    f.queue.Pause();  // still Playing, so the Pause is taken
    f.Settle();       // Pump notices: nothing is left
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.queue.Add({"one"});
    f.Settle();
    CHECK(f.sink.heard == Ints{91, 11});
}

TEST_CASE("Announcements: Pause with only an Announcement waiting is ignored") {
    Fixture f;
    f.queue.Announce(f.Say(91));
    f.queue.Pause();  // nothing audible to hold
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.pauses == 0);
}

// ------------------------------------------------------------------- replacement ---

TEST_CASE("Announcements: the newest replaces one that is waiting") {
    Fixture f;
    int first = 0, second = 0;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91, &first));
    f.queue.Announce(f.Say(92, &second));
    f.Hear();
    CHECK(first == 0);
    CHECK(second == 1);
    CHECK(f.sink.heard == Ints{11, 92});
}

TEST_CASE("Announcements: the newest replaces one that is ready") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Settle();  // synthesised, waiting for the boundary
    f.queue.Announce(f.Say(92));
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 92});
}

TEST_CASE("Announcements: the newest cuts off one that is playing") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();  // 91 is playing
    REQUIRE(f.sink.heard == Ints{11, 91});
    f.queue.Announce(f.Say(92));
    CHECK(f.sink.stops == 1);  // the old one is cut off
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 92});
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 92, 12});  // and the Reading still resumes where it was
}

TEST_CASE("Announcements: the newest replaces one that is being synthesised") {
    Fixture f;
    int second = 0;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce([&](const std::function<bool(const SpeechChunk&)>& on) {
        f.queue.Announce(f.Say(92, &second));  // arrives while the first is being made
        return on(Words(91, 1)) ? SynthResult::Done : SynthResult::Cancelled;
    });
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 92, 12});  // the first one was dropped before it was heard
    CHECK(second == 1);
}

// -------------------------------------------------------------------------- Skip ---

TEST_CASE("Announcements: Skip while one is playing ends only the Announcement") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();
    REQUIRE(f.sink.heard == Ints{11, 91});
    f.queue.Skip();
    CHECK(f.sink.stops == 1);  // the Announcement is cut off
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 12});  // the same Block goes on, not "two"
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 12, 13, 14, 21});
}

TEST_CASE("Announcements: Skip while one waits drops it and leaves the Reading playing") {
    Fixture f;
    int spoken = 0;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91, &spoken));
    f.queue.Skip();
    CHECK(f.sink.stops == 0);  // the word being heard is not touched
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12});
    CHECK(spoken == 0);
}

TEST_CASE("Announcements: Skip while one waits over a paused Reading resumes the Reading") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.queue.Skip();  // before the Announcement began
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 12});
}

TEST_CASE("Announcements: Skip while one plays over a paused Reading resumes the Reading from the word start") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.sink.Advance(0.4);
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.Skip();
    CHECK_FALSE(f.queue.Paused());
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 11});
    CHECK_FALSE(f.sink.paused);
    CHECK(f.sink.PendingSeconds() == doctest::Approx(1.0));
}

TEST_CASE("Announcements: Skip with only an Announcement ends the queue's activity") {
    Fixture f;
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.Skip();
    CHECK_FALSE(f.queue.Active());
    CHECK(f.sink.PendingSeconds() == 0.0);
}

// -------------------------------------------------------------------------- Stop ---

TEST_CASE("Announcements: Stop cuts off the Announcement and empties the queue") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();
    REQUIRE(f.sink.heard == Ints{11, 91});
    f.queue.Stop();
    CHECK_FALSE(f.queue.Active());
    CHECK(f.sink.PendingSeconds() == 0.0);
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91});  // nothing more is spoken
}

TEST_CASE("Announcements: Stop drops an Announcement that is still waiting") {
    Fixture f;
    int spoken = 0;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91, &spoken));
    f.queue.Stop();
    f.Hear();
    CHECK(spoken == 0);
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Announcements: Stop ends a pause held over an Announcement") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.Stop();
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.queue.Add({"two"});
    f.Settle();
    CHECK(f.sink.heard == Ints{11, 91, 21});
}

TEST_CASE("Announcements: a Session starting while paused empties the queue, Announcement included") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Announce(f.Say(91));
    f.Settle();
    f.queue.SessionStarted();
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.PendingSeconds() == 0.0);
}

TEST_CASE("Announcements: a Session starting while playing keeps the Announcement") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Announce(f.Say(91));
    f.Hear();
    f.queue.SessionStarted();
    CHECK(f.queue.Active());
    CHECK(f.sink.stops == 0);
}

// ------------------------------------------------ synthesis waiting for playback ---

TEST_CASE("Announcements: one that arrives while a Block waits for playback does not wait for the whole Block") {
    // The speech thread is inside the Block's synthesis, held back because it is far enough ahead.
    // The Announcement can only be made once that synthesis is over, so the hold-back ends.
    Fixture f(/*aheadSeconds=*/1.0, /*sinkSeconds=*/1.0);
    f.chunksOfBlock = 4;  // four chunks of one word
    f.queue.Add({"one"});
    std::thread speech([&] { f.queue.Pump(); });

    auto waitFor = [](const std::function<bool()>& condition) {
        for (int i = 0; i < 500; ++i) {  // up to 5 s
            if (condition()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    };
    REQUIRE(waitFor([&] { return f.sink.writes == 1; }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(f.chunksAccepted == 1);  // waiting for playback: nothing further is made

    f.queue.Announce(f.Say(91));
    const bool finished = waitFor([&] { return f.chunksAccepted == 4; });
    if (!finished) f.queue.Stop();  // let the test end rather than hang
    speech.join();
    REQUIRE(finished);

    f.Settle();  // the Announcement is made
    f.Hear();    // word 11 heard: the boundary
    CHECK(f.sink.heard == Ints{11, 91});
    f.Hear();
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.sink.heard == Ints{11, 91, 12, 13, 14});
}

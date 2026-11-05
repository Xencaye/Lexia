// Unit tests for the Reading queue (src/tts/reading_queue.h): every rule, with a fake sink and a
// fake synthesiser, driven from one thread by calling Pump (the last tests use a second thread).
#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "fake_audio_sink.h"
#include "tts/reading_queue.h"

namespace {

using Strings = std::vector<std::string>;

// Speaks a Block as `chunksPerBlock` chunks of one second each (at 100 Hz, so tests stay small)
// and records the text of every Block it was asked to speak.
struct FakeVoice {
    int chunksPerBlock = 1;
    Strings spoken;
    std::function<void(int chunk)> beforeChunk;  // lets a test act in the middle of a Block
    SynthResult result = SynthResult::Done;
    bool refused = false;  // a chunk was refused (the caller cancelled)
    int chunksAccepted = 0;

    SynthResult operator()(const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
        spoken.push_back(text);
        for (int i = 0; i < chunksPerBlock; ++i) {
            if (beforeChunk) beforeChunk(i);
            SpeechChunk c;
            c.samples.assign(100, 0.f);
            c.sampleRate = 100;
            if (!onChunk(c)) {
                refused = true;
                return SynthResult::Cancelled;
            }
            ++chunksAccepted;
        }
        return result;
    }
};

struct Fixture {
    FakeAudioSink sink;
    FakeVoice voice;
    ReadingQueue queue{
        sink,
        [this](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) { return voice(t, f); },
        /*aheadSeconds=*/1e9};

    // Pumps until it has nothing to do without the sound card playing.
    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
    // Plays everything queued to the end and settles: the next Block starts.
    void Hear() {
        sink.Drain();
        Settle();
    }
};

}  // namespace

TEST_CASE("Reading queue: an empty queue has nothing to do") {
    Fixture f;
    CHECK_FALSE(f.queue.Pump());
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
}

TEST_CASE("Reading queue: a Reading is spoken Block by Block, each once the last was heard") {
    Fixture f;
    f.queue.Add({"one", "two", "three"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one"});
    f.sink.Advance(0.5);  // half heard: the next Block must wait
    f.Settle();
    CHECK(f.voice.spoken.size() == 1);
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one", "two"});
    f.Hear();
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one", "two", "three"});
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Reading queue: a new Pick joins the end and does not interrupt the Reading") {
    Fixture f;
    f.queue.Add({"first A", "first B"});
    f.Settle();
    f.queue.Add({"second"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"first A"});
    CHECK(f.sink.stops == 0);
    f.Hear();
    f.Hear();
    f.Hear();
    CHECK(f.voice.spoken == Strings{"first A", "first B", "second"});
}

TEST_CASE("Reading queue: a Reading with no text is ignored, blank Blocks are skipped") {
    Fixture f;
    f.queue.Add({});
    f.queue.Add({"  ", ""});
    CHECK_FALSE(f.queue.Active());
    f.queue.Add({"", "real", " "});
    f.Settle();
    f.Hear();
    f.Hear();
    CHECK(f.voice.spoken == Strings{"real"});
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Reading queue: a Block that failed to synthesise does not stop the queue") {
    Fixture f;
    f.voice.chunksPerBlock = 0;
    f.voice.result = SynthResult::Failed;
    f.queue.Add({"bad", "worse"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"bad", "worse"});
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Reading queue: a Voice that throws ends its Block, and the queue goes on") {
    Fixture f;
    f.voice.chunksPerBlock = 2;
    f.voice.beforeChunk = [&](int chunk) {
        if (chunk == 1 && f.voice.spoken.back() == "bad") throw std::runtime_error("the Voice broke");
    };
    f.queue.Add({"bad", "good"});
    f.Settle();  // the throw must not leave Pump (on the speech thread it would end Lexia)
    CHECK(f.voice.spoken == Strings{"bad"});
    CHECK(f.sink.PendingSeconds() == doctest::Approx(1.0));  // what was made before the throw plays
    f.Hear();
    CHECK(f.voice.spoken == Strings{"bad", "good"});
    CHECK(f.voice.chunksAccepted == 3);
    f.Hear();
    CHECK_FALSE(f.queue.Active());
}

// ----------------------------------------------------------------------- Pause ---

TEST_CASE("Reading queue: Pause holds the sink and starts nothing new") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.Pause();
    CHECK(f.queue.Paused());
    CHECK(f.sink.paused);
    f.Hear();  // a paused sink plays nothing, so the Block never ends
    CHECK(f.voice.spoken == Strings{"one"});
    CHECK(f.queue.Active());
}

TEST_CASE("Reading queue: Resume carries on with the rest") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.sink.Advance(0.4);
    f.queue.Pause();
    f.queue.Resume();
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    CHECK(f.sink.stops == 0);  // not restarted: the same audio goes on
    CHECK(f.sink.PendingSeconds() == doctest::Approx(0.6));
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one", "two"});
}

TEST_CASE("Reading queue: Pause between two Blocks holds the next one back") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.sink.Drain();  // "one" heard, "two" not started yet
    f.queue.Pause();
    f.Settle();
    CHECK(f.voice.spoken.size() == 1);
    f.queue.Resume();
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "two"});
}

TEST_CASE("Reading queue: Pause with nothing queued is ignored, Resume when not paused too") {
    Fixture f;
    f.queue.Pause();
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.pauses == 0);
    f.queue.Add({"one"});
    f.queue.Resume();
    CHECK(f.sink.resumes == 0);
    f.queue.Pause();
    f.queue.Pause();  // twice is once
    CHECK(f.sink.pauses == 1);
}

TEST_CASE("Reading queue: a Pick added while paused joins the queue and stays paused") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Add({"two"});
    f.Hear();
    CHECK(f.queue.Paused());
    CHECK(f.voice.spoken.size() == 1);
    f.queue.Resume();
    f.Hear();
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one", "two"});
}

// ------------------------------------------------------------------------ Skip ---

TEST_CASE("Reading queue: Skip jumps to the next Block of the Reading") {
    Fixture f;
    f.queue.Add({"one", "two", "three"});
    f.Settle();
    f.queue.Skip();
    CHECK(f.sink.stops == 1);  // "one" is cut off
    CHECK(f.sink.PendingSeconds() == 0.0);
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "two"});
}

TEST_CASE("Reading queue: Skip past the last Block moves on to the next Reading") {
    Fixture f;
    f.queue.Add({"a1", "a2"});
    f.queue.Add({"b1"});
    f.Settle();
    f.queue.Skip();
    f.Settle();
    f.queue.Skip();  // a2 -> b1
    f.Settle();
    CHECK(f.voice.spoken == Strings{"a1", "a2", "b1"});
    f.Hear();
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Reading queue: Skip on the last Block of the last Reading ends the queue") {
    Fixture f;
    f.queue.Add({"only"});
    f.Settle();
    f.queue.Skip();
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Pump());
}

TEST_CASE("Reading queue: Skip while paused plays on from the next Block") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.Pause();
    f.queue.Skip();
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "two"});
    CHECK(f.sink.PendingSeconds() > 0.0);  // "two" is playing
}

TEST_CASE("Reading queue: Skip with nothing queued does nothing") {
    Fixture f;
    f.queue.Skip();
    CHECK(f.sink.stops == 0);
}

TEST_CASE("Reading queue: Skip during synthesis drops the rest of that Block's audio") {
    Fixture f;
    f.voice.chunksPerBlock = 3;
    f.voice.beforeChunk = [&](int chunk) {
        if (chunk == 1) f.queue.Skip();  // another thread presses Skip mid-Block
    };
    f.queue.Add({"one", "two"});
    f.queue.Pump();
    CHECK(f.voice.chunksAccepted == 1);  // the second chunk was refused
    CHECK(f.voice.refused);
    CHECK(f.sink.TotalWrittenSeconds() == doctest::Approx(1.0));
    f.voice.beforeChunk = nullptr;
    f.Settle();  // "two" starts
    CHECK(f.voice.spoken == Strings{"one", "two"});
}

// ------------------------------------------------------------------------ Stop ---

TEST_CASE("Reading queue: Stop cuts off the Block and empties the queue") {
    Fixture f;
    f.queue.Add({"a1", "a2"});
    f.queue.Add({"b1"});
    f.Settle();
    f.queue.Stop();
    CHECK(f.sink.stops == 1);
    CHECK_FALSE(f.queue.Active());
    f.Hear();
    CHECK(f.voice.spoken == Strings{"a1"});
}

TEST_CASE("Reading queue: Stop also ends a pause, and the queue works again afterwards") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Stop();
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.queue.Add({"fresh"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "fresh"});
}

TEST_CASE("Reading queue: Stop during synthesis drops the rest of the audio and the queue") {
    Fixture f;
    f.voice.chunksPerBlock = 3;
    f.voice.beforeChunk = [&](int chunk) {
        if (chunk == 1) f.queue.Stop();
    };
    f.queue.Add({"one", "two"});
    f.queue.Pump();
    CHECK(f.voice.refused);
    CHECK_FALSE(f.queue.Active());
    CHECK(f.sink.PendingSeconds() == 0.0);
    CHECK_FALSE(f.queue.Pump());
}

// ------------------------------------------------ a key pressed during a Write ---
// Pump gives the sink a Block's words with the queue's lock released (a Write may be slow), so a
// Skip or Stop can come in while it writes. The queue must move on once, not twice.

namespace {

// A sink that holds one second, and Blocks of two one-second chunks: the second chunk is given
// to the sink by Pump once the first has been heard, not during synthesis.
struct SmallSinkFixture {
    FakeAudioSink sink;
    FakeVoice voice;
    ReadingQueue queue{
        sink,
        [this](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) { return voice(t, f); },
        /*aheadSeconds=*/1e9, /*sinkSeconds=*/1.0};
    std::function<void()> command;  // to run inside the next Write

    SmallSinkFixture() {
        voice.chunksPerBlock = 2;
        sink.onWrite = [this] {
            if (!command) return;
            const std::function<void()> run = command;
            command = nullptr;
            run();
        };
    }
    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
};

}  // namespace

TEST_CASE("Reading queue: a Skip during a Write skips one Block, not two") {
    SmallSinkFixture f;
    f.queue.Add({"one", "two", "three"});
    f.Settle();
    REQUIRE(f.sink.writes == 1);  // the second chunk of "one" waits in the queue
    f.command = [&] { f.queue.Skip(); };
    f.sink.Drain();
    f.Settle();  // Pump writes the second chunk, and the Skip comes in during that Write
    CHECK(f.voice.spoken == Strings{"one", "two"});  // not "three"
    CHECK(f.queue.Active());
    f.sink.Drain();
    f.Settle();
    f.sink.Drain();
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "two", "three"});
}

TEST_CASE("Reading queue: a Stop during a Write leaves an empty queue that works again") {
    SmallSinkFixture f;
    f.queue.Add({"a1", "a2"});
    f.queue.Add({"b1"});
    f.Settle();
    f.command = [&] { f.queue.Stop(); };
    f.sink.Drain();
    f.Settle();  // before the fix the queue moved on from an empty queue: undefined behaviour
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.PendingSeconds() == 0.0);  // the audio that landed after the Stop was cut off
    f.queue.Add({"next"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"a1", "next"});
}

TEST_CASE("Reading queue: a Skip past the last Block during a Write ends the queue cleanly") {
    SmallSinkFixture f;
    f.queue.Add({"only"});
    f.Settle();
    f.command = [&] { f.queue.Skip(); };
    f.sink.Drain();
    f.Settle();
    CHECK_FALSE(f.queue.Active());
    CHECK(f.sink.PendingSeconds() == 0.0);
    f.queue.Add({"next"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"only", "next"});
}

// ------------------------------------------------------------ busy / idle reports ---
// The App registers the Pause and Speech keys only while the queue is active (ADR-0001), so the
// queue says when it turns busy or idle. One report per change, never for a Pause.

namespace {
struct Reports {
    std::vector<bool> seen;
    void Attach(ReadingQueue& q) {
        q.SetActiveListener([this](bool active) { seen.push_back(active); });
    }
};
}  // namespace

TEST_CASE("Reading queue: busy is reported when a Reading joins an empty queue, once") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one"});
    f.queue.Add({"two"});  // already busy: no new report
    CHECK(r.seen == std::vector<bool>{true});
}

TEST_CASE("Reading queue: a Reading with no text reports nothing") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"  ", ""});
    CHECK(r.seen.empty());
}

TEST_CASE("Reading queue: idle is reported when the last Block has been heard") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one", "two"});
    f.Settle();                  // "one" starts
    f.Hear();                    // "one" heard, "two" starts
    CHECK(r.seen == std::vector<bool>{true});
    f.Hear();                    // "two" heard: nothing left
    CHECK(r.seen == (std::vector<bool>{true, false}));
}

TEST_CASE("Reading queue: Pause and Resume do not report") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Pause();
    f.queue.Resume();
    CHECK(r.seen == std::vector<bool>{true});
}

TEST_CASE("Reading queue: Stop reports idle, and Stop on an empty queue reports nothing") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Stop();
    CHECK(r.seen.empty());
    f.queue.Add({"one"});
    f.Settle();
    f.queue.Stop();
    CHECK(r.seen == (std::vector<bool>{true, false}));
}

TEST_CASE("Reading queue: Skip past the last Block reports idle; Skip to another Block does not") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.Skip();
    CHECK(r.seen == std::vector<bool>{true});
    f.queue.Skip();
    CHECK(r.seen == (std::vector<bool>{true, false}));
}

TEST_CASE("Reading queue: a Session starting is reported only when it empties a paused queue") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.Add({"one"});
    f.Settle();
    f.queue.SessionStarted();
    CHECK(r.seen == std::vector<bool>{true});  // playing: kept, nothing new to report
    f.queue.Pause();
    f.queue.SessionStarted();
    CHECK(r.seen == (std::vector<bool>{true, false}));  // paused: emptied
}

TEST_CASE("Reading queue: a listener set while busy is told only of changes after that") {
    Fixture f;
    f.queue.Add({"one"});
    Reports r;
    r.Attach(f.queue);
    CHECK(r.seen.empty());
    f.queue.Stop();
    CHECK(r.seen == std::vector<bool>{false});
}

TEST_CASE("Reading queue: the listener can be removed") {
    Fixture f;
    Reports r;
    r.Attach(f.queue);
    f.queue.SetActiveListener(nullptr);
    f.queue.Add({"one"});
    CHECK(r.seen.empty());
}

// -------------------------------------------------------------------- Sessions ---

TEST_CASE("Reading queue: a Session starting while playing keeps the queue") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.queue.SessionStarted();
    CHECK(f.sink.stops == 0);
    CHECK(f.queue.Active());
    f.queue.Add({"three"});  // a Pick in the new Session joins the end
    f.Hear();
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one", "two", "three"});
}

TEST_CASE("Reading queue: a Session starting while paused empties the queue") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.queue.Add({"three"});
    f.Settle();
    f.queue.Pause();
    f.queue.SessionStarted();
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK(f.sink.stops == 1);
    f.Hear();
    CHECK(f.voice.spoken == Strings{"one"});
}

TEST_CASE("Reading queue: a Session starting while idle does nothing") {
    Fixture f;
    f.queue.SessionStarted();
    CHECK(f.sink.stops == 0);
    CHECK_FALSE(f.queue.Active());
}

// A Pause that comes just as the last Block has been heard must not outlive the queue.
TEST_CASE("Reading queue: a Pause after the last Block ended does not swallow the next Pick") {
    Fixture f;
    f.queue.Add({"one"});
    f.Settle();
    f.sink.Drain();  // heard, but Pump has not noticed yet: the queue still holds the Block
    f.queue.Pause();
    f.Settle();      // Pump notices: the queue is empty
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    f.queue.Add({"next"});
    f.Settle();
    CHECK(f.voice.spoken == Strings{"one", "next"});
}

// ------------------------------------------------------------------ two threads ---

namespace {

bool WaitFor(const std::function<bool()>& condition) {
    for (int i = 0; i < 500; ++i) {  // up to 5 s
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// A sink whose Write waits until Stop, like WaveOutSink does when the sample rate changes.
class BlockingWriteSink : public FakeAudioSink {
public:
    void Write(const float* s, size_t n, int rate) override {
        std::unique_lock<std::mutex> lk(m);
        entered = true;
        cv.wait(lk, [&] { return stopped; });
        lk.unlock();
        FakeAudioSink::Write(s, n, rate);  // the audio lands after the Stop, as in the real sink
    }
    void Stop() override {
        FakeAudioSink::Stop();
        std::lock_guard<std::mutex> lk(m);
        stopped = true;
        cv.notify_all();
    }
    std::mutex m;
    std::condition_variable cv;
    bool entered = false, stopped = false;
};

}  // namespace

TEST_CASE("Reading queue: Stop, Pause and Skip do not wait behind a sink that blocks in Write") {
    BlockingWriteSink sink;
    FakeVoice voice;
    ReadingQueue queue{
        sink,
        [&](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) { return voice(t, f); },
        1e9};
    queue.Add({"one", "two"});
    std::thread speech([&] { queue.Pump(); });  // blocks inside sink.Write
    REQUIRE(WaitFor([&] {
        std::lock_guard<std::mutex> lk(sink.m);
        return sink.entered;
    }));

    // Before the fix these took the queue's lock while Write held it: deadlock.
    auto commands = std::async(std::launch::async, [&] {
        queue.Pause();
        queue.Skip();
        queue.Stop();
    });
    const bool done = commands.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (!done) {  // release the sink so the test can end instead of hanging
        sink.Stop();
    }
    CHECK(done);
    speech.join();
    commands.wait();
    CHECK_FALSE(queue.Active());
    CHECK(sink.PendingSeconds() == 0.0);  // the audio that landed after Stop was cut off again
}

TEST_CASE("Reading queue: synthesis stays a few seconds ahead of playback") {
    FakeAudioSink sink;
    FakeVoice voice;
    voice.chunksPerBlock = 4;  // four one-second chunks
    ReadingQueue queue{
        sink,
        [&](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) { return voice(t, f); },
        /*aheadSeconds=*/1.5};
    queue.Add({"one"});
    std::thread speech([&] { queue.Pump(); });

    REQUIRE(WaitFor([&] { return sink.writes == 2; }));  // 2 s queued is over 1.5 s: waits
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(sink.writes == 2);

    sink.Advance(1.0);  // 1 s left: below the limit, one more chunk
    REQUIRE(WaitFor([&] { return sink.writes == 3; }));
    sink.Advance(5.0);
    REQUIRE(WaitFor([&] { return sink.writes == 4; }));
    speech.join();
    CHECK(voice.chunksAccepted == 4);
}

TEST_CASE("Reading queue: Stop ends the wait for playback to catch up") {
    FakeAudioSink sink;
    FakeVoice voice;
    voice.chunksPerBlock = 4;
    ReadingQueue queue{
        sink,
        [&](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) { return voice(t, f); },
        1.5};
    queue.Add({"one", "two"});
    std::thread speech([&] { queue.Pump(); });
    REQUIRE(WaitFor([&] { return sink.writes == 2; }));
    queue.Stop();
    speech.join();  // returns: the throttle loop saw the Stop
    CHECK(voice.refused);  // the second chunk, written and then waited on, was the last
    CHECK(voice.chunksAccepted == 1);
    CHECK(sink.writes == 2);
    CHECK_FALSE(queue.Active());
}

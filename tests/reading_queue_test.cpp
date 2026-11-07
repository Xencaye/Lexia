// Unit tests for the Reading queue (src/tts/reading_queue.h): every rule, with a fake sink and a
// fake synthesiser, driven from one thread by calling Pump (the last tests use a second thread).
#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
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

// ------------------------------------------------------------ the heard word (G1) ---

namespace {

// A synthesiser whose chunks have phoneme timings: each sentence of the text (cut at '.') is one
// chunk of one second at 100 Hz per word (words are the runs between spaces), 2 phonemes a word,
// with Piper's markers and ' ' phonemes, so the queue can cut words and name them.
BlockSynthesizer WordVoice() {
    return [](const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
        size_t from = 0;
        while (from < text.size()) {
            const size_t dot = text.find('.', from);
            const size_t end = dot == std::string::npos ? text.size() : dot + 1;
            const std::string sentence = text.substr(from, end - from);
            from = end;
            size_t words = 0;
            bool inWord = false;
            for (char ch : sentence) {
                const bool space = ch == ' ';
                if (!space && !inWord) ++words;
                inWord = !space;
            }
            if (words == 0) continue;
            SpeechChunk c;
            c.sampleRate = 100;
            c.samples.assign(words * 100, 0.f);
            c.phonemes.push_back({U'^', 0, 0});
            for (size_t w = 0; w < words; ++w) {
                const int at = static_cast<int>(w) * 100;
                if (w > 0) c.phonemes.push_back({U' ', at, 5});
                c.phonemes.push_back({U'a', at + 5, 45});
                c.phonemes.push_back({U'b', at + 50, 50});
            }
            c.phonemes.push_back({U'$', static_cast<int>(words) * 100, 0});
            c.isLast = end >= text.size();
            if (!onChunk(c)) return SynthResult::Cancelled;
        }
        return SynthResult::Done;
    };
}

// The sink holds one word (1 s) at a time, so the queue keeps the rest and Announcements can cut in.
struct WordFixture {
    FakeAudioSink sink;
    ReadingQueue queue{sink, WordVoice(), /*aheadSeconds=*/1e9, /*sinkSeconds=*/1.0};

    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
    // The sound card plays one word; the queue feeds the next.
    void NextWord() {
        sink.Advance(1.0);
        Settle();
    }
    std::string HeardText(const std::string& block) const {
        const auto h = queue.HeardWordNow();
        return h ? block.substr(h->range.start, h->range.end - h->range.start) : std::string("<none>");
    }
};

SynthResult SayOneSecond(const std::function<bool(const SpeechChunk&)>& onChunk) {
    SpeechChunk c;
    c.sampleRate = 100;
    c.samples.assign(100, 0.f);
    onChunk(c);
    return SynthResult::Done;
}

}  // namespace

TEST_CASE("Heard word: Add gives each Reading an id; a Reading with no text gets 0") {
    WordFixture f;
    const uint64_t a = f.queue.Add({"one two"});
    const uint64_t b = f.queue.Add({"three"});
    CHECK(a != 0);
    CHECK(b != 0);
    CHECK(a != b);
    CHECK(f.queue.Add({"", "  "}) == 0);
    CHECK(f.queue.Add({}) == 0);
}

TEST_CASE("Heard word: nothing is heard when idle") {
    WordFixture f;
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
}

TEST_CASE("Heard word: it moves word by word through the Block") {
    WordFixture f;
    const std::string text = "alpha beta gamma";
    const uint64_t id = f.queue.Add({text});
    f.Settle();
    REQUIRE(f.queue.HeardWordNow().has_value());
    CHECK(f.queue.HeardWordNow()->readingId == id);
    CHECK(f.queue.HeardWordNow()->block == 0);
    CHECK(f.HeardText(text) == "alpha");
    f.sink.Advance(0.5);  // half way through the word: still that word
    CHECK(f.HeardText(text) == "alpha");
    f.sink.Advance(0.5);
    f.Settle();
    CHECK(f.HeardText(text) == "beta");
    f.NextWord();
    CHECK(f.HeardText(text) == "gamma");
    f.sink.Drain();  // the last word is played
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
    f.Settle();
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
}

TEST_CASE("Heard word: the second sentence counts on from the first") {
    WordFixture f;
    const std::string text = "One two. Three four five.";
    f.queue.Add({text});
    f.Settle();
    const char* expected[] = {"One", "two", "Three", "four", "five"};
    for (const char* word : expected) {
        CHECK(f.HeardText(text) == word);
        f.NextWord();
    }
}

TEST_CASE("Heard word: the Block index and the Reading follow the queue") {
    WordFixture f;
    const uint64_t first = f.queue.Add({"aa bb", "cc"});
    const uint64_t second = f.queue.Add({"dd"});
    CHECK(first != second);
    f.Settle();
    CHECK(f.queue.HeardWordNow()->readingId == first);
    CHECK(f.queue.HeardWordNow()->block == 0);
    f.NextWord();  // "bb"
    f.NextWord();  // the end of Block 0: Block 1 starts
    REQUIRE(f.queue.HeardWordNow().has_value());
    CHECK(f.queue.HeardWordNow()->readingId == first);
    CHECK(f.queue.HeardWordNow()->block == 1);
    CHECK(f.queue.HeardWordNow()->range.start == 0);
    CHECK(f.queue.HeardWordNow()->range.end == 2);
    f.sink.Drain();
    f.Settle();
    REQUIRE(f.queue.HeardWordNow().has_value());
    CHECK(f.queue.HeardWordNow()->readingId == second);
    CHECK(f.queue.HeardWordNow()->block == 0);
}

TEST_CASE("Heard word: a Pause keeps the word, Resume goes on") {
    WordFixture f;
    const std::string text = "alpha beta gamma";
    f.queue.Add({text});
    f.Settle();
    f.NextWord();
    CHECK(f.HeardText(text) == "beta");
    f.queue.Pause();
    f.sink.Advance(1.0);  // a paused sink plays nothing
    f.Settle();
    CHECK(f.HeardText(text) == "beta");
    f.queue.Resume();
    f.NextWord();
    CHECK(f.HeardText(text) == "gamma");
}

TEST_CASE("Heard word: Skip and Stop clear it; Skip goes on in the next Block") {
    WordFixture f;
    const uint64_t id = f.queue.Add({"alpha beta", "gamma delta"});
    f.Settle();
    CHECK(f.queue.HeardWordNow().has_value());
    f.queue.Skip();
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
    f.Settle();
    REQUIRE(f.queue.HeardWordNow().has_value());
    CHECK(f.queue.HeardWordNow()->readingId == id);
    CHECK(f.queue.HeardWordNow()->block == 1);
    f.queue.Stop();
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
    f.Settle();
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
}

TEST_CASE("Heard word: an Announcement hides it, and the Reading's next word returns after it") {
    WordFixture f;
    const std::string text = "alpha beta gamma";
    f.queue.Add({text});
    f.Settle();
    CHECK(f.HeardText(text) == "alpha");
    f.queue.Announce(SayOneSecond);
    f.Settle();  // the Announcement is made; the Reading plays on to its word boundary
    CHECK(f.HeardText(text) == "alpha");
    f.sink.Drain();
    f.Settle();  // the Announcement plays
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
    f.sink.Drain();
    f.Settle();  // it is over; the Reading carries on
    CHECK(f.HeardText(text) == "beta");
}

TEST_CASE("Heard word: an Announcement over a paused Reading takes the audio back without losing the word") {
    WordFixture f;
    const std::string text = "alpha beta gamma";
    f.queue.Add({text});
    f.Settle();
    f.NextWord();
    CHECK(f.HeardText(text) == "beta");
    f.queue.Pause();
    f.queue.Announce(SayOneSecond);
    f.Settle();  // the Announcement plays over the pause; the Reading's audio is taken back
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
    f.sink.Drain();
    f.Settle();  // it is over; the Reading is paused again, with nothing in the sink
    f.queue.Resume();
    f.Settle();
    CHECK(f.HeardText(text) == "beta");  // from the start of the word it was paused in
    f.NextWord();
    CHECK(f.HeardText(text) == "gamma");
}

TEST_CASE("Heard word: audio without timings has no word to show") {
    Fixture f;  // the plain fake voice makes chunks with no phonemes
    f.queue.Add({"one two"});
    f.Settle();
    CHECK(f.sink.PendingSeconds() > 0);
    CHECK_FALSE(f.queue.HeardWordNow().has_value());
}

TEST_CASE("HasReading: a Reading is in the queue until its last Block is played, or Stop") {
    WordFixture f;
    CHECK_FALSE(f.queue.HasReading(0));
    CHECK_FALSE(f.queue.HasReading(1));  // not added yet
    const uint64_t a = f.queue.Add({"alpha beta"});
    const uint64_t b = f.queue.Add({"gamma"});
    CHECK(f.queue.HasReading(a));
    CHECK(f.queue.HasReading(b));
    CHECK_FALSE(f.queue.HasReading(b + 1));
    f.Settle();
    f.queue.Pause();  // a paused Reading is still there: the word mark keeps its word
    CHECK(f.queue.HasReading(a));
    f.queue.Resume();
    f.NextWord();
    f.NextWord();  // alpha and beta are played: Reading a is done
    f.Settle();
    CHECK_FALSE(f.queue.HasReading(a));
    CHECK(f.queue.HasReading(b));
    f.queue.Stop();
    CHECK_FALSE(f.queue.HasReading(b));
    CHECK_FALSE(f.queue.HasReading(0));  // the "ignored" id is never a Reading
}

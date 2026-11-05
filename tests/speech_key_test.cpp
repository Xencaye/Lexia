// Unit tests for the Speech key (src/tts/speech_key.h): a tap is Skip, taps within the window are
// counted (two = two Skips, the third = Stop). Driven with a real Reading queue over a fake sink and a
// fake Voice, and with time given by the test, so nothing waits.
#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <vector>

#include "fake_audio_sink.h"
#include "tts/reading_queue.h"
#include "tts/speech_key.h"

namespace {

using Strings = std::vector<std::string>;

struct Fixture {
    FakeAudioSink sink;
    Strings spoken;  // the Blocks the Voice was asked to speak, in order
    ReadingQueue queue{
        sink,
        [this](const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
            spoken.push_back(text);
            SpeechChunk c;
            c.samples.assign(100, 0.f);  // one second at 100 Hz
            c.sampleRate = 100;
            return onChunk(c) ? SynthResult::Done : SynthResult::Cancelled;
        },
        /*aheadSeconds=*/1e9};
    SpeechKey key{queue};  // the default window: 400 ms

    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
    // Reading "one", "two", "three", with "one" being spoken.
    void StartReading() {
        queue.Add({"one", "two", "three"});
        Settle();
    }
};

}  // namespace

TEST_CASE("Speech key: the window is about 0.4 s") {
    CHECK(SpeechKey::kTapWindowMs == 400);
}

TEST_CASE("Speech key: one tap silences the Block at once, then skips it when the window is over") {
    Fixture f;
    f.StartReading();

    CHECK(f.key.Tap(1000) == 400);  // the caller should come back in 400 ms
    CHECK(f.sink.paused);           // silent now, not 400 ms from now
    f.Settle();
    CHECK(f.spoken == Strings{"one"});  // and the next Block has not started

    CHECK(f.key.Settle(1399) == 1);  // 1 ms short: nothing yet
    CHECK(f.sink.paused);
    CHECK(f.key.Settle(1400) == 0);  // over: Skip
    f.Settle();
    CHECK_FALSE(f.sink.paused);
    CHECK(f.spoken == Strings{"one", "two"});
    CHECK(f.queue.Active());
}

TEST_CASE("Speech key: a third tap within the window stops, and the first two skipped nothing") {
    Fixture f;
    f.StartReading();

    CHECK(f.key.Tap(1000) == 400);
    CHECK(f.key.Tap(1200) == 400);  // the window starts again from the last tap
    CHECK(f.sink.paused);
    CHECK(f.key.Tap(1599) == 0);    // Stop: nothing more to wait for
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    CHECK_FALSE(f.sink.paused);
    CHECK(f.sink.PendingSeconds() == 0.0);
    f.Settle();
    CHECK(f.spoken == Strings{"one"});  // "two" was never started, not even for a moment

    CHECK(f.key.Settle(5000) == 0);  // the closed window does not fire a Skip later
    f.queue.Add({"later"});
    f.Settle();
    CHECK(f.spoken == Strings{"one", "later"});
}

TEST_CASE("Speech key: two taps within the window skip two Blocks when it ends") {
    Fixture f;
    f.StartReading();

    f.key.Tap(1000);
    CHECK(f.key.Tap(1300) == 400);
    f.Settle();
    CHECK(f.spoken == Strings{"one"});  // still quiet, nothing started
    CHECK(f.sink.paused);

    CHECK(f.key.Settle(1699) == 1);  // the window runs from the last tap
    CHECK(f.key.Settle(1700) == 0);
    f.Settle();
    CHECK_FALSE(f.sink.paused);
    CHECK(f.spoken.back() == "three");  // "one" and "two" are gone
    CHECK(f.queue.Active());
}

TEST_CASE("Speech key: two taps on the last two Blocks skip them both") {
    Fixture f;
    f.queue.Add({"one", "two"});
    f.Settle();
    f.key.Tap(0);
    f.key.Tap(100);
    f.key.Settle(500);
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Speech key: a tap after a Stop starts a new count") {
    Fixture f;
    f.StartReading();
    f.key.Tap(1000);
    f.key.Tap(1100);
    f.key.Tap(1200);  // Stop
    CHECK_FALSE(f.queue.Active());
    f.queue.Add({"new", "next"});
    f.Settle();
    CHECK(f.key.Tap(1300) == 400);  // the first tap of a new count, not a second one
    CHECK(f.queue.Active());
    f.key.Settle(1700);
    f.Settle();
    CHECK(f.spoken == Strings{"one", "new", "next"});  // one Skip only
}

TEST_CASE("Speech key: a second tap exactly one window after the first is a new tap, not a Stop") {
    Fixture f;
    f.StartReading();

    f.key.Tap(1000);
    CHECK(f.key.Tap(1400) == 400);  // the first tap's Skip happens, this one starts a window
    CHECK(f.queue.Active());        // not stopped
    CHECK(f.queue.Paused());        // held again by the new tap

    CHECK(f.key.Settle(1800) == 0);  // and its Skip comes
    CHECK_FALSE(f.queue.Paused());
    f.Settle();
    CHECK(f.queue.Active());
    CHECK(f.spoken.back() == "three");  // two Skips: "one" and "two" are gone
}

TEST_CASE("Speech key: taps spaced out skip one Block each") {
    Fixture f;
    f.StartReading();
    f.key.Tap(0);
    f.key.Settle(400);
    f.Settle();
    f.key.Tap(2000);
    f.key.Settle(2400);
    f.Settle();
    CHECK(f.spoken == Strings{"one", "two", "three"});
    CHECK(f.queue.Active());  // on "three"
    f.key.Tap(5000);
    f.key.Settle(5400);  // skips "three", the last
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Speech key: a tap while nothing is spoken does nothing and opens no window") {
    Fixture f;
    CHECK(f.key.Tap(1000) == 0);
    CHECK(f.sink.pauses == 0);
    CHECK(f.sink.stops == 0);
    f.queue.Add({"one", "two"});
    f.Settle();
    CHECK(f.key.Settle(1400) == 0);  // no Skip is waiting
    CHECK(f.spoken == Strings{"one"});
    CHECK_FALSE(f.sink.paused);
}

TEST_CASE("Speech key: on a paused queue one tap skips and resumes, three taps stop") {
    Fixture f;
    f.StartReading();
    f.queue.Pause();

    f.key.Tap(0);
    CHECK(f.queue.Paused());  // still the user's pause during the window
    f.key.Settle(400);
    f.Settle();
    CHECK_FALSE(f.queue.Paused());  // Skip carries on from the next Block
    CHECK(f.spoken == Strings{"one", "two"});

    f.queue.Pause();
    f.key.Tap(1000);
    f.key.Tap(1200);
    f.key.Tap(1300);
    CHECK_FALSE(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
}

TEST_CASE("Speech key: a Stop from somewhere else cancels the waiting Skip") {
    Fixture f;
    f.StartReading();
    f.key.Tap(1000);
    f.queue.Stop();  // e.g. the Read key opened a Session
    f.key.Cancel();
    f.queue.Add({"new"});
    f.Settle();
    CHECK(f.key.Settle(1400) == 0);
    f.Settle();
    CHECK(f.spoken == Strings{"one", "new"});  // "new" was not skipped
    CHECK_FALSE(f.sink.paused);
}

TEST_CASE("Speech key: a Session starting in the window does the Skip and keeps the rest of the Reading") {
    Fixture f;
    f.StartReading();
    f.key.Tap(1000);           // the queue is held for the window
    f.key.Flush();             // what Speaker::SessionStarted does first ...
    f.queue.SessionStarted();  // ... so the tap's Pause is not taken for the person's
    CHECK(f.queue.Active());
    CHECK_FALSE(f.queue.Paused());
    f.Settle();
    CHECK(f.spoken == Strings{"one", "two"});
    CHECK(f.key.Settle(1400) == 0);  // the window is closed: no second Skip
    f.Settle();
    CHECK(f.spoken == Strings{"one", "two"});
    CHECK(f.queue.Active());
}

TEST_CASE("Speech key: Flush with no tap waiting does nothing") {
    Fixture f;
    f.StartReading();
    f.key.Flush();
    CHECK(f.sink.stops == 0);
    f.Settle();
    CHECK(f.spoken == Strings{"one"});
    f.key.Tap(1000);
    f.key.Settle(1400);  // the Skip is done ...
    f.key.Flush();       // ... so there is nothing left to flush
    f.Settle();
    CHECK(f.spoken == Strings{"one", "two"});
}

TEST_CASE("Speech key: a tap right after a Stop does nothing") {
    Fixture f;
    f.StartReading();
    f.key.Tap(1000);
    f.key.Tap(1100);
    f.key.Tap(1150);  // Stop
    CHECK(f.key.Tap(1200) == 0);
    CHECK_FALSE(f.queue.Active());
}

TEST_CASE("Speech key: a Session starting after two taps does both Skips at once") {
    Fixture f;
    f.StartReading();
    f.key.Tap(1000);
    f.key.Tap(1100);
    f.key.Flush();
    f.queue.SessionStarted();
    f.Settle();
    CHECK(f.spoken.back() == "three");
    CHECK(f.key.Settle(2000) == 0);  // closed: no more Skips
    CHECK_FALSE(f.queue.Paused());
}

// Unit tests for tts/stretch_speech.h: a Block spoken as its stretches, each with the Voice of its
// Language, and what the Reading queue does with such a Block. Fake Voices and a fake sink.
#include <doctest/doctest.h>

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fake_audio_sink.h"
#include "tts/reading_queue.h"
#include "tts/stretch_speech.h"

namespace {

// "Продукт работи добре" and "Това е" in Cyrillic, as UTF-8 escapes.
const std::string kBg1 = "\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB4\xD1\x83\xD0\xBA\xD1\x82 \xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xB8 \xD0\xB4\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB5. ";
const std::string kBg2 = "\xD0\xA2\xD0\xBE\xD0\xB2\xD0\xB0 \xD0\xB5 \xD0\xB4\xD0\xBE\xD0\xB1\xD1\x8A\xD1\x80.";
const std::string kEn = "Hello big world. ";

struct Spoken {
    std::string language;  // code of the Voice that spoke it
    std::string text;
};

// Two chunks (one second each at 100 Hz) per stretch; the first chunk of a stretch carries the
// Voice's own isLast = false, the second true, as Piper does.
struct FakeVoices {
    std::set<std::string> installed{"en", "bg"};
    std::vector<Spoken> spoken;
    std::function<void(const std::string& text, int chunk)> beforeChunk;
    std::string failFor;  // text of a stretch that fails
    int chunksPerStretch = 2;

    VoiceAvailable Available() {
        return [this](const Language& l) { return installed.count(l.code) > 0; };
    }
    VoiceSynthesizer Synth() {
        return [this](const Language& l, const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
            spoken.push_back({l.code, text});
            if (text == failFor) return SynthResult::Failed;
            for (int i = 0; i < chunksPerStretch; ++i) {
                if (beforeChunk) beforeChunk(text, i);
                SpeechChunk c;
                c.samples.assign(100, 0.f);
                c.sampleRate = 100;
                c.isLast = i == chunksPerStretch - 1;
                c.phonemes = {{U'a', 0, 50}, {U' ', 50, 50}};
                if (!onChunk(c)) return SynthResult::Cancelled;
            }
            return SynthResult::Done;
        };
    }
};

}  // namespace

TEST_CASE("Stretch speech: each stretch is spoken in order by the Voice of its Language") {
    FakeVoices v;
    std::vector<SpeechChunk> chunks;
    const std::string text = kBg1 + kEn + kBg2;
    const SynthResult r = SpeakStretches(text, v.Available(), v.Synth(), [&](const SpeechChunk& c) {
        chunks.push_back(c);
        return true;
    });
    CHECK(r == SynthResult::Done);
    REQUIRE(v.spoken.size() == 3);
    CHECK(v.spoken[0].language == "bg");
    CHECK(v.spoken[0].text == kBg1);
    CHECK(v.spoken[1].language == "en");
    CHECK(v.spoken[1].text == kEn);
    CHECK(v.spoken[2].language == "bg");
    CHECK(v.spoken[2].text == kBg2);
    CHECK(v.spoken[0].text + v.spoken[1].text + v.spoken[2].text == text);
}

TEST_CASE("Stretch speech: chunks say which stretch they belong to; only the Block's last chunk is last") {
    FakeVoices v;
    std::vector<SpeechChunk> chunks;
    SpeakStretches(kBg1 + kEn + kBg2, v.Available(), v.Synth(), [&](const SpeechChunk& c) {
        chunks.push_back(c);
        return true;
    });
    REQUIRE(chunks.size() == 6);
    const size_t offsets[] = {0, kBg1.size(), kBg1.size() + kEn.size()};
    for (size_t i = 0; i < chunks.size(); ++i) {
        CHECK(chunks[i].textOffset == offsets[i / 2]);
        CHECK(chunks[i].isLast == (i == chunks.size() - 1));
        // The phoneme timings are untouched: relative to the chunk's own audio
        REQUIRE(chunks[i].phonemes.size() == 2);
        CHECK(chunks[i].phonemes[1].startSample == 50);
    }
}

TEST_CASE("Stretch speech: a Block of one stretch is spoken as it always was") {
    FakeVoices v;
    int n = 0;
    bool last = false;
    CHECK(SpeakStretches(kEn, v.Available(), v.Synth(), [&](const SpeechChunk& c) {
        ++n;
        last = c.isLast;
        CHECK(c.textOffset == 0);
        return true;
    }) == SynthResult::Done);
    CHECK(n == 2);
    CHECK(last);
    REQUIRE(v.spoken.size() == 1);
    CHECK(v.spoken[0].language == "en");
}

TEST_CASE("Stretch speech: a stretch with no Voice is spoken by the Block's main Voice, the longest stretch's") {
    FakeVoices v;
    v.installed = {"bg"};  // no English Voice
    const std::string text = kBg1 + kBg2 + " " + kEn;  // Bulgarian is the longer; the English phrase falls back
    CHECK(SpeakStretches(text, v.Available(), v.Synth(), [](const SpeechChunk&) { return true; }) ==
          SynthResult::Done);
    REQUIRE(v.spoken.size() == 2);  // the Bulgarian stretches are one: nothing in between
    CHECK(v.spoken[0].language == "bg");
    CHECK(v.spoken[0].text == kBg1 + kBg2 + " ");
    CHECK(v.spoken[1].language == "bg");
    CHECK(v.spoken[1].text == kEn);
}

TEST_CASE("Stretch speech: with no Voice for the main Language, nothing is spoken and it fails") {
    FakeVoices v;
    v.installed = {"en"};
    int n = 0;
    CHECK(SpeakStretches(kBg1 + kBg2 + " " + kEn, v.Available(), v.Synth(), [&](const SpeechChunk&) {
        ++n;
        return true;
    }) == SynthResult::Failed);
    CHECK(n == 0);
    CHECK(v.spoken.empty());
}

TEST_CASE("Stretch speech: a stretch that fails is skipped and the others are still spoken") {
    FakeVoices v;
    v.failFor = kEn;
    std::vector<SpeechChunk> chunks;
    CHECK(SpeakStretches(kBg1 + kEn + kBg2, v.Available(), v.Synth(), [&](const SpeechChunk& c) {
        chunks.push_back(c);
        return true;
    }) == SynthResult::Done);
    CHECK(v.spoken.size() == 3);
    CHECK(chunks.size() == 4);
    CHECK(chunks.back().isLast);
}

TEST_CASE("Stretch speech: when every stretch fails the Block fails") {
    FakeVoices v;
    v.failFor = kEn;
    CHECK(SpeakStretches(kEn, v.Available(), v.Synth(), [](const SpeechChunk&) { return true; }) ==
          SynthResult::Failed);
}

TEST_CASE("Stretch speech: a cut-off ends the Block, the stretches after it are never started") {
    FakeVoices v;
    int n = 0;
    CHECK(SpeakStretches(kBg1 + kEn + kBg2, v.Available(), v.Synth(), [&](const SpeechChunk&) {
        return ++n < 3;  // refuses the first chunk of the second stretch
    }) == SynthResult::Cancelled);
    CHECK(v.spoken.size() == 2);
}

TEST_CASE("Stretch speech: text with nothing to say fails and speaks nothing") {
    FakeVoices v;
    CHECK(SpeakStretches("  ", v.Available(), v.Synth(), [](const SpeechChunk&) { return true; }) ==
          SynthResult::Failed);
    CHECK(v.spoken.empty());
}

namespace {

// A Reading queue whose synthesiser speaks every Block as its stretches.
struct QueueFixture {
    FakeAudioSink sink;
    FakeVoices voices;
    ReadingQueue queue{
        sink,
        [this](const std::string& t, const std::function<bool(const SpeechChunk&)>& f) {
            return SpeakStretches(t, voices.Available(), voices.Synth(), f);
        },
        /*aheadSeconds=*/1e9};

    void Settle() {
        for (int i = 0; i < 100 && queue.Pump(); ++i) {
        }
    }
};

}  // namespace

TEST_CASE("Reading queue with stretches: a Block's stretches are one unit, the next Block waits for it to be heard") {
    QueueFixture f;
    f.queue.Add({kBg1 + kEn + kBg2, kEn});
    f.Settle();
    CHECK(f.voices.spoken.size() == 3);  // all of Block 1, none of Block 2
    CHECK(f.sink.TotalWrittenSeconds() == doctest::Approx(6.0));
    f.sink.Drain();
    f.Settle();
    CHECK(f.voices.spoken.size() == 4);
    CHECK(f.voices.spoken.back().text == kEn);
}

TEST_CASE("Reading queue with stretches: Skip during a Block skips all of it") {
    QueueFixture f;
    f.queue.Add({kBg1 + kEn + kBg2, kEn});
    // The skip comes in the middle of the first stretch
    f.voices.beforeChunk = [&](const std::string& text, int chunk) {
        if (text == kBg1 && chunk == 1) f.queue.Skip();
    };
    f.Settle();
    // Block 1 was cut off in its first stretch: its other stretches never started. The next Block
    // starts straight away.
    CHECK(f.voices.spoken.size() == 2);
    CHECK(f.voices.spoken[0].text == kBg1);
    CHECK(f.voices.spoken[1].text == kEn);
    CHECK(f.voices.spoken[1].language == "en");
}

TEST_CASE("Reading queue with stretches: Stop during a Block cuts it off and empties the queue") {
    QueueFixture f;
    f.queue.Add({kBg1 + kEn + kBg2, kEn});
    f.voices.beforeChunk = [&](const std::string& text, int chunk) {
        if (text == kEn && chunk == 0) f.queue.Stop();
    };
    f.Settle();
    CHECK(f.voices.spoken.size() == 2);  // the last stretch never started
    CHECK_FALSE(f.queue.Active());
    CHECK(f.sink.PendingSeconds() == 0.0);
}

TEST_CASE("Reading queue with stretches: Pause holds the sink and Resume goes on, across stretches") {
    QueueFixture f;
    f.queue.Add({kBg1 + kEn});
    f.queue.Pause();
    f.Settle();
    CHECK(f.voices.spoken.empty());  // paused before it started: nothing is spoken
    f.queue.Resume();
    f.Settle();
    CHECK(f.voices.spoken.size() == 2);
    f.queue.Pause();
    CHECK(f.sink.paused);
    f.queue.Resume();
    CHECK_FALSE(f.sink.paused);
    f.sink.Drain();
    f.Settle();
    CHECK_FALSE(f.queue.Active());
}

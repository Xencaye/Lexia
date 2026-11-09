// Unit tests for the pure parts of src/tts: phoneme timings, WAV encoding, voices.json.
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "tts/speech_chunk.h"
#include "tts/voices.h"
#include "tts/wav.h"

// ------------------------------------------------------------ phoneme timings ---

TEST_CASE("PhonemeTimings: a group of repeated code points takes that many alignments") {
    // '^' made two ids (start + pad), 'a' one id plus a pad, ' ' one id plus a pad, '$' one id.
    const std::vector<char32_t> ph = {U'^', U'^', 0, U'a', U'a', 0, U' ', U' ', 0, U'$', 0};
    const std::vector<int> al = {10, 20, 300, 40, 50, 60, 70};
    const auto t = PhonemeTimings(ph, al);
    REQUIRE(t.size() == 4);
    CHECK(t[0].phoneme == U'^');
    CHECK(t[0].startSample == 0);
    CHECK(t[0].lengthSamples == 30);
    CHECK(t[1].phoneme == U'a');
    CHECK(t[1].startSample == 30);
    CHECK(t[1].lengthSamples == 340);
    CHECK(t[2].phoneme == U' ');
    CHECK(t[2].startSample == 370);
    CHECK(t[2].lengthSamples == 110);
    CHECK(t[3].phoneme == U'$');
    CHECK(t[3].startSample == 480);
    CHECK(t[3].lengthSamples == 70);
}

TEST_CASE("PhonemeTimings: the timings sit back to back") {
    const std::vector<char32_t> ph = {U'h', U'h', 0, U'i', U'i', 0};
    const auto t = PhonemeTimings(ph, {5, 6, 7, 8});
    REQUIRE(t.size() == 2);
    CHECK(t[1].startSample == t[0].startSample + t[0].lengthSamples);
}

TEST_CASE("PhonemeTimings: missing alignments count as zero, no phonemes gives nothing") {
    const auto t = PhonemeTimings({U'a', U'a', 0, U'b', 0}, {9});
    REQUIRE(t.size() == 2);
    CHECK(t[0].lengthSamples == 9);
    CHECK(t[1].lengthSamples == 0);
    CHECK(PhonemeTimings({}, {1, 2}).empty());
    CHECK(PhonemeTimings({0, 0}, {1, 2}).empty());
}

// ------------------------------------------------------------------------- WAV ---

namespace {
uint32_t U32(const std::string& s, size_t at) {
    return static_cast<uint32_t>(static_cast<unsigned char>(s[at])) |
           static_cast<uint32_t>(static_cast<unsigned char>(s[at + 1])) << 8 |
           static_cast<uint32_t>(static_cast<unsigned char>(s[at + 2])) << 16 |
           static_cast<uint32_t>(static_cast<unsigned char>(s[at + 3])) << 24;
}
int16_t S16(const std::string& s, size_t at) {
    return static_cast<int16_t>(static_cast<unsigned char>(s[at]) |
                                static_cast<unsigned char>(s[at + 1]) << 8);
}
}  // namespace

TEST_CASE("EncodeWav16: header describes 16-bit mono PCM") {
    const std::string w = EncodeWav16({0.f, 0.f, 0.f}, 22050);
    REQUIRE(w.size() == 44 + 6);
    CHECK(w.substr(0, 4) == "RIFF");
    CHECK(U32(w, 4) == 36 + 6);
    CHECK(w.substr(8, 8) == "WAVEfmt ");
    CHECK(U32(w, 16) == 16);
    CHECK(S16(w, 20) == 1);      // PCM
    CHECK(S16(w, 22) == 1);      // mono
    CHECK(U32(w, 24) == 22050);  // sample rate
    CHECK(U32(w, 28) == 44100);  // bytes per second
    CHECK(S16(w, 32) == 2);      // bytes per frame
    CHECK(S16(w, 34) == 16);     // bits
    CHECK(w.substr(36, 4) == "data");
    CHECK(U32(w, 40) == 6);
}

TEST_CASE("EncodeWav16: samples are scaled, rounded and clipped") {
    const std::string w = EncodeWav16({0.5f, -0.5f, 1.f, -1.f, 2.f, -3.f}, 8000);
    REQUIRE(w.size() == 44 + 12);
    CHECK(S16(w, 44) == 16384);
    CHECK(S16(w, 46) == -16384);
    CHECK(S16(w, 48) == 32767);
    CHECK(S16(w, 50) == -32767);
    CHECK(S16(w, 52) == 32767);
    CHECK(S16(w, 54) == -32767);
}

TEST_CASE("EncodeWav16: no samples is still a valid, empty WAV") {
    const std::string w = EncodeWav16({}, 16000);
    CHECK(w.size() == 44);
    CHECK(U32(w, 40) == 0);
}

// ------------------------------------------------------------------ voices.json ---

TEST_CASE("ParseVoiceList: reads language and name, ignores the other fields") {
    std::string err;
    const auto v = ParseVoiceList(
        R"({"source":"x","voices":[
            {"language":"en","name":"en_GB-cori-medium","ship":true,"onnxSha256":"AB"},
            {"language":"bg","name":"bg_BG-dimitar-medium","ship":false}]})", &err);
    REQUIRE(v.size() == 2);
    CHECK(err.empty());
    CHECK(v[0].language == "en");
    CHECK(v[0].name == "en_GB-cori-medium");
    CHECK(v[1].language == "bg");
}

TEST_CASE("ParseVoiceList: bad input gives a one-line error") {
    std::string err;
    CHECK(ParseVoiceList("{not json", &err).empty());
    CHECK_FALSE(err.empty());
    err.clear();
    CHECK(ParseVoiceList(R"({"other":1})", &err).empty());
    CHECK_FALSE(err.empty());
    err.clear();
    const auto v = ParseVoiceList(
        R"({"voices":[{"language":"en","name":"a"},{"language":"bg"}]})", &err);
    CHECK(v.size() == 1);  // the entries read so far
    CHECK_FALSE(err.empty());
    CHECK(ParseVoiceList("{not json").empty());  // the error line is optional
}


// How a Voice is named on screen: the short name from the file name, and "(Medium)" after it.
#include "tts/voice_library.h"
#include "voice_names.h"

#include <doctest/doctest.h>

TEST_CASE("VoiceShortName: the id part of a Piper name, as a person reads it") {
    CHECK(VoiceShortName("en_US-ljspeech-medium") == "LJ Speech");
    CHECK(VoiceShortName("en_GB-jenny_dioco-medium") == "Jenny (Dioco)");  // the form the dataset's licence asks for
    CHECK(VoiceShortName("en_US-libritts_r-medium") == "LibriTTS-R");
    CHECK(VoiceShortName("en_US-sam-medium") == "Sam");
    CHECK(VoiceShortName("en_US-norman-high") == "Norman");
    CHECK(VoiceShortName("en_GB-northern_english_male-medium") == "Northern English Male");
    // A '-' inside the id stays: everything between the first and the last '-'.
    CHECK(VoiceShortName("en_US-a-b-low") == "A-b");
}

TEST_CASE("VoiceShortName: a name that is not <locale>-<id>-<quality> is shown as it is") {
    CHECK(VoiceShortName("myvoice") == "myvoice");
    CHECK(VoiceShortName("my-voice") == "my-voice");
    CHECK(VoiceShortName("en_US--medium") == "en_US--medium");  // no id between the dashes
}

TEST_CASE("VoiceQualityWord: Piper's four qualities, nothing for another word") {
    CHECK(VoiceQualityWord("en", "x_low") == "X-low");
    CHECK(VoiceQualityWord("en", "low") == "Low");
    CHECK(VoiceQualityWord("en", "medium") == "Medium");
    CHECK(VoiceQualityWord("en", "high") == "High");
    CHECK(VoiceQualityWord("en", "").empty());
    CHECK(VoiceQualityWord("en", "ultra").empty());
}

TEST_CASE("VoiceLabel: short name and quality; the short name alone with no quality") {
    CHECK(VoiceLabel("en", "en_US-ljspeech-medium", "medium") == "LJ Speech (Medium)");
    CHECK(VoiceLabel("en", "en_US-ljspeech-high", "high") == "LJ Speech (High)");
    CHECK(VoiceLabel("en", "myvoice", "") == "myvoice");
    // The config's quality wins over the name's (a Voice may be renamed).
    CHECK(VoiceLabel("en", "en_US-amy-medium", "low") == "Amy (Low)");
    CHECK(VoiceLabelFromName("en", "en_US-amy-x_low") == "Amy (X-low)");
    CHECK(VoiceLabelFromName("en", "my-voice") == "my-voice");
}

TEST_CASE("VoiceQualityFromName: only Piper's quality words at the end of the name") {
    CHECK(VoiceQualityFromName("en_US-ljspeech-medium") == "medium");
    CHECK(VoiceQualityFromName("en_US-amy-X_LOW") == "x_low");
    CHECK(VoiceQualityFromName("my-voice").empty());
    CHECK(VoiceQualityFromName("myvoice").empty());
}

// Unit tests for tts/preset_file.h: the Preset file a person saves and loads from the Voice tab.
#include <doctest/doctest.h>

#include <limits>
#include <string>

#include <nlohmann/json.hpp>

#include "tts/preset_file.h"
#include "tts/voice_controls.h"

namespace {
VoiceControls Controls(float speed, float pitch, float volume) {
    VoiceControls c;
    Set(c, VoiceControl::Speed, speed);
    Set(c, VoiceControl::Pitch, pitch);
    Set(c, VoiceControl::Volume, volume);
    return c;
}
}  // namespace

TEST_CASE("Preset file: the text holds the format version, the Voice and the three controls") {
    const nlohmann::json j = nlohmann::json::parse(PresetToJson(Controls(1.25f, -2.f, 0.8f), "en_US-amy-medium"));
    CHECK(j.at("lexiaPreset") == 1);
    CHECK(j.at("voice") == "en_US-amy-medium");
    CHECK(j.at("speed") == doctest::Approx(1.25));
    CHECK(j.at("pitch") == doctest::Approx(-2.0));
    CHECK(j.at("volume") == doctest::Approx(0.8));
    CHECK_FALSE(j.contains("expressiveness"));
}

TEST_CASE("Preset file: a saved Preset reads back the same") {
    const VoiceControls c = Controls(1.25f, -2.f, 0.8f);
    const PresetFileOutcome r = ParsePresetFile(PresetToJson(c, "en_US-amy-medium"));
    REQUIRE(r.ok());
    CHECK(r.voice == "en_US-amy-medium");
    CHECK(r.controls.speed == c.speed);
    CHECK(r.controls.pitch == c.pitch);
    CHECK(r.controls.volume == c.volume);
}

TEST_CASE("Preset file: the text is short numbers, not float noise") {
    const std::string text = PresetToJson(Controls(0.9f, 0.f, 1.f), "v");
    CHECK(text.find("0.9") != std::string::npos);
    CHECK(text.find("0.899") == std::string::npos);
}

TEST_CASE("Preset file: a missing control is that control's default") {
    const PresetFileOutcome r = ParsePresetFile(R"({"lexiaPreset":1,"voice":"v","speed":1.5})");
    REQUIRE(r.ok());
    CHECK(r.controls.speed == 1.5f);
    CHECK(r.controls.pitch == RangeOf(VoiceControl::Pitch).def);
    CHECK(r.controls.volume == RangeOf(VoiceControl::Volume).def);
}

TEST_CASE("Preset file: a value outside its range is clamped") {
    const PresetFileOutcome r = ParsePresetFile(R"({"lexiaPreset":1,"voice":"v","speed":99,"pitch":-99,"volume":99})");
    REQUIRE(r.ok());
    CHECK(r.controls.speed == RangeOf(VoiceControl::Speed).max);
    CHECK(r.controls.pitch == RangeOf(VoiceControl::Pitch).min);
    CHECK(r.controls.volume == RangeOf(VoiceControl::Volume).max);
}

TEST_CASE("Preset file: a control that is not a number is its default") {
    const PresetFileOutcome r = ParsePresetFile(R"({"lexiaPreset":1,"voice":"v","speed":"fast","pitch":null,"volume":[1]})");
    REQUIRE(r.ok());
    CHECK(r.controls.speed == RangeOf(VoiceControl::Speed).def);
    CHECK(r.controls.pitch == RangeOf(VoiceControl::Pitch).def);
    CHECK(r.controls.volume == RangeOf(VoiceControl::Volume).def);
}

TEST_CASE("Preset file: no Voice name is an empty name; a name that is not text too") {
    CHECK(ParsePresetFile(R"({"lexiaPreset":1})").voice.empty());
    const PresetFileOutcome r = ParsePresetFile(R"({"lexiaPreset":1,"voice":7})");
    CHECK(r.ok());
    CHECK(r.voice.empty());
}

TEST_CASE("Preset file: unknown keys are ignored, an old expressiveness too") {
    const PresetFileOutcome r =
        ParsePresetFile(R"({"lexiaPreset":1,"voice":"v","speed":1.1,"expressiveness":1.4,"colour":"red"})");
    REQUIRE(r.ok());
    CHECK(r.controls.speed == doctest::Approx(1.1f));
}

TEST_CASE("Preset file: text that is not JSON is NotJson") {
    CHECK(ParsePresetFile("").problem == PresetFileProblem::NotJson);
    CHECK(ParsePresetFile("hello").problem == PresetFileProblem::NotJson);
    CHECK(ParsePresetFile("{\"lexiaPreset\":1,").problem == PresetFileProblem::NotJson);
}

TEST_CASE("Preset file: JSON that is not a Lexia Preset is NotAPreset") {
    CHECK(ParsePresetFile("[1,2]").problem == PresetFileProblem::NotAPreset);
    CHECK(ParsePresetFile("{}").problem == PresetFileProblem::NotAPreset);
    CHECK(ParsePresetFile(R"({"voice":"v","speed":1})").problem == PresetFileProblem::NotAPreset);
    CHECK(ParsePresetFile(R"({"lexiaPreset":2,"voice":"v"})").problem == PresetFileProblem::NotAPreset);
    CHECK(ParsePresetFile(R"({"lexiaPreset":"1"})").problem == PresetFileProblem::NotAPreset);
}

TEST_CASE("Preset file: a NaN control is written as its default") {
    VoiceControls c;
    c.speed = std::numeric_limits<float>::quiet_NaN();
    const PresetFileOutcome r = ParsePresetFile(PresetToJson(c, "v"));
    REQUIRE(r.ok());
    CHECK(r.controls.speed == RangeOf(VoiceControl::Speed).def);
}

TEST_CASE("Preset file: ProblemId gives a stable id for the page") {
    CHECK(std::string(PresetFileProblemId(PresetFileProblem::NotJson)) == "badFile");
    CHECK(std::string(PresetFileProblemId(PresetFileProblem::NotAPreset)) == "badFile");
}

TEST_CASE("Preset file: the folder is 'presets' in the data folder") {
    CHECK(PresetsDir(std::filesystem::path("C:/data/Lexia")) == std::filesystem::path("C:/data/Lexia") / "presets");
    CHECK(PresetsDir({}).empty());
}

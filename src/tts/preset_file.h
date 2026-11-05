#pragma once
// The Preset file: a person saves the Voice tab's controls to a file and loads it again (the
// Voice tab's "save preset" / "load preset"). Win32-free; the file dialog and the reading and
// writing of the file are the caller's, this is the text and its checks.
//
//   {"lexiaPreset": 1, "voice": "<Voice name>", "speed": 1.0, "pitch": 0.0, "volume": 1.0}
//
// "lexiaPreset" is the format version. Reading is forgiving: a control that is missing (or not a
// number) is that control's default, one outside its range is clamped (voice_controls.h), and
// keys this version does not know are ignored (an old file's "expressiveness" too).
#include <filesystem>
#include <string>
#include <string_view>

#include "tts/voice_controls.h"

// The file's text for these controls and the Voice they were set on (pretty-printed, UTF-8).
std::string PresetToJson(const VoiceControls& controls, std::string_view voice);

enum class PresetFileProblem {
    None,
    NotJson,     // the text is not JSON
    NotAPreset,  // JSON, but no "lexiaPreset": 1 object
};

// A stable id for the page's result sheet ("badFile"); not UI text.
const char* PresetFileProblemId(PresetFileProblem problem);

struct PresetFileOutcome {
    PresetFileProblem problem = PresetFileProblem::None;
    VoiceControls controls;  // speed, pitch, volume read and clamped; the rest at their defaults
    std::string voice;       // the Voice's name; empty when the file has none
    bool ok() const { return problem == PresetFileProblem::None; }
};

PresetFileOutcome ParsePresetFile(std::string_view text);

// Where Preset files are suggested to live: `<dataDir>\presets` (dataDir = LexiaDataDir()).
// Empty if dataDir is empty. Pure; creating the folder is the caller's.
std::filesystem::path PresetsDir(const std::filesystem::path& dataDir);

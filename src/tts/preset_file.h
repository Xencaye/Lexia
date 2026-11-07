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
#include <cstddef>
#include <filesystem>
#include <set>
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

// ---- Loading a Preset for the right Voice ------------------------------------------------------------
// A Preset file names the Voice it was saved for. Loading it must not put another Voice's controls on the
// wrong Voice, and must not pretend a Voice that is not here was set. The decision, from the file's Voice,
// the Voice that is active now and the installed Voice names:
//
//   file's voice                       kind          voice     what the caller does
//   empty (an old file)                ActiveVoice   active    the controls go to the active Voice
//   the active Voice                   ActiveVoice   active    the same
//   another Voice that is installed    OtherVoice    file's    its stored controls change; the active Voice stays
//   a Voice that is not installed      NotInstalled  file's    nothing changes, nothing is saved: a Problem
enum class PresetTargetKind { ActiveVoice, OtherVoice, NotInstalled };

struct PresetTarget {
    PresetTargetKind kind = PresetTargetKind::ActiveVoice;
    std::string voice;  // ActiveVoice / OtherVoice: the Voice that gets the controls; NotInstalled: the file's name
};

PresetTarget ChoosePresetTarget(std::string_view fileVoice, std::string_view activeVoice,
                                const std::set<std::string>& installedNames);

// A Voice name from a file is untrusted text that is only shown (and logged), never used as a path. This is
// the text to show: control characters (a newline in a log line or a sheet) and the invisible direction
// marks that could reorder the text become spaces / are dropped, and the name is cut to kPresetVoiceShownMax
// code points with a "…" at the end.
constexpr size_t kPresetVoiceShownMax = 80;
std::string PresetVoiceForDisplay(std::string_view voice);

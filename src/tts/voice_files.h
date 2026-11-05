#pragma once
// The pieces the Voice library's adds and downloads share (voice_library.cpp, voice_catalogue.cpp,
// voice_download.cpp): refusals, temporary files and putting a checked Voice in place. Internal
// to those files; Win32-free (std::filesystem only).
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "tts/voice_library.h"

// A VoiceOutcome that refuses: `problem`, the Voice's name (may be empty) and one line for the log.
VoiceOutcome Fail(VoiceProblem problem, const std::string& name, const std::string& detail);

bool EndsWith(const std::string& s, const std::string& suffix);
// Does the file name `file` end with `ext` (lower case, e.g. ".onnx"), in any case? Windows file
// names are not case-sensitive, so "Voice.ONNX" is a model too.
bool HasExtension(const std::string& file, const std::string& ext);

// parent[key] if `parent` is an object and that is a string, else "".
std::string StringOf(const nlohmann::json& parent, const char* key);

// Removes the temporary files of an add or download that did not finish.
struct TempFiles {
    std::vector<std::filesystem::path> files;
    TempFiles() = default;
    TempFiles(const TempFiles&) = delete;
    TempFiles& operator=(const TempFiles&) = delete;
    ~TempFiles();
    void Done() { files.clear(); }  // they were put in place: keep them
};

// A Voice's two files under their temporary names in its Voice folder.
struct VoiceParts {
    std::filesystem::path model;   // <name>.onnx.part
    std::filesystem::path config;  // <name>.onnx.json.part
    TempFiles temp;                // removes both unless FinishVoice put them in place
};

// Creates `destDir` if missing and names `parts` in it. CannotWrite if the folder cannot be created.
VoiceOutcome PrepareVoiceParts(const std::string& name, const std::filesystem::path& destDir, VoiceParts* parts);

// The common end of every add and download: the parts are complete and `checked` (CheckVoiceConfig
// of the config) accepted them; move them into place (InstallVoice). `checked`, or CannotWrite.
VoiceOutcome FinishVoice(const VoiceOutcome& checked, const std::filesystem::path& destDir, VoiceParts& parts);

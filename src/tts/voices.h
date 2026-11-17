#pragma once
// voices.json: which Piper Voice speaks which Language. The choice lives in this data file (in
// voices\ next to the exe), not in code. Win32-free.
//   {"voices": [{"language": "en", "name": "en_GB-cori-medium", "ship": true, ...}]}
// "language" is a Language code (language.h); the Voice's files are voices\<name>.onnx and
// voices\<name>.onnx.json. "ship" and the other fields are for scripts\fetch_deps.ps1 and the
// package step (a Voice may name its own "source", a pinned download URL); only language and name are read here.
// The first English entry is the default Voice (PickVoice), so the order of the file is the order of preference.
#include <string>
#include <vector>

struct VoiceEntry {
    std::string language;  // Language code
    std::string name;      // file name without ".onnx"
};

// The entries of a voices.json. On malformed JSON, or an entry without both strings, `error`
// (if given) gets one line and the entries read so far are returned.
std::vector<VoiceEntry> ParseVoiceList(const std::string& jsonText, std::string* error = nullptr);

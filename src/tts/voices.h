#pragma once
// voices.json: which Piper Voice speaks which Language. The choice lives in this data file (in
// voices\ next to the exe), not in code. Win32-free.
//   {"voices": [{"language": "en", "name": "en_US-ljspeech-medium", "ship": true, ...}]}
// "language" is a Language code (language.h); the Voice's files are voices\<name>.onnx and
// voices\<name>.onnx.json. "ship" and the other fields are for scripts\fetch_deps.ps1 and the
// package step; only language and name are read here.
#include <string>
#include <vector>

struct VoiceEntry {
    std::string language;  // Language code
    std::string name;      // file name without ".onnx"
};

// The entries of a voices.json. On malformed JSON, or an entry without both strings, `error`
// (if given) gets one line and the entries read so far are returned.
std::vector<VoiceEntry> ParseVoiceList(const std::string& jsonText, std::string* error = nullptr);

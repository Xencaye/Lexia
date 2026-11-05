#pragma once
// The Piper Voices catalogue: the `voices.json` of Hugging Face's rhasspy/piper-voices, which
// lists every downloadable Voice with its files, sizes and MD5 digests. Parsing and filtering
// only (Win32-free, no network); voice_download.h fetches it. Only the fields Lexia needs are read.
#include <cstdint>
#include <string>
#include <vector>

#include "tts/voice_library.h"

struct CatalogueFile {
    std::string path;     // inside the repository, e.g. "bg/bg_BG/dimitar/medium/bg_BG-dimitar-medium.onnx"
    uint64_t sizeBytes = 0;
    std::string md5;      // lower case hex
};

struct CatalogueVoice {
    std::string name;          // "bg_BG-dimitar-medium"
    std::string languageCode;  // the catalogue's, "bg_BG"
    std::string languageName;  // "Bulgarian" (name_english)
    std::string quality;       // "medium", "low", ...
    int numSpeakers = 1;
    CatalogueFile model;       // the .onnx
    CatalogueFile config;      // the .onnx.json
};

// Every Voice in a catalogue that has an .onnx and an .onnx.json with a safe path, a size and an
// MD5. Voices without them are left out. On text that is not a catalogue, `error` (if given) gets
// one line and the result is empty.
std::vector<CatalogueVoice> ParseVoiceCatalogue(const std::string& jsonText, std::string* error = nullptr);

// Can Lexia use this Voice (CheckVoiceAccepted on its language and quality)?
VoiceOutcome CheckCatalogueVoice(const CatalogueVoice& voice);

// The Voices Lexia offers: those for one of its Languages (AllLanguages, by the catalogue's
// language code) in Medium quality, sorted by name.
std::vector<CatalogueVoice> FilterCatalogueForLexia(const std::vector<CatalogueVoice>& voices);

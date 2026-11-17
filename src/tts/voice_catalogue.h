#pragma once
// What the Download sheet knows about one downloadable Voice: its name, Language, quality and the two files
// (path, size, MD5) with the place they come from. The list of these is compiled in (download_list.h); nothing
// here fetches or parses anything from the network. Win32-free.
#include <cstdint>
#include <string>

#include "tts/voice_library.h"

struct CatalogueFile {
    std::string path;     // inside the repository, e.g. "en/en_US/kristin/medium/en_US-kristin-medium.onnx"
    uint64_t sizeBytes = 0;
    std::string md5;      // lower case hex
};

struct CatalogueVoice {
    std::string name;          // "en_US-kristin-medium"
    std::string languageCode;  // Piper's, "en_US"
    std::string languageName;  // "English"
    std::string quality;       // "medium", "low", ...
    // Where the files are, with the trailing slash: `<baseUrl><path>` is the file's URL. A Hugging Face
    // rhasspy/piper-voices URL pinned to one commit (never a branch), so the file, its size and its MD5 cannot
    // change under Lexia. The download refuses a base that is not https (voice_download.h).
    std::string baseUrl;
    CatalogueFile model;       // the .onnx
    CatalogueFile config;      // the .onnx.json
};

// Can Lexia use this Voice (CheckVoiceAccepted on its name, language and quality)?
VoiceOutcome CheckCatalogueVoice(const CatalogueVoice& voice);

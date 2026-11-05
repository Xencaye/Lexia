#pragma once
// Downloading Voices from the piper-voices catalogue. Only when the user asks (ADR-0005): nothing
// here runs at startup or in the background, and nothing but a Voice's own files is ever sent
// or fetched. The network is behind a FetchFn, so this module is Win32-free and lexia_tests runs it
// with a fake; the real one is net/winhttp_fetch.h (HTTPS only).
//
// A Voice is fetched under temporary names (`<name>.onnx.part`), checked against the catalogue's
// size and MD5 and against the accepted Voices (voice_library.h), and only then renamed into
// place, so the Voice folder never holds half a Voice. The UI is D5; progress and cancelling are here.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "tts/voice_catalogue.h"
#include "tts/voice_library.h"

enum class FetchResult { Done, Cancelled, Failed };

// Gets `url` into the file `to`. `onBytes` is called with the bytes received so far; returning false
// cancels (Cancelled). More than `maxBytes` is a Failed. `error` (if given) gets one line on Failed.
using FetchFn = std::function<FetchResult(const std::string& url, const std::filesystem::path& to,
                                          uint64_t maxBytes, const std::function<bool(uint64_t)>& onBytes,
                                          std::string* error)>;

// Called with bytes done and bytes in all (the Voice's files, from the catalogue); false cancels.
using DownloadProgressFn = std::function<bool(uint64_t done, uint64_t total)>;

// Where the catalogue and the files are, with the trailing slash: Hugging Face rhasspy/piper-voices at
// one pinned commit, so the catalogue and every file it lists (and their MD5s) can't change under
// Lexia. Re-pin deliberately, to the commit voices/voices.json's "source" names, to see newer Voices.
extern const char* const kVoiceRepoUrl;

struct CatalogueResult {
    VoiceOutcome error;                   // not ok: the catalogue could not be fetched or read
    std::vector<CatalogueVoice> voices;   // FilterCatalogueForLexia of it
};

// Fetches voices.json (into `tempDir`, deleted again) and filters it.
CatalogueResult FetchVoiceCatalogue(const FetchFn& fetch, const std::filesystem::path& tempDir);

// Downloads one Voice into `destDir`. A Voice that Lexia would not accept is refused before any
// download (UnsupportedLanguage, NotMedium). Network on a failed fetch, BadDownload on a wrong
// size or digest, Cancelled when `progress` returns false. A Voice already in `destDir` with this
// name is replaced only when the new one is complete and right.
VoiceOutcome DownloadVoice(const CatalogueVoice& voice, const FetchFn& fetch,
                           const DownloadProgressFn& progress, const std::filesystem::path& destDir);

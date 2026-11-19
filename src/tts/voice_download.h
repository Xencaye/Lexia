#pragma once
// Downloading a Voice of the download list (download_list.h). Only when the user asks (ADR-0005): nothing
// here runs at startup or in the background, and nothing but a Voice's own files is ever sent
// or fetched. The network is behind a FetchFn, so this module is Win32-free and lexia_tests runs it
// with a fake; the real one is net/winhttp_fetch.h (HTTPS only).
//
// A Voice is fetched under temporary names (`<name>.onnx.part`), checked against the list's
// size and MD5 and against the accepted Voices (voice_library.h), and only then renamed into
// place, so the Voice folder never holds half a Voice. The UI is D5; progress and cancelling are here.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include "tts/voice_catalogue.h"
#include "tts/voice_library.h"

enum class FetchResult { Done, Cancelled, Failed };

// Gets `url` into the file `to`. `onBytes` is called with the bytes received so far; returning false
// cancels (Cancelled). More than `maxBytes` is a Failed. `error` (if given) gets one line on Failed.
using FetchFn = std::function<FetchResult(const std::string& url, const std::filesystem::path& to,
                                          uint64_t maxBytes, const std::function<bool(uint64_t)>& onBytes,
                                          std::string* error)>;

// Called with bytes done and bytes in all (the Voice's files, from the list); false cancels.
using DownloadProgressFn = std::function<bool(uint64_t done, uint64_t total)>;

// Downloads one Voice into `destDir`, each of its two files from `voice.baseUrl + path` (https only: a base
// that is not https is refused as Network, with no request). A Voice that Lexia would not accept is refused
// before any download (BadName, UnsupportedLanguage, UnknownQuality). Network on a failed fetch, BadDownload on
// a wrong size or digest, Cancelled when `progress` returns false. A Voice already in `destDir` with this
// name is replaced only when the new one is complete and right.
VoiceOutcome DownloadVoice(const CatalogueVoice& voice, const FetchFn& fetch,
                           const DownloadProgressFn& progress, const std::filesystem::path& destDir);

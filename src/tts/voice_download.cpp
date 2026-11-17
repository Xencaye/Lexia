#include "tts/voice_download.h"

#include <system_error>

#include "common.h"
#include "tts/md5.h"
#include "tts/voice_files.h"

// The Voice library's downloads (B5): one Voice's two files, from Hugging Face (the download list names them).
// The network itself is behind FetchFn (WinHttpFetch in the app, a fake in the tests). Nothing here runs unless the
// user asks for it (ADR-0005).
namespace fs = std::filesystem;

namespace {

bool IsHttps(const std::string& url) { return url.rfind("https://", 0) == 0; }

// Where one file sits in the progress of a whole Voice: `doneBefore` bytes of `total` came before it.
struct ProgressSpan {
    uint64_t doneBefore = 0;
    uint64_t total = 0;
    const DownloadProgressFn* progress = nullptr;  // may be null or empty
};

// Fetches one file of the list to `to` and checks its size and digest. `baseUrl` is the Voice's own, compiled
// in and pinned to a commit (download_list.cpp); the check that it is https is kept here, so a wrong entry never
// sends a request in the clear. Ok, or the problem.
VoiceOutcome FetchChecked(const FetchFn& fetch, const std::string& baseUrl, const CatalogueFile& file,
                          const std::string& name, const fs::path& to, const ProgressSpan& span) {
    const std::string url = baseUrl + file.path;
    if (!IsHttps(url)) return Fail(VoiceProblem::Network, name, "refusing a download that is not HTTPS");
    std::string error;
    const FetchResult r = fetch(url, to, file.sizeBytes,
                                [&](uint64_t bytes) {
                                    return !span.progress || !*span.progress ||
                                           (*span.progress)(span.doneBefore + bytes, span.total);
                                }, &error);
    if (r == FetchResult::Cancelled) return Fail(VoiceProblem::Cancelled, name, "cancelled");
    if (r == FetchResult::Failed) return Fail(VoiceProblem::Network, name, "download failed: " + error);
    std::error_code ec;
    const uint64_t size = fs::file_size(to, ec);
    if (ec || size != file.sizeBytes) {
        return Fail(VoiceProblem::BadDownload, name,
                    "wrong size for " + file.path + ": got " + std::to_string(ec ? 0 : size) + ", the list says " +
                        std::to_string(file.sizeBytes));
    }
    if (Md5OfFile(to) != file.md5) {
        return Fail(VoiceProblem::BadDownload, name, "the MD5 of " + file.path + " does not match the list");
    }
    return {};
}

}  // namespace

// Downloads one Voice into destDir: both files as .part files, each checked against the list's
// size and MD5, the config also checked by CheckVoiceConfig, then renamed into place. Progress is
// reported over both files as one total. On any failure or cancel the .part files are deleted.
VoiceOutcome DownloadVoice(const CatalogueVoice& voice, const FetchFn& fetch, const DownloadProgressFn& progress,
                           const fs::path& destDir) {
    const VoiceOutcome accepted = CheckCatalogueVoice(voice);
    if (!accepted.ok()) return accepted;

    VoiceParts parts;
    if (VoiceOutcome bad = PrepareVoiceParts(voice.name, destDir, &parts); !bad.ok()) return bad;

    const uint64_t total = voice.model.sizeBytes + voice.config.sizeBytes;
    const ProgressSpan configSpan{0, total, &progress};
    const ProgressSpan modelSpan{voice.config.sizeBytes, total, &progress};
    // The config first: it is small, and it can still refuse the Voice before the big download.
    VoiceOutcome r = FetchChecked(fetch, voice.baseUrl, voice.config, voice.name, parts.config, configSpan);
    if (!r.ok()) return r;
    std::string configText;
    ReadFileToString(parts.config, &configText);  // unreadable: empty text, which the check refuses
    const VoiceOutcome checked = CheckVoiceConfig(voice.name, configText);
    if (!checked.ok()) return checked;
    r = FetchChecked(fetch, voice.baseUrl, voice.model, voice.name, parts.model, modelSpan);
    if (!r.ok()) return r;
    return FinishVoice(checked, destDir, parts);
}

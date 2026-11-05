#include "tts/voice_download.h"

#include <system_error>

#include "common.h"
#include "tts/md5.h"
#include "tts/voice_files.h"

// The Voice library's downloads (B5): the catalogue of Piper Voices and one Voice's two files, from
// Hugging Face. The network itself is behind FetchFn (WinHttpFetch in the app, a fake in the tests).
// Nothing here runs unless the user asks for it (ADR-0005).
namespace fs = std::filesystem;

// Pinned to one commit of rhasspy/piper-voices, so the catalogue and its file sizes and MD5s cannot
// change under Lexia.
const char* const kVoiceRepoUrl =
    "https://huggingface.co/rhasspy/piper-voices/resolve/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/";

namespace {

constexpr uint64_t kMaxCatalogueBytes = 16ull * 1024 * 1024;  // it is ~300 KB

bool IsHttps(const std::string& url) { return url.rfind("https://", 0) == 0; }

// Where one file sits in the progress of a whole Voice: `doneBefore` bytes of `total` came before it.
struct ProgressSpan {
    uint64_t doneBefore = 0;
    uint64_t total = 0;
    const DownloadProgressFn* progress = nullptr;  // may be null or empty
};

// Fetches one catalogue file to `to` and checks its size and digest. Ok, or the problem.
VoiceOutcome FetchChecked(const FetchFn& fetch, const CatalogueFile& file, const std::string& name, const fs::path& to,
                          const ProgressSpan& span) {
    const std::string url = std::string(kVoiceRepoUrl) + file.path;
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
                    "wrong size for " + file.path + ": got " + std::to_string(ec ? 0 : size) + ", the catalogue says " +
                        std::to_string(file.sizeBytes));
    }
    if (Md5OfFile(to) != file.md5) {
        return Fail(VoiceProblem::BadDownload, name, "the MD5 of " + file.path + " does not match the catalogue");
    }
    return {};
}

}  // namespace

// Fetches voices.json of the pinned commit into tempDir (deleted again afterwards), parses it, and
// keeps the Voices that Lexia accepts (Medium quality, a Language in Lexia's table), sorted by name.
// The Voice tab then shows only those of the Languages that Lexia offers.
CatalogueResult FetchVoiceCatalogue(const FetchFn& fetch, const fs::path& tempDir) {
    CatalogueResult result;
    const std::string url = std::string(kVoiceRepoUrl) + "voices.json";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    const fs::path part = tempDir / "voices.json.part";
    TempFiles temp;
    temp.files = {part};
    std::string error;
    const FetchResult r = fetch(url, part, kMaxCatalogueBytes, [](uint64_t) { return true; }, &error);
    if (r != FetchResult::Done) {
        result.error = Fail(VoiceProblem::Network, {}, "could not fetch the Voice catalogue: " + error);
        return result;
    }
    std::string text;
    ReadFileToString(part, &text);  // unreadable: empty text, which the parse below refuses
    std::string parseError;
    const std::vector<CatalogueVoice> all = ParseVoiceCatalogue(text, &parseError);
    if (!parseError.empty()) {
        result.error = Fail(VoiceProblem::Network, {}, parseError);
        return result;
    }
    result.voices = FilterCatalogueForLexia(all);
    return result;
}

// Downloads one Voice into destDir: both files as .part files, each checked against the catalogue's
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
    VoiceOutcome r = FetchChecked(fetch, voice.config, voice.name, parts.config, configSpan);
    if (!r.ok()) return r;
    std::string configText;
    ReadFileToString(parts.config, &configText);  // unreadable: empty text, which the check refuses
    const VoiceOutcome checked = CheckVoiceConfig(voice.name, configText);
    if (!checked.ok()) return checked;
    r = FetchChecked(fetch, voice.model, voice.name, parts.model, modelSpan);
    if (!r.ok()) return r;
    return FinishVoice(checked, destDir, parts);
}

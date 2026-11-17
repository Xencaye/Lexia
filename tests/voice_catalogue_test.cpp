// Unit tests for tts/download_list.h and tts/voice_catalogue.h: the compiled-in list of downloadable Voices.
#include <doctest/doctest.h>

#include <cctype>
#include <set>
#include <string>

#include "tts/download_list.h"
#include "tts/voice_catalogue.h"
#include "tts/voice_library.h"

namespace {

// The path goes into a URL: only plain path characters, no ".." and no leading slash.
bool PlainPath(const std::string& path) {
    if (!IsSafeRelativePath(path)) return false;
    for (unsigned char c : path) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/')) return false;
    }
    return true;
}

bool LowerHex(const std::string& s, size_t length) {
    if (s.size() != length) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("DownloadList: the owner's three Voices, in the owner's order") {
    const auto& list = DownloadList();
    REQUIRE(list.size() == 3);
    CHECK(list[0].name == "en_US-kristin-medium");
    CHECK(list[1].name == "en_US-john-medium");
    CHECK(list[2].name == "en_US-ljspeech-medium");
}

TEST_CASE("DownloadList: every entry is accepted by CheckCatalogueVoice and its Language is English") {
    for (const CatalogueVoice& v : DownloadList()) {
        CAPTURE(v.name);
        const VoiceOutcome o = CheckCatalogueVoice(v);
        CHECK(o.ok());
        CHECK(o.languageCode == "en");
        CHECK(v.languageName == "English");
        CHECK(v.name.rfind(v.languageCode + "-", 0) == 0);  // starts with "<code>-"
        CHECK(v.name.size() > v.quality.size());
        CHECK(v.name.substr(v.name.size() - v.quality.size()) == v.quality);  // ends with its quality
    }
}

TEST_CASE("DownloadList: each base URL is https, rhasspy/piper-voices, pinned to a 40-digit commit, with a slash") {
    const std::string prefix = "https://huggingface.co/rhasspy/piper-voices/resolve/";
    for (const CatalogueVoice& v : DownloadList()) {
        CAPTURE(v.name);
        REQUIRE(v.baseUrl.rfind(prefix, 0) == 0);
        CHECK(v.baseUrl.back() == '/');
        const std::string commit = v.baseUrl.substr(prefix.size(), v.baseUrl.size() - prefix.size() - 1);
        CHECK(LowerHex(commit, 40));  // a commit, not a branch that can move
    }
}

TEST_CASE("DownloadList: the file paths are plain and safe, and name the Voice's two files") {
    for (const CatalogueVoice& v : DownloadList()) {
        CAPTURE(v.name);
        CHECK(PlainPath(v.model.path));
        CHECK(PlainPath(v.config.path));
        CHECK(v.model.path.size() > v.name.size());
        CHECK(v.model.path.substr(v.model.path.size() - v.name.size() - 5) == v.name + ".onnx");
        CHECK(v.config.path == v.model.path + ".json");
    }
}

TEST_CASE("DownloadList: sizes are above 0 and MD5 digests are 32 lower-case hex digits") {
    for (const CatalogueVoice& v : DownloadList()) {
        CAPTURE(v.name);
        CHECK(v.model.sizeBytes > 0);
        CHECK(v.config.sizeBytes > 0);
        CHECK(LowerHex(v.model.md5, 32));
        CHECK(LowerHex(v.config.md5, 32));
    }
}

TEST_CASE("DownloadList: names are unique, and a Voice Lexia ships is not on the list") {
    std::set<std::string> names;
    for (const CatalogueVoice& v : DownloadList()) {
        CHECK(names.insert(v.name).second);
        CHECK(v.name != "en_GB-cori-medium");    // shipped (voices/voices.json)
        CHECK(v.name != "en_US-norman-medium");  // shipped
    }
}

TEST_CASE("CheckCatalogueVoice: names the language Lexia cannot read, or a quality Piper does not have") {
    CatalogueVoice v = DownloadList().front();

    CatalogueVoice german = v;
    german.name = "de_DE-thorsten-medium";
    german.languageCode = "de_DE";
    german.languageName = "German";
    const VoiceOutcome refused = CheckCatalogueVoice(german);
    CHECK(refused.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(refused.languageName == "German");

    CatalogueVoice odd = v;
    odd.quality = "ultra";
    const VoiceOutcome unknown = CheckCatalogueVoice(odd);
    CHECK(unknown.problem == VoiceProblem::UnknownQuality);
    CHECK(unknown.quality == "ultra");

    // A Low or a High Voice is fine: the owner may put one on the list later.
    CatalogueVoice low = v;
    low.quality = "low";
    CHECK(CheckCatalogueVoice(low).ok());
}

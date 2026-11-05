// Unit tests for tts/voice_download.h, with a fake FetchFn: no network is touched.
#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "tts/md5.h"
#include "tts/voice_download.h"

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() / ("lexia_voice_download_test_" + std::to_string(++counter) + "_" +
                                            std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string Md5OfText(const std::string& s) {
    Md5 m;
    m.Update(s.data(), s.size());
    return m.Hex();
}

std::string ReadText(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

const std::string kConfig = "{\"audio\": {\"quality\": \"medium\"}, \"language\": {\"code\": \"en_US\", \"name_english\": \"English\"}}";
const std::string kModel(5000, 'M');

// A Voice the fake server has, with the catalogue entry that describes it.
CatalogueVoice AmyEntry(const std::string& model = kModel, const std::string& config = kConfig) {
    CatalogueVoice v;
    v.name = "en_US-amy-medium";
    v.languageCode = "en_US";
    v.languageName = "English";
    v.quality = "medium";
    v.model = {"en/en_US/amy/medium/en_US-amy-medium.onnx", model.size(), Md5OfText(model)};
    v.config = {"en/en_US/amy/medium/en_US-amy-medium.onnx.json", config.size(), Md5OfText(config)};
    return v;
}

// The fake network: url -> body. Records the URLs asked for.
struct FakeServer {
    std::map<std::string, std::string> files;
    std::vector<std::string> asked;
    bool fail = false;

    FetchFn Fn() {
        return [this](const std::string& url, const fs::path& to, uint64_t maxBytes,
                      const std::function<bool(uint64_t)>& onBytes, std::string* error) {
            asked.push_back(url);
            const auto it = files.find(url);
            if (fail || it == files.end()) {
                if (error) *error = "HTTP 404";
                return FetchResult::Failed;
            }
            if (it->second.size() > maxBytes) {
                if (error) *error = "too much data";
                return FetchResult::Failed;
            }
            std::ofstream out(to, std::ios::binary | std::ios::trunc);
            const std::string& body = it->second;
            for (size_t i = 0; i < body.size(); i += 1000) {
                const size_t n = std::min<size_t>(1000, body.size() - i);
                out.write(body.data() + i, static_cast<std::streamsize>(n));
                out.flush();
                if (!onBytes(i + n)) return FetchResult::Cancelled;
            }
            return FetchResult::Done;
        };
    }
};

FakeServer AmyServer(const std::string& model = kModel, const std::string& config = kConfig) {
    FakeServer s;
    s.files[std::string(kVoiceRepoUrl) + "en/en_US/amy/medium/en_US-amy-medium.onnx"] = model;
    s.files[std::string(kVoiceRepoUrl) + "en/en_US/amy/medium/en_US-amy-medium.onnx.json"] = config;
    return s;
}

}  // namespace

TEST_CASE("kVoiceRepoUrl: the catalogue is fetched over HTTPS from the rhasspy/piper-voices repository") {
    const std::string url = kVoiceRepoUrl;
    CHECK(url.rfind("https://huggingface.co/rhasspy/piper-voices/", 0) == 0);
    CHECK(url.back() == '/');
}

TEST_CASE("kVoiceRepoUrl: pinned to one commit, not a branch that can move") {
    const std::string prefix = "https://huggingface.co/rhasspy/piper-voices/resolve/";
    const std::string url = kVoiceRepoUrl;
    REQUIRE(url.rfind(prefix, 0) == 0);
    const std::string revision = url.substr(prefix.size(), url.size() - prefix.size() - 1);
    CHECK(revision.size() == 40);
    CHECK(revision.find_first_not_of("0123456789abcdef") == std::string::npos);
}

TEST_CASE("DownloadVoice: fetches both files, checks them and moves them into the folder") {
    TempDir dir;
    FakeServer server = AmyServer();
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), nullptr, dir.path / "voices");
    REQUIRE(r.ok());
    CHECK(r.voiceName == "en_US-amy-medium");
    CHECK(r.languageCode == "en");
    CHECK(ReadText(dir.path / "voices" / "en_US-amy-medium.onnx") == kModel);
    CHECK(ReadText(dir.path / "voices" / "en_US-amy-medium.onnx.json") == kConfig);
    CHECK(server.asked.size() == 2);
    for (const std::string& url : server.asked) CHECK(url.rfind("https://", 0) == 0);
    int parts = 0;
    for (const auto& e : fs::directory_iterator(dir.path / "voices")) parts += e.path().extension() == ".part";
    CHECK(parts == 0);
}

TEST_CASE("DownloadVoice: progress counts the bytes of both files and runs up to the total") {
    TempDir dir;
    FakeServer server = AmyServer();
    uint64_t last = 0, total = 0;
    bool increasing = true;
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), [&](uint64_t done, uint64_t all) {
        if (done < last) increasing = false;
        last = done;
        total = all;
        return true;
    }, dir.path);
    REQUIRE(r.ok());
    CHECK(increasing);
    CHECK(total == kModel.size() + kConfig.size());
    CHECK(last == total);
}

TEST_CASE("DownloadVoice: cancelling stops the download and keeps nothing") {
    TempDir dir;
    FakeServer server = AmyServer();
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), [&](uint64_t done, uint64_t) {
        return done < kConfig.size() + 2000;  // cancel part-way through the model
    }, dir.path);
    CHECK(r.problem == VoiceProblem::Cancelled);
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("DownloadVoice: a wrong digest is a BadDownload and an existing Voice is left alone") {
    TempDir dir;
    {
        std::ofstream(dir.path / "en_US-amy-medium.onnx", std::ios::binary) << "OLD";
        std::ofstream(dir.path / "en_US-amy-medium.onnx.json", std::ios::binary) << kConfig;
    }
    std::string damaged = kModel;
    damaged[100] = 'X';  // same size, other bytes
    FakeServer server = AmyServer(damaged);
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::BadDownload);
    CHECK(ReadText(dir.path / "en_US-amy-medium.onnx") == "OLD");
    CHECK_FALSE(fs::exists(dir.path / "en_US-amy-medium.onnx.part"));
    CHECK_FALSE(fs::exists(dir.path / "en_US-amy-medium.onnx.json.part"));
}

TEST_CASE("DownloadVoice: a file of the wrong size is a BadDownload") {
    TempDir dir;
    FakeServer server = AmyServer(kModel.substr(0, 3000));  // cut off
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::BadDownload);
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("DownloadVoice: a server that sends more than the catalogue says is stopped") {
    TempDir dir;
    FakeServer server = AmyServer(kModel + "extra");
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::Network);
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("DownloadVoice: a failed fetch is a Network problem and keeps nothing") {
    TempDir dir;
    FakeServer server = AmyServer();
    server.fail = true;
    const VoiceOutcome r = DownloadVoice(AmyEntry(), server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::Network);
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("DownloadVoice: a Voice Lexia would not accept is refused before anything is fetched") {
    TempDir dir;
    FakeServer server = AmyServer();
    CatalogueVoice german = AmyEntry();
    german.languageCode = "de_DE";
    german.languageName = "German";
    const VoiceOutcome r = DownloadVoice(german, server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(r.languageName == "German");
    CatalogueVoice low = AmyEntry();
    low.quality = "low";
    CHECK(DownloadVoice(low, server.Fn(), nullptr, dir.path).problem == VoiceProblem::NotMedium);
    CHECK(server.asked.empty());
}

TEST_CASE("DownloadVoice: the downloaded config is checked too, whatever the catalogue said") {
    TempDir dir;
    const std::string germanConfig = "{\"audio\": {\"quality\": \"medium\"}, \"language\": {\"code\": \"de_DE\", \"name_english\": \"German\"}}";
    FakeServer server = AmyServer(kModel, germanConfig);
    const VoiceOutcome r = DownloadVoice(AmyEntry(kModel, germanConfig), server.Fn(), nullptr, dir.path);
    CHECK(r.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(server.asked.size() == 1);  // the model was never fetched
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("FetchVoiceCatalogue: fetches voices.json, keeps Lexia's Voices and cleans up its temporary file") {
    TempDir dir;
    const std::string catalogue =
        "{\"en_US-amy-medium\": {\"language\": {\"code\": \"en_US\", \"name_english\": \"English\"}, \"quality\": \"medium\", "
        "\"files\": {\"a/en_US-amy-medium.onnx\": {\"size_bytes\": 5, \"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}, "
        "\"a/en_US-amy-medium.onnx.json\": {\"size_bytes\": 5, \"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}}}, "
        "\"de_DE-t-medium\": {\"language\": {\"code\": \"de_DE\", \"name_english\": \"German\"}, \"quality\": \"medium\", "
        "\"files\": {\"d/de_DE-t-medium.onnx\": {\"size_bytes\": 5, \"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}, "
        "\"d/de_DE-t-medium.onnx.json\": {\"size_bytes\": 5, \"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}}}}";
    FakeServer server;
    server.files[std::string(kVoiceRepoUrl) + "voices.json"] = catalogue;
    const CatalogueResult r = FetchVoiceCatalogue(server.Fn(), dir.path);
    REQUIRE(r.error.ok());
    REQUIRE(r.voices.size() == 1);
    CHECK(r.voices[0].name == "en_US-amy-medium");
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("FetchVoiceCatalogue: no network, or a reply that is not a catalogue, is a Network problem") {
    TempDir dir;
    FakeServer none;
    CHECK(FetchVoiceCatalogue(none.Fn(), dir.path).error.problem == VoiceProblem::Network);
    FakeServer junk;
    junk.files[std::string(kVoiceRepoUrl) + "voices.json"] = "<html>captive portal</html>";
    CHECK(FetchVoiceCatalogue(junk.Fn(), dir.path).error.problem == VoiceProblem::Network);
    CHECK(fs::is_empty(dir.path));
}

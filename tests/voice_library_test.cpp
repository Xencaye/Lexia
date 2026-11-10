// Unit tests for tts/voice_library.h: accepting Voices, listing them, adding from files and zips.
#include <doctest/doctest.h>

#include <miniz.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tts/voice_library.h"

namespace fs = std::filesystem;

namespace {

// A folder that is removed again at the end of the test.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() / ("lexia_voice_library_test_" + std::to_string(++counter) + "_" +
                                            std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void WriteText(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << text;
}

std::string ReadText(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string Config(const std::string& code, const std::string& english, const std::string& quality) {
    return "{\"audio\": {\"sample_rate\": 22050, \"quality\": \"" + quality + "\"}, \"language\": {\"code\": \"" + code +
           "\", \"family\": \"x\", \"name_english\": \"" + english + "\"}, \"num_speakers\": 1}";
}

// Config as the real Bulgarian Voice has it: no "language" object at all.
const char* kNoLanguageConfig = "{\"audio\": {\"sample_rate\": 22050, \"quality\": \"medium\"}, \"espeak\": {\"voice\": \"bg\"}}";

// A zip with these (name, bytes) entries, written with miniz so a test can make names no sane zipper would.
std::string MakeZip(const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip{};
    REQUIRE(mz_zip_writer_init_heap(&zip, 0, 0));
    for (const auto& [name, bytes] : entries) {
        REQUIRE(mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(), 6 /* zlib level */));
    }
    void* buf = nullptr;
    size_t size = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &buf, &size));
    std::string out(static_cast<const char*>(buf), size);
    mz_zip_writer_end(&zip);
    mz_free(buf);
    return out;
}

}  // namespace

// ------------------------------------------------------------------ accepting ---

TEST_CASE("CheckVoiceConfig: a Medium Voice for a Language Lexia reads is accepted, and attached to that Language") {
    const VoiceOutcome en = CheckVoiceConfig("en_US-amy-medium", Config("en_US", "English", "medium"));
    CHECK(en.ok());
    CHECK(en.languageCode == "en");
    CHECK(en.voiceName == "en_US-amy-medium");
    const VoiceOutcome bg = CheckVoiceConfig("bg_BG-dimitar-medium", Config("bg_BG", "Bulgarian", "medium"));
    CHECK(bg.ok());
    CHECK(bg.languageCode == "bg");
}

TEST_CASE("CheckVoiceConfig: a config with no language object falls back to the Voice's name, then espeak's voice") {
    const VoiceOutcome byName = CheckVoiceConfig("bg_BG-dimitar-medium", kNoLanguageConfig);
    CHECK(byName.ok());
    CHECK(byName.languageCode == "bg");
    const VoiceOutcome byEspeak = CheckVoiceConfig("mine", "{\"audio\": {\"quality\": \"medium\"}, \"espeak\": {\"voice\": \"en-us\"}}");
    CHECK(byEspeak.ok());
    CHECK(byEspeak.languageCode == "en");
}

TEST_CASE("CheckVoiceConfig: a Voice for a language Lexia cannot read is refused, naming the language") {
    const VoiceOutcome r = CheckVoiceConfig("de_DE-thorsten-medium", Config("de_DE", "German", "medium"));
    CHECK(r.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(r.languageName == "German");
    CHECK(r.languageCode.empty());
    // No English name in the config: the code stands in for it.
    const VoiceOutcome noName = CheckVoiceConfig("fr_FR-x-medium", "{\"language\": {\"code\": \"fr_FR\"}, \"audio\": {\"quality\": \"medium\"}}");
    CHECK(noName.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(noName.languageName == "fr_FR");
}

TEST_CASE("CheckVoiceConfig: only Medium; the quality comes from the config, else the end of the name") {
    const VoiceOutcome low = CheckVoiceConfig("en_US-amy-low", Config("en_US", "English", "low"));
    CHECK(low.problem == VoiceProblem::NotMedium);
    CHECK(low.quality == "low");
    CHECK(CheckVoiceConfig("en_US-amy-high", Config("en_US", "English", "high")).problem == VoiceProblem::NotMedium);
    CHECK(CheckVoiceConfig("en_US-amy-x_low", Config("en_US", "English", "x_low")).problem == VoiceProblem::NotMedium);
    // No quality in the config: the name says it.
    CHECK(CheckVoiceConfig("en_US-amy-medium", "{\"language\": {\"code\": \"en_US\"}}").ok());
    CHECK(CheckVoiceConfig("en_US-amy-low", "{\"language\": {\"code\": \"en_US\"}}").problem == VoiceProblem::NotMedium);
    // Neither says: refused, with an empty quality.
    const VoiceOutcome unknown = CheckVoiceConfig("amy", "{\"language\": {\"code\": \"en_US\"}}");
    CHECK(unknown.problem == VoiceProblem::NotMedium);
    CHECK(unknown.quality.empty());
}

TEST_CASE("CheckVoiceConfig: a German low-quality Voice is reported as German, the language comes first") {
    CHECK(CheckVoiceConfig("de_DE-x-low", Config("de_DE", "German", "low")).problem == VoiceProblem::UnsupportedLanguage);
}

TEST_CASE("CheckVoiceConfig: text that is not a Piper config, or a name with odd characters, is refused") {
    CHECK(CheckVoiceConfig("en_US-a-medium", "not json").problem == VoiceProblem::BadConfig);
    CHECK(CheckVoiceConfig("en_US-a-medium", "[]").problem == VoiceProblem::BadConfig);
    CHECK(CheckVoiceConfig("mine", "{}").problem == VoiceProblem::BadConfig);  // no language anywhere
    for (const char* bad : {"en US-a-medium", "en_US-a-medium/../x", "..\\x", ".hidden", "", "en_US-é-medium"}) {
        CAPTURE(bad);
        CHECK(CheckVoiceConfig(bad, Config("en_US", "English", "medium")).problem == VoiceProblem::BadName);
    }
}

TEST_CASE("CheckVoiceAccepted: a Windows device name is refused, in any case and with any extension") {
    for (const char* device : {"con", "PRN", "Aux", "nul", "NUL.x", "nul.tar.gz", "com0", "COM1", "com9", "lpt0", "LPT1",
                               "lpt9.medium"}) {
        CAPTURE(device);
        CHECK(CheckVoiceAccepted(device, "en_US", "English", "medium").problem == VoiceProblem::BadName);
        CHECK(CheckVoiceConfig(device, Config("en_US", "English", "medium")).problem == VoiceProblem::BadName);
    }
    // Only the whole part before the first '.' counts.
    for (const char* fine : {"console", "com10", "comx", "lpt", "nul_voice", "en_US-con-medium", "x.nul"}) {
        CAPTURE(fine);
        CHECK(CheckVoiceAccepted(fine, "en_US", "English", "medium").ok());
    }
}

TEST_CASE("IsSafeRelativePath: refuses paths that climb out, are absolute, or carry a drive or stream") {
    CHECK(IsSafeRelativePath("voice.onnx"));
    CHECK(IsSafeRelativePath("folder/voice.onnx"));
    CHECK(IsSafeRelativePath("folder/"));
    CHECK(IsSafeRelativePath("folder\\voice.onnx"));
    CHECK(IsSafeRelativePath("folder/sub/"));
    // A folder entry ("ends with /") still has its characters checked.
    for (const char* bad : {"../voice.onnx", "a/../../b", "a\\..\\b", "/etc/x", "\\x", "C:\\x", "C:x", "x:y", "a//b", ".",
                            "", "a/./b", "a\nb", "a:b/", "C:/", "a*/", "a\nb/", "folder/a?b/"}) {
        CAPTURE(bad);
        CHECK_FALSE(IsSafeRelativePath(bad));
    }
}

// -------------------------------------------------------------- adding files ---

TEST_CASE("AddVoiceFiles: copies a .onnx and its .onnx.json into the folder and attaches the Language") {
    TempDir src, dest;
    WriteText(src.path / "en_US-amy-medium.onnx", "MODEL");
    WriteText(src.path / "en_US-amy-medium.onnx.json", Config("en_US", "English", "medium"));
    const VoiceOutcome r = AddVoiceFiles(src.path / "en_US-amy-medium.onnx", src.path / "en_US-amy-medium.onnx.json", dest.path / "voices");
    REQUIRE(r.ok());
    CHECK(r.voiceName == "en_US-amy-medium");
    CHECK(r.languageCode == "en");
    CHECK(ReadText(dest.path / "voices" / "en_US-amy-medium.onnx") == "MODEL");
    CHECK(ReadText(dest.path / "voices" / "en_US-amy-medium.onnx.json") == Config("en_US", "English", "medium"));
    CHECK_FALSE(fs::exists(dest.path / "voices" / "en_US-amy-medium.onnx.part"));  // no temporary files left
    CHECK_FALSE(fs::exists(dest.path / "voices" / "en_US-amy-medium.onnx.json.part"));
    CHECK(fs::exists(src.path / "en_US-amy-medium.onnx"));  // the source stays
}

TEST_CASE("AddVoiceFiles: a refused Voice leaves nothing in the folder") {
    TempDir src, dest;
    WriteText(src.path / "de_DE-t-medium.onnx", "MODEL");
    WriteText(src.path / "de_DE-t-medium.onnx.json", Config("de_DE", "German", "medium"));
    const VoiceOutcome r = AddVoiceFiles(src.path / "de_DE-t-medium.onnx", src.path / "de_DE-t-medium.onnx.json", dest.path);
    CHECK(r.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(r.languageName == "German");
    CHECK(fs::is_empty(dest.path));
}

TEST_CASE("AddVoiceFiles: a missing file, or files with different names, is a MissingFile") {
    TempDir src, dest;
    WriteText(src.path / "en_US-a-medium.onnx", "MODEL");
    CHECK(AddVoiceFiles(src.path / "en_US-a-medium.onnx", src.path / "en_US-a-medium.onnx.json", dest.path).problem ==
          VoiceProblem::MissingFile);
    WriteText(src.path / "other.onnx.json", Config("en_US", "English", "medium"));
    CHECK(AddVoiceFiles(src.path / "en_US-a-medium.onnx", src.path / "other.onnx.json", dest.path).problem == VoiceProblem::MissingFile);
    CHECK(AddVoiceFiles(src.path / "gone.onnx", src.path / "gone.onnx.json", dest.path).problem == VoiceProblem::MissingFile);
}

TEST_CASE("AddVoiceFiles: adding a Voice again replaces the old one") {
    TempDir src, dest;
    WriteText(src.path / "en_US-a-medium.onnx.json", Config("en_US", "English", "medium"));
    WriteText(src.path / "en_US-a-medium.onnx", "ONE");
    REQUIRE(AddVoiceFiles(src.path / "en_US-a-medium.onnx", src.path / "en_US-a-medium.onnx.json", dest.path).ok());
    WriteText(src.path / "en_US-a-medium.onnx", "TWO");
    REQUIRE(AddVoiceFiles(src.path / "en_US-a-medium.onnx", src.path / "en_US-a-medium.onnx.json", dest.path).ok());
    CHECK(ReadText(dest.path / "en_US-a-medium.onnx") == "TWO");
}

// A rename that fails for the Nth call (1-based) and works otherwise.
RenameFn FailOnCall(int n) {
    auto calls = std::make_shared<int>(0);
    return [calls, n](const fs::path& from, const fs::path& to, std::error_code& ec) {
        if (++*calls == n) {
            ec = std::make_error_code(std::errc::permission_denied);
            return;
        }
        fs::rename(from, to, ec);
    };
}

TEST_CASE("InstallVoice: a failure at any step keeps the old Voice and leaves no .old behind") {
    // Renames in order: old model aside, old config aside, new model in, new config in.
    for (int failAt = 1; failAt <= 4; ++failAt) {
        CAPTURE(failAt);
        TempDir dest;
        const std::string name = "en_US-a-medium";
        WriteText(dest.path / (name + ".onnx"), "OLD MODEL");
        WriteText(dest.path / (name + ".onnx.json"), Config("en_US", "English", "medium"));
        WriteText(dest.path / (name + ".onnx.part"), "NEW MODEL");
        WriteText(dest.path / (name + ".onnx.json.part"), Config("en_US", "English", "medium") + " ");
        const VoiceOutcome r = InstallVoice(name, dest.path, dest.path / (name + ".onnx.part"),
                                            dest.path / (name + ".onnx.json.part"), FailOnCall(failAt));
        CHECK(r.problem == VoiceProblem::CannotWrite);
        CHECK(ReadText(dest.path / (name + ".onnx")) == "OLD MODEL");
        CHECK(ReadText(dest.path / (name + ".onnx.json")) == Config("en_US", "English", "medium"));
        CHECK_FALSE(fs::exists(dest.path / (name + ".onnx.old")));
        CHECK_FALSE(fs::exists(dest.path / (name + ".onnx.json.old")));
        // The old Voice is still a usable one.
        const auto listed = ListInstalledVoices(dest.path / "none", dest.path);
        REQUIRE(listed.size() == 1);
        CHECK(listed[0].name == name);
    }
}

TEST_CASE("InstallVoice: a failure with no old Voice leaves no Voice, and success replaces the old one") {
    TempDir dest;
    const std::string name = "en_US-a-medium";
    WriteText(dest.path / (name + ".onnx.part"), "NEW MODEL");
    WriteText(dest.path / (name + ".onnx.json.part"), "{}");
    CHECK(InstallVoice(name, dest.path, dest.path / (name + ".onnx.part"), dest.path / (name + ".onnx.json.part"),
                       FailOnCall(2)).problem == VoiceProblem::CannotWrite);
    CHECK_FALSE(fs::exists(dest.path / (name + ".onnx")));
    CHECK_FALSE(fs::exists(dest.path / (name + ".onnx.json")));

    WriteText(dest.path / (name + ".onnx.part"), "NEW MODEL");  // the failed try used them up
    WriteText(dest.path / (name + ".onnx.json.part"), "{}");
    WriteText(dest.path / (name + ".onnx"), "OLD");
    WriteText(dest.path / (name + ".onnx.json"), "{old}");
    REQUIRE(InstallVoice(name, dest.path, dest.path / (name + ".onnx.part"), dest.path / (name + ".onnx.json.part")).ok());
    CHECK(ReadText(dest.path / (name + ".onnx")) == "NEW MODEL");
    CHECK(ReadText(dest.path / (name + ".onnx.json")) == "{}");
    CHECK_FALSE(fs::exists(dest.path / (name + ".onnx.old")));
    CHECK_FALSE(fs::exists(dest.path / (name + ".onnx.json.old")));
}


TEST_CASE("AddVoice: takes a .onnx, a .onnx.json or a .zip, and nothing else") {
    TempDir src, dest;
    WriteText(src.path / "en_US-a-medium.onnx", "MODEL");
    WriteText(src.path / "en_US-a-medium.onnx.json", Config("en_US", "English", "medium"));
    CHECK(AddVoice(src.path / "en_US-a-medium.onnx", dest.path / "one").ok());
    CHECK(AddVoice(src.path / "en_US-a-medium.onnx.json", dest.path / "two").ok());
    CHECK(fs::exists(dest.path / "two" / "en_US-a-medium.onnx"));
    WriteText(src.path / "notes.txt", "x");
    CHECK(AddVoice(src.path / "notes.txt", dest.path / "three").problem == VoiceProblem::MissingFile);
}

TEST_CASE("AddVoice, AddVoiceZip, ListInstalledVoices: the extension may be in capitals, as Windows allows") {
    TempDir src, dest;
    WriteText(src.path / "en_US-a-medium.ONNX", "MODEL");
    WriteText(src.path / "en_US-a-medium.ONNX.JSON", Config("en_US", "English", "medium"));
    const VoiceOutcome model = AddVoice(src.path / "en_US-a-medium.ONNX", dest.path / "one");
    CHECK(model.ok());
    CHECK(model.voiceName == "en_US-a-medium");
    CHECK(ReadText(dest.path / "one" / "en_US-a-medium.onnx") == "MODEL");  // installed with the usual extension
    CHECK(AddVoice(src.path / "en_US-a-medium.ONNX.JSON", dest.path / "two").ok());

    WriteText(src.path / "v.zip", MakeZip({{"en_US-b-medium.Onnx", "MODEL"}, {"en_US-b-medium.Onnx.Json", Config("en_US", "English", "medium")}}));
    CHECK(AddVoiceZip(src.path / "v.zip", dest.path / "three").ok());
    CHECK(fs::exists(dest.path / "three" / "en_US-b-medium.onnx"));

    WriteText(dest.path / "user" / "en_US-c-medium.ONNX", "M");
    WriteText(dest.path / "user" / "en_US-c-medium.ONNX.json", Config("en_US", "English", "medium"));
    const auto listed = ListInstalledVoices({}, dest.path / "user");
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].name == "en_US-c-medium");
}

// ------------------------------------------------------------------- zips ---

TEST_CASE("AddVoiceZip: unpacks the two files, also from one folder inside the zip") {
    TempDir dir;
    const std::string cfg = Config("bg_BG", "Bulgarian", "medium");
    WriteText(dir.path / "flat.zip", MakeZip({{"bg_BG-d-medium.onnx", "MODEL"}, {"bg_BG-d-medium.onnx.json", cfg}}));
    const VoiceOutcome flat = AddVoiceZip(dir.path / "flat.zip", dir.path / "a");
    REQUIRE(flat.ok());
    CHECK(flat.languageCode == "bg");
    CHECK(ReadText(dir.path / "a" / "bg_BG-d-medium.onnx") == "MODEL");
    CHECK(ReadText(dir.path / "a" / "bg_BG-d-medium.onnx.json") == cfg);

    WriteText(dir.path / "folder.zip", MakeZip({{"voice/", ""}, {"voice/bg_BG-d-medium.onnx", "M2"}, {"voice/bg_BG-d-medium.onnx.json", cfg}}));
    REQUIRE(AddVoiceZip(dir.path / "folder.zip", dir.path / "b").ok());
    CHECK(ReadText(dir.path / "b" / "bg_BG-d-medium.onnx") == "M2");
    CHECK_FALSE(fs::exists(dir.path / "b" / "voice"));  // the folder inside is not recreated
}

TEST_CASE("AddVoiceZip: a path that climbs out is refused and nothing is written anywhere") {
    TempDir dir;
    const std::string cfg = Config("en_US", "English", "medium");
    for (const char* evil : {"../en_US-a-medium.onnx", "sub/../../en_US-a-medium.onnx", "/en_US-a-medium.onnx",
                             "C:/en_US-a-medium.onnx", "..\\en_US-a-medium.onnx"}) {
        CAPTURE(std::string(evil));
        // miniz's writer refuses a leading '/': write '#' and patch the name in the finished zip.
        const bool leadingSlash = evil[0] == '/';
        std::string name = evil;
        if (leadingSlash) name[0] = '#';
        std::string zip = MakeZip({{name, "MODEL"}, {"en_US-a-medium.onnx.json", cfg}});
        if (leadingSlash) {
            for (size_t at = zip.find(name); at != std::string::npos; at = zip.find(name, at)) zip[at] = '/';
        }
        WriteText(dir.path / "evil.zip", zip);
        const VoiceOutcome r = AddVoiceZip(dir.path / "evil.zip", dir.path / "dest");
        CHECK(r.problem == VoiceProblem::BadZip);
        CHECK_FALSE(fs::exists(dir.path / "en_US-a-medium.onnx"));
        CHECK_FALSE(fs::exists(dir.path / "dest"));
    }
}

TEST_CASE("AddVoiceZip: more than the two files, or fewer, or names that do not pair up, is a BadZip") {
    TempDir dir;
    const std::string cfg = Config("en_US", "English", "medium");
    const std::vector<std::vector<std::pair<std::string, std::string>>> bad = {
        {{"en_US-a-medium.onnx", "M"}, {"en_US-a-medium.onnx.json", cfg}, {"MODEL_CARD", "card"}},
        {{"en_US-a-medium.onnx", "M"}},
        {{"en_US-a-medium.onnx.json", cfg}},
        {},
        {{"en_US-a-medium.onnx", "M"}, {"en_US-b-medium.onnx.json", cfg}},
        {{"en_US-a-medium.onnx", "M"}, {"en_US-b-medium.onnx", "M"}},
    };
    for (const auto& entries : bad) {
        WriteText(dir.path / "bad.zip", MakeZip(entries));
        CHECK(AddVoiceZip(dir.path / "bad.zip", dir.path / "dest").problem == VoiceProblem::BadZip);
        CHECK_FALSE(fs::exists(dir.path / "dest" / "en_US-a-medium.onnx"));
    }
}

TEST_CASE("AddVoiceZip: a Voice name that is not safe is refused before anything is written") {
    TempDir dir;
    const std::string cfg = Config("en_US", "English", "medium");
    // A space, a name that is not UTF-8 (fs::u8path would throw on it), a Windows device name.
    for (const std::string& name : {std::string("en US-a-medium"), std::string("en_US-\xFF-medium"), std::string("nul"),
                                    std::string("COM1")}) {
        CAPTURE(name);
        WriteText(dir.path / "bad.zip", MakeZip({{name + ".onnx", "MODEL"}, {name + ".onnx.json", cfg}}));
        const VoiceOutcome r = AddVoiceZip(dir.path / "bad.zip", dir.path / "dest");
        CHECK(r.problem == VoiceProblem::BadName);
        CHECK_FALSE(fs::exists(dir.path / "dest"));  // not even the temporary files
    }
}

namespace {

// Sets the uncompressed size the central directory gives for the entry `name` of `zip`.
void PatchDeclaredSize(std::string& zip, const std::string& name, std::uint32_t size) {
    // Central directory header: signature "PK\1\2", the uncompressed size at +24, the name at +46.
    const std::string signature("PK\x01\x02", 4);
    for (size_t at = zip.find(signature); at != std::string::npos; at = zip.find(signature, at + 1)) {
        if (zip.compare(at + 46, name.size(), name) != 0) continue;
        for (int i = 0; i < 4; ++i) zip[at + 24 + static_cast<size_t>(i)] = static_cast<char>((size >> (8 * i)) & 0xFF);
        return;
    }
    FAIL("the zip has no central directory entry with that name");
}

}  // namespace

TEST_CASE("AddVoiceZip: an entry that says it unpacks to more than 1 GiB is refused before anything is written") {
    TempDir dir;
    // Compressible, so the entry is deflated and its two sizes may differ.
    std::string zip = MakeZip({{"en_US-a-medium.onnx", std::string(10000, 'M')},
                               {"en_US-a-medium.onnx.json", Config("en_US", "English", "medium")}});
    PatchDeclaredSize(zip, "en_US-a-medium.onnx", 0x40000001u);  // 1 GiB + 1 byte
    WriteText(dir.path / "big.zip", zip);
    const VoiceOutcome r = AddVoiceZip(dir.path / "big.zip", dir.path / "dest");
    CHECK(r.problem == VoiceProblem::BadZip);
    CHECK_FALSE(fs::exists(dir.path / "dest"));
}

TEST_CASE("AddVoiceZip: something that is not a zip, or a zip that is not there") {
    TempDir dir;
    WriteText(dir.path / "fake.zip", "this is not a zip file at all");
    CHECK(AddVoiceZip(dir.path / "fake.zip", dir.path / "dest").problem == VoiceProblem::BadZip);
    CHECK(AddVoiceZip(dir.path / "gone.zip", dir.path / "dest").problem == VoiceProblem::MissingFile);
}

TEST_CASE("AddVoiceZip: the config is checked before the model is unpacked, and a refused Voice leaves nothing") {
    TempDir dir;
    WriteText(dir.path / "de.zip", MakeZip({{"de_DE-t-medium.onnx", std::string(10000, 'M')},
                                            {"de_DE-t-medium.onnx.json", Config("de_DE", "German", "medium")}}));
    const VoiceOutcome r = AddVoiceZip(dir.path / "de.zip", dir.path / "dest");
    CHECK(r.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(r.languageName == "German");
    CHECK(fs::is_empty(dir.path / "dest"));  // not even the temporary files
}

// ---------------------------------------------------------------- listing ---

TEST_CASE("ListInstalledVoices: the shipped Voices first, then the user's, each with its Language") {
    TempDir shipped, user;
    WriteText(shipped.path / "voices.json",
              "{\"voices\": [{\"language\": \"en\", \"name\": \"en_GB-cori-medium\"}, "
              "{\"language\": \"bg\", \"name\": \"bg_BG-gone-medium\"}]}");
    WriteText(shipped.path / "en_GB-cori-medium.onnx", "M");  // bg_BG-gone-medium has no file: not listed
    WriteText(user.path / "en_US-zoe-medium.onnx", "M");
    WriteText(user.path / "en_US-zoe-medium.onnx.json", Config("en_US", "English", "medium"));
    WriteText(user.path / "bg_BG-dimitar-medium.onnx", "M");
    WriteText(user.path / "bg_BG-dimitar-medium.onnx.json", kNoLanguageConfig);
    WriteText(user.path / "de_DE-t-medium.onnx", "M");  // not a Language Lexia reads
    WriteText(user.path / "de_DE-t-medium.onnx.json", Config("de_DE", "German", "medium"));
    WriteText(user.path / "en_US-nojson-medium.onnx", "M");  // no config
    WriteText(user.path / "en_GB-cori-medium.onnx", "M");  // the name of a shipped Voice: left out
    WriteText(user.path / "en_GB-cori-medium.onnx.json", Config("en_US", "English", "medium"));
    WriteText(user.path / "en_US-half-medium.onnx.part", "M");

    const auto voices = ListInstalledVoices(shipped.path, user.path);
    REQUIRE(voices.size() == 3);
    CHECK(voices[0].name == "en_GB-cori-medium");
    CHECK(voices[0].shipped);
    CHECK(voices[0].languageCode == "en");
    CHECK(voices[1].name == "bg_BG-dimitar-medium");
    CHECK_FALSE(voices[1].shipped);
    CHECK(voices[1].languageCode == "bg");
    CHECK(voices[2].name == "en_US-zoe-medium");
    CHECK(voices[2].model == user.path / "en_US-zoe-medium.onnx");
}

TEST_CASE("ListInstalledVoices: missing folders give an empty list, not an error") {
    CHECK(ListInstalledVoices("no/such/dir", "no/such/other").empty());
    CHECK(ListInstalledVoices({}, {}).empty());
}

// ------------------------------------------------- PickVoice (D5) ---

TEST_CASE("PickVoice: the preferred Voice if it is installed for the Language, else the first one") {
    const std::vector<InstalledVoice> list = {
        {"en_GB-cori-medium", "en", "a.onnx", true},
        {"en_GB-alba-medium", "en", "b.onnx", false},
        {"bg_BG-dimitar-medium", "bg", "c.onnx", true},
    };
    REQUIRE(PickVoice(list, "en", "en_GB-alba-medium"));
    CHECK(PickVoice(list, "en", "en_GB-alba-medium")->name == "en_GB-alba-medium");
    CHECK(PickVoice(list, "en", "")->name == "en_GB-cori-medium");
    CHECK(PickVoice(list, "en", "not-installed")->name == "en_GB-cori-medium");
    // a Voice of another Language is not a choice for this one
    CHECK(PickVoice(list, "en", "bg_BG-dimitar-medium")->name == "en_GB-cori-medium");
    CHECK(PickVoice(list, "de", "") == nullptr);
    CHECK(PickVoice({}, "en", "") == nullptr);
}

// The default English Voice is the first one voices.json lists (Cori, then Mike). A settings.json from
// Lexia 0.6 names en_US-ljspeech-medium as the active Voice; that Voice no longer ships, so the
// first listed one speaks instead, with no migration step.
TEST_CASE("PickVoice: the first Voice in voices.json is the default, also for a Voice that no longer ships") {
    TempDir shipped;
    WriteText(shipped.path / "voices.json",
              "{\"voices\": [{\"language\": \"en\", \"name\": \"en_GB-cori-medium\"}, "
              "{\"language\": \"en\", \"name\": \"en_US-mike-medium\"}]}");
    WriteText(shipped.path / "en_GB-cori-medium.onnx", "M");
    WriteText(shipped.path / "en_US-mike-medium.onnx", "M");

    const auto voices = ListInstalledVoices(shipped.path, {});
    REQUIRE(voices.size() == 2);
    CHECK(PickVoice(voices, "en", "")->name == "en_GB-cori-medium");
    CHECK(PickVoice(voices, "en", "en_US-ljspeech-medium")->name == "en_GB-cori-medium");
    CHECK(PickVoice(voices, "en", "en_US-mike-medium")->name == "en_US-mike-medium");
}

// ------------------------------------------------------------- RemoveVoice ---

TEST_CASE("RemoveVoice: deletes both files of the Voice and nothing else") {
    TempDir dir;
    WriteText(dir.path / "en_US-amy-medium.onnx", "model");
    WriteText(dir.path / "en_US-amy-medium.onnx.json", "{}");
    WriteText(dir.path / "en_US-bob-medium.onnx", "model");
    WriteText(dir.path / "en_US-bob-medium.onnx.json", "{}");

    const VoiceOutcome r = RemoveVoice(dir.path, "en_US-amy-medium");
    CHECK(r.ok());
    CHECK(r.voiceName == "en_US-amy-medium");
    CHECK_FALSE(fs::exists(dir.path / "en_US-amy-medium.onnx"));
    CHECK_FALSE(fs::exists(dir.path / "en_US-amy-medium.onnx.json"));
    CHECK(fs::exists(dir.path / "en_US-bob-medium.onnx"));
    CHECK(fs::exists(dir.path / "en_US-bob-medium.onnx.json"));
}

TEST_CASE("RemoveVoice: a Voice that is not there is MissingFile") {
    TempDir dir;
    const VoiceOutcome r = RemoveVoice(dir.path, "en_US-nobody-medium");
    CHECK(r.problem == VoiceProblem::MissingFile);
    CHECK(r.voiceName == "en_US-nobody-medium");
    CHECK(RemoveVoice(dir.path / "no_such_folder", "en_US-nobody-medium").problem == VoiceProblem::MissingFile);
}

TEST_CASE("RemoveVoice: a name with path parts or odd characters is refused, and no file is touched") {
    TempDir dir;
    WriteText(dir.path / "outside.onnx", "model");
    WriteText(dir.path / "outside.onnx.json", "{}");
    const fs::path inner = dir.path / "voices";
    fs::create_directories(inner);

    for (const char* name : {"..\\outside", "../outside", "..", ".", "a/b", "a\\b", "", "C:evil", "nul", "con"}) {
        CAPTURE(name);
        CHECK(RemoveVoice(inner, name).problem == VoiceProblem::BadName);
    }
    CHECK(fs::exists(dir.path / "outside.onnx"));
    CHECK(fs::exists(dir.path / "outside.onnx.json"));
}

TEST_CASE("RemoveVoice: a Voice with only one of its files is still removed") {
    TempDir dir;
    WriteText(dir.path / "en_US-half-medium.onnx", "model");
    CHECK(RemoveVoice(dir.path, "en_US-half-medium").ok());
    CHECK_FALSE(fs::exists(dir.path / "en_US-half-medium.onnx"));
}

TEST_CASE("RemoveVoice: the Voice is gone from ListInstalledVoices") {
    TempDir dir;
    WriteText(dir.path / "en_US-amy-medium.onnx", "model");
    WriteText(dir.path / "en_US-amy-medium.onnx.json", Config("en_US", "English", "medium"));
    REQUIRE(ListInstalledVoices({}, dir.path).size() == 1);
    CHECK(RemoveVoice(dir.path, "en_US-amy-medium").ok());
    CHECK(ListInstalledVoices({}, dir.path).empty());
}

// -------------------------------------------------- AdoptRetiredVoices (B9) ---

namespace {

// A program folder as an update of 0.6 leaves it: voices.json lists only Cori; ljspeech is a leftover.
void MakeOldProgramFolder(const fs::path& exe) {
    WriteText(exe / "voices.json", "{\"voices\": [{\"language\": \"en\", \"name\": \"en_GB-cori-medium\"}]}");
    WriteText(exe / "en_GB-cori-medium.onnx", "CORI");
    WriteText(exe / "en_GB-cori-medium.onnx.json", Config("en_GB", "English", "medium"));
    WriteText(exe / "en_US-ljspeech-medium.onnx", "LJSPEECH");
    WriteText(exe / "en_US-ljspeech-medium.onnx.json", Config("en_US", "English", "medium"));
}

}  // namespace

TEST_CASE("AdoptRetiredVoices: a retired Voice is copied and then listed as the user's own") {
    TempDir exe, user;
    MakeOldProgramFolder(exe.path);
    const fs::path dest = user.path / "voices";  // does not exist yet: made by the copy

    const auto adopted = AdoptRetiredVoices(exe.path, dest, {}).outcomes;
    REQUIRE(adopted.size() == 1);
    CHECK(adopted[0].ok());
    CHECK(adopted[0].voiceName == "en_US-ljspeech-medium");
    CHECK(ReadText(dest / "en_US-ljspeech-medium.onnx") == "LJSPEECH");
    CHECK(fs::exists(dest / "en_US-ljspeech-medium.onnx.json"));
    CHECK(ReadText(exe.path / "en_US-ljspeech-medium.onnx") == "LJSPEECH");  // nothing is deleted or moved

    const auto listed = ListInstalledVoices(exe.path, dest);
    REQUIRE(listed.size() == 2);
    CHECK(listed[0].name == "en_GB-cori-medium");
    CHECK(listed[0].shipped);
    CHECK(listed[1].name == "en_US-ljspeech-medium");
    CHECK_FALSE(listed[1].shipped);
    CHECK(listed[1].languageCode == "en");
    CHECK(fs::exists(exe.path / "en_US-ljspeech-medium.onnx.json"));
}

TEST_CASE("AdoptRetiredVoices: a Voice that voices.json lists is shipped, not retired") {
    TempDir exe, user;
    MakeOldProgramFolder(exe.path);
    AdoptRetiredVoices(exe.path, user.path, {});
    CHECK_FALSE(fs::exists(user.path / "en_GB-cori-medium.onnx"));
    CHECK_FALSE(fs::exists(user.path / "en_GB-cori-medium.onnx.json"));
}

TEST_CASE("AdoptRetiredVoices: a Voice of that name in the user's folder is never replaced") {
    TempDir exe, user;
    MakeOldProgramFolder(exe.path);
    WriteText(user.path / "en_US-ljspeech-medium.onnx", "MINE");
    WriteText(user.path / "en_US-ljspeech-medium.onnx.json", Config("en_US", "English", "medium"));
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).outcomes.empty());
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).handled == std::vector<std::string>{"en_US-ljspeech-medium"});  // present = handled
    CHECK(ReadText(user.path / "en_US-ljspeech-medium.onnx") == "MINE");
}

TEST_CASE("AdoptRetiredVoices: a bad config, a missing config or an odd name is skipped") {
    TempDir exe, user;
    WriteText(exe.path / "voices.json", "{\"voices\": []}");
    WriteText(exe.path / "en_US-low-low.onnx", "M");  // quality low: refused, and reported
    WriteText(exe.path / "en_US-low-low.onnx.json", Config("en_US", "English", "low"));
    WriteText(exe.path / "en_US-junk-medium.onnx", "M");  // not JSON: refused, and reported
    WriteText(exe.path / "en_US-junk-medium.onnx.json", "not json");
    WriteText(exe.path / "en_US-nojson-medium.onnx", "M");  // no config file: skipped
    WriteText(exe.path / "nul.onnx", "M");  // a Windows device name: skipped
    WriteText(exe.path / "nul.onnx.json", Config("en_US", "English", "medium"));
    WriteText(exe.path / "bad name.onnx", "M");  // a space: skipped
    WriteText(exe.path / "bad name.onnx.json", Config("en_US", "English", "medium"));

    const auto results = AdoptRetiredVoices(exe.path, user.path, {}).outcomes;
    REQUIRE(results.size() == 2);
    for (const VoiceOutcome& o : results) CHECK_FALSE(o.ok());
    CHECK(ListInstalledVoices({}, user.path).empty());
    CHECK_FALSE(fs::exists(user.path / "en_US-low-low.onnx"));
    CHECK_FALSE(fs::exists(user.path / "en_US-junk-medium.onnx"));
    CHECK_FALSE(fs::exists(user.path / "en_US-nojson-medium.onnx"));
    CHECK_FALSE(fs::exists(user.path / "nul.onnx"));
}

TEST_CASE("AdoptRetiredVoices: without a readable voices.json nothing is adopted") {
    TempDir exe, user;
    WriteText(exe.path / "en_US-ljspeech-medium.onnx", "LJSPEECH");
    WriteText(exe.path / "en_US-ljspeech-medium.onnx.json", Config("en_US", "English", "medium"));
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).outcomes.empty());  // no voices.json
    CHECK_FALSE(fs::exists(user.path / "en_US-ljspeech-medium.onnx"));
    WriteText(exe.path / "voices.json", "not json at all");  // unreadable list
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).outcomes.empty());
    CHECK_FALSE(fs::exists(user.path / "en_US-ljspeech-medium.onnx"));
}

TEST_CASE("AdoptRetiredVoices: a second start adopts nothing; empty or missing folders do nothing") {
    TempDir exe, user;
    MakeOldProgramFolder(exe.path);
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).outcomes.size() == 1);
    CHECK(AdoptRetiredVoices(exe.path, user.path, {}).outcomes.empty());
    CHECK(AdoptRetiredVoices({}, user.path, {}).outcomes.empty());
    CHECK(AdoptRetiredVoices(exe.path, {}, {}).outcomes.empty());
    CHECK(AdoptRetiredVoices(exe.path / "no" / "such", user.path, {}).outcomes.empty());
}

TEST_CASE("AdoptRetiredVoices: a Voice the person deleted is not adopted again once it is handled") {
    TempDir exe, user;
    MakeOldProgramFolder(exe.path);
    const AdoptResult first = AdoptRetiredVoices(exe.path, user.path, {});
    REQUIRE(first.outcomes.size() == 1);
    CHECK(first.handled == std::vector<std::string>{"en_US-ljspeech-medium"});

    // The person deletes it on the Voices page; its files are still in the program folder.
    REQUIRE(RemoveVoice(user.path, "en_US-ljspeech-medium").ok());
    CHECK_FALSE(fs::exists(user.path / "en_US-ljspeech-medium.onnx"));

    const AdoptResult again = AdoptRetiredVoices(exe.path, user.path, first.handled);
    CHECK(again.outcomes.empty());
    CHECK(again.handled.empty());
    CHECK_FALSE(fs::exists(user.path / "en_US-ljspeech-medium.onnx"));
    CHECK(fs::exists(exe.path / "en_US-ljspeech-medium.onnx"));  // nothing in the program folder is touched
}

TEST_CASE("AdoptRetiredVoices: a refused Voice is not handled, so it is tried again") {
    TempDir exe, user;
    WriteText(exe.path / "voices.json", "{\"voices\": []}");
    WriteText(exe.path / "en_US-low-low.onnx", "M");
    WriteText(exe.path / "en_US-low-low.onnx.json", Config("en_US", "English", "low"));  // refused: not Medium
    const AdoptResult r = AdoptRetiredVoices(exe.path, user.path, {});
    REQUIRE(r.outcomes.size() == 1);
    CHECK_FALSE(r.outcomes[0].ok());
    CHECK(r.handled.empty());
}

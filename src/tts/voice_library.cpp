#include "tts/voice_library.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

#include <miniz.h>
#include <nlohmann/json.hpp>

#include "common.h"
#include "language.h"
#include "tts/voice_files.h"
#include "tts/voices.h"

namespace fs = std::filesystem;

namespace {

constexpr std::uintmax_t kMaxUnpackedBytes = 1024ull * 1024 * 1024;  // 1 GiB over both files: a Medium Voice is ~60 MB
constexpr std::uintmax_t kMaxConfigBytes = 4ull * 1024 * 1024;       // a config is ~5 KB

// A Voice name that is safe as a file name: letters, digits, "_", "-" and "." only, no "..", not
// starting with ".", at most 120 characters ("en_US-ljspeech-medium").
bool SafeName(const std::string& name) {
    if (name.empty() || name.size() > 120 || name[0] == '.') return false;
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return name.find("..") == std::string::npos;
}

// Windows opens a device, not a file, for these names, whatever follows the first '.'
// ("nul.onnx.json"). CONIN$ and CONOUT$ need a '$', which SafeName refuses already.
bool DeviceName(const std::string& name) {
    const std::string stem = ToLowerAscii(name.substr(0, name.find('.')));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return true;
    return stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0) &&
           std::isdigit(static_cast<unsigned char>(stem[3]));
}

// "bg_BG" / "en-us" / "bg" -> "bg"
std::string Family(const std::string& code) {
    return ToLowerAscii(code.substr(0, code.find_first_of("_-")));
}

// BadName unless `name` can be a Voice's file name; ok() otherwise. Checked before the name
// is used in a path.
VoiceOutcome CheckName(const std::string& name) {
    if (!SafeName(name)) {
        return Fail(VoiceProblem::BadName, name,
                    "the Voice name '" + name + "' has characters other than letters, digits, '_', '-' and '.'");
    }
    if (DeviceName(name)) {
        return Fail(VoiceProblem::BadName, name, "the Voice name '" + name + "' is a name Windows keeps for a device");
    }
    return {};
}

// ----------------------------------------------------------------- zip ---

size_t ZipRead(void* opaque, mz_uint64 offset, void* buf, size_t n) {
    std::ifstream& in = *static_cast<std::ifstream*>(opaque);
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset));
    if (!in) return 0;
    in.read(static_cast<char*>(buf), static_cast<std::streamsize>(n));
    return static_cast<size_t>(in.gcount());
}

struct ZipSink {
    std::ofstream out;
    mz_uint64 limit = 0;
    mz_uint64 written = 0;
};

// miniz's write callback while one zip entry is unpacked: writes to the sink's file and stops (returns
// 0) as soon as the entry gives more bytes than its header said, so a zip bomb cannot fill the disk.
size_t ZipWrite(void* opaque, mz_uint64, const void* buf, size_t n) {
    ZipSink& sink = *static_cast<ZipSink*>(opaque);
    if (n > sink.limit - sink.written) return 0;  // more than the entry said: stops the extraction
    sink.out.write(static_cast<const char*>(buf), static_cast<std::streamsize>(n));
    if (!sink.out) return 0;
    sink.written += n;
    return n;
}

struct ZipReader {
    std::ifstream in;
    mz_zip_archive zip{};
    bool open = false;
    ~ZipReader() {
        if (open) mz_zip_reader_end(&zip);
    }
};

// Unpacks entry `index` to file `to`; true only if exactly `size` bytes were written.
bool ExtractEntry(ZipReader& z, mz_uint index, mz_uint64 size, const fs::path& to) {
    ZipSink sink;
    sink.out.open(to, std::ios::binary | std::ios::trunc);
    if (!sink.out) return false;
    sink.limit = size;
    if (!mz_zip_reader_extract_to_callback(&z.zip, index, ZipWrite, &sink, 0)) return false;
    sink.out.close();
    return sink.written == size && !sink.out.fail();
}

}  // namespace

// A name the user chose ("my-voice") must not be read as a quality ("voice"): only Piper's four words count.
std::string VoiceQualityFromName(const std::string& name) {
    const size_t dash = name.rfind('-');
    if (dash == std::string::npos) return {};
    const std::string q = ToLowerAscii(name.substr(dash + 1));
    return IsKnownVoiceQuality(q) ? q : std::string();
}

const char* ProblemId(VoiceProblem problem) {
    switch (problem) {
        case VoiceProblem::None: return "ok";
        case VoiceProblem::UnsupportedLanguage: return "unsupported-language";
        case VoiceProblem::NotMedium: return "not-medium";
        case VoiceProblem::UnknownQuality: return "unknown-quality";
        case VoiceProblem::BadName: return "bad-name";
        case VoiceProblem::MissingFile: return "missing-file";
        case VoiceProblem::BadConfig: return "bad-config";
        case VoiceProblem::BadZip: return "bad-zip";
        case VoiceProblem::CannotWrite: return "cannot-write";
        case VoiceProblem::NotInCatalogue: return "not-in-catalogue";
        case VoiceProblem::Network: return "network";
        case VoiceProblem::BadDownload: return "bad-download";
        case VoiceProblem::Cancelled: return "cancelled";
        case VoiceProblem::Busy: return "unknown";  // the page has no text of its own for it
    }
    return "unknown";
}

bool IsSafeRelativePath(const std::string& path) {
    if (path.empty() || path[0] == '/' || path[0] == '\\') return false;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of("/\\", start);
        if (end == std::string::npos) end = path.size();
        const std::string part = path.substr(start, end - start);
        if (part.empty()) {
            // An empty part is only fine as the end of a folder entry ("folder/"), and the
            // characters are still checked below.
            if (end == path.size() && start > 0) break;
            return false;
        }
        if (part == "." || part == "..") return false;
        start = end + 1;
    }
    for (unsigned char c : path) {
        if (c < 0x20 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    return true;
}

// Piper's quality names, as its configs and the catalogue write them.
bool IsKnownVoiceQuality(const std::string& quality) {
    return quality == "x_low" || quality == "low" || quality == "medium" || quality == "high";
}

VoiceOutcome CheckVoiceAccepted(const std::string& name, const std::string& languageCode,
                                const std::string& languageName, const std::string& quality, QualityRule rule) {
    if (VoiceOutcome bad = CheckName(name); !bad.ok()) return bad;
    const Language* lang = FindLanguageByCode(Family(languageCode));
    if (!lang) {
        VoiceOutcome o = Fail(VoiceProblem::UnsupportedLanguage, name,
                              "Lexia cannot read " + (languageName.empty() ? languageCode : languageName) + " yet");
        o.languageName = languageName.empty() ? languageCode : languageName;
        return o;
    }
    const std::string q = ToLowerAscii(quality);
    if (rule == QualityRule::AnyKnown) {
        // A file the user chose: any of Piper's qualities, or none stated. A word that is not a quality
        // ("ultra") means the config is not what it claims to be, so it is refused by name.
        if (!q.empty() && !IsKnownVoiceQuality(q)) {
            VoiceOutcome o = Fail(VoiceProblem::UnknownQuality, name,
                                  "the Voice's quality '" + q + "' is not one of x_low, low, medium or high");
            o.quality = q;
            return o;
        }
    } else if (q != "medium") {
        VoiceOutcome o = Fail(VoiceProblem::NotMedium, name,
                              q.empty() ? "the Voice's quality is not stated; only Medium Voices are accepted"
                                        : "the Voice's quality is " + q + "; only Medium Voices are accepted");
        o.quality = q;
        return o;
    }
    VoiceOutcome o;
    o.voiceName = name;
    o.languageCode = lang->code;
    o.languageName = lang->name;
    o.quality = q;
    return o;
}

bool ParseVoiceConfig(const std::string& name, const std::string& configText, VoiceConfig* out) {
    const nlohmann::json doc = nlohmann::json::parse(configText, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) return false;
    VoiceConfig c;
    const auto lang = doc.find("language");
    if (lang != doc.end()) {
        c.languageCode = StringOf(*lang, "code");
        c.languageName = StringOf(*lang, "name_english");
    }
    if (c.languageCode.empty()) {  // "bg_BG-sample-medium" -> "bg_BG"
        const size_t dash = name.find('-');
        if (dash != std::string::npos && dash > 0) c.languageCode = name.substr(0, dash);
    }
    if (c.languageCode.empty()) {
        const auto espeak = doc.find("espeak");
        if (espeak != doc.end()) c.languageCode = StringOf(*espeak, "voice");
    }
    if (c.languageCode.empty()) return false;
    const auto audio = doc.find("audio");
    if (audio != doc.end()) c.quality = ToLowerAscii(StringOf(*audio, "quality"));
    if (c.quality.empty()) c.quality = VoiceQualityFromName(name);  // only a real quality word, never part of a user's name
    *out = c;
    return true;
}

VoiceOutcome CheckVoiceConfig(const std::string& name, const std::string& configText, QualityRule rule) {
    VoiceConfig c;
    if (!ParseVoiceConfig(name, configText, &c)) {
        return Fail(VoiceProblem::BadConfig, name,
                    "the .onnx.json is not a Piper Voice config (not JSON, or it names no language)");
    }
    return CheckVoiceAccepted(name, c.languageCode, c.languageName, c.quality, rule);
}

std::vector<InstalledVoice> ListInstalledVoices(const fs::path& voicesDir, const fs::path& userDir) {
    std::vector<InstalledVoice> out;
    try {
        std::error_code ec;
        if (!voicesDir.empty()) {
            std::string text;
            if (ReadFileToString(voicesDir / "voices.json", &text, kMaxConfigBytes)) {
                for (const VoiceEntry& e : ParseVoiceList(text)) {
                    const fs::path model = voicesDir / fs::u8path(e.name + ".onnx");
                    if (fs::is_regular_file(model, ec)) out.push_back({e.name, e.language, model, true, VoiceQualityFromName(e.name)});
                }
            }
        }
        if (!userDir.empty() && fs::is_directory(userDir, ec)) {
            std::vector<InstalledVoice> user;
            for (const fs::directory_entry& f : fs::directory_iterator(userDir, ec)) {
                if (!f.is_regular_file(ec) || !HasExtension(f.path().filename().u8string(), ".onnx")) continue;
                const std::string name = f.path().stem().u8string();
                const bool shipped = std::any_of(out.begin(), out.end(), [&](const InstalledVoice& v) { return v.name == name; });
                std::string config;
                if (shipped || !ReadFileToString(fs::path(f.path()) += ".json", &config, kMaxConfigBytes)) continue;
                // The user's own folder: any of Piper's qualities (QualityRule::AnyKnown), as when the Voice was added.
                const VoiceOutcome checked = CheckVoiceConfig(name, config, QualityRule::AnyKnown);
                if (checked.ok()) user.push_back({name, checked.languageCode, f.path(), false, checked.quality});
            }
            std::sort(user.begin(), user.end(), [](const InstalledVoice& a, const InstalledVoice& b) { return a.name < b.name; });
            out.insert(out.end(), user.begin(), user.end());
        }
    } catch (const std::exception&) {
        // A folder that cannot be read: the Voices found so far are what there is.
    }
    return out;
}

AdoptResult AdoptRetiredVoices(const fs::path& exeVoicesDir, const fs::path& userVoicesDir,
                               const std::vector<std::string>& alreadyHandled) {
    AdoptResult result;
    if (exeVoicesDir.empty() || userVoicesDir.empty()) return result;
    try {
        std::error_code ec;
        // Which Voices are shipped: without a readable voices.json nothing can be called retired, so
        // nothing is copied (a half-installed folder must not be copied into the user's data).
        std::string listText;
        if (!ReadFileToString(exeVoicesDir / "voices.json", &listText, kMaxConfigBytes)) return result;
        std::string listError;
        const std::vector<VoiceEntry> shipped = ParseVoiceList(listText, &listError);
        if (!listError.empty() && shipped.empty()) return result;
        if (!fs::is_directory(exeVoicesDir, ec)) return result;

        // A plain file only: symlink_status does not follow a link, so a link or junction is not "regular".
        auto isPlainFile = [](const fs::path& p) {
            std::error_code e;
            const fs::file_status st = fs::symlink_status(p, e);
            return !e && st.type() == fs::file_type::regular;
        };
        // Collect first, copy after: the folders are not changed while they are iterated.
        std::vector<fs::path> models;
        for (const fs::directory_entry& f : fs::directory_iterator(exeVoicesDir, ec)) {
            if (isPlainFile(f.path()) && HasExtension(f.path().filename().u8string(), ".onnx")) models.push_back(f.path());
        }
        std::sort(models.begin(), models.end());  // a stable order for the log and the tests
        for (const fs::path& model : models) {
            const std::string name = model.stem().u8string();
            if (!CheckName(name).ok()) continue;
            if (std::any_of(shipped.begin(), shipped.end(), [&](const VoiceEntry& e) { return e.name == name; })) continue;
            const fs::path config = fs::path(model) += ".json";
            if (std::find(alreadyHandled.begin(), alreadyHandled.end(), name) != alreadyHandled.end()) continue;  // done before: a Voice the person deleted stays deleted
            if (!isPlainFile(config)) continue;
            // Never replace: a Voice of this name in the user's folder is the user's. Any entry of that
            // name counts (a link too), so nothing is written over it.
            const fs::path present = userVoicesDir / fs::u8path(name + ".onnx");
            if (fs::exists(fs::symlink_status(present, ec))) {
                result.handled.push_back(name);  // the person has this Voice already: nothing to do, and never again
                continue;
            }
            result.outcomes.push_back(AddVoiceFiles(model, config, userVoicesDir));
            // Handled only when it is really in place: a refusal (bad config, disk full ...) is tried again next start.
            if (result.outcomes.back().ok()) result.handled.push_back(name);
        }
    } catch (const std::exception&) {
        // A folder that cannot be read (or a name that is not UTF-8): what was done so far is what there is.
    }
    return result;
}

VoiceOutcome AddVoiceFiles(const fs::path& model, const fs::path& config, const fs::path& destDir) {
    const std::string modelFile = model.filename().u8string();
    const std::string configFile = config.filename().u8string();
    if (!HasExtension(modelFile, ".onnx") || ToLowerAscii(configFile) != ToLowerAscii(modelFile) + ".json") {
        return Fail(VoiceProblem::MissingFile, {},
                    "a Voice is <name>.onnx and <name>.onnx.json; got '" + modelFile + "' and '" + configFile + "'");
    }
    const std::string name = modelFile.substr(0, modelFile.size() - 5);
    std::string configText;
    if (!ReadFileToString(config, &configText, kMaxConfigBytes)) {
        return Fail(VoiceProblem::MissingFile, name, "cannot read " + configFile);
    }
    std::error_code ec;
    if (!fs::is_regular_file(model, ec)) return Fail(VoiceProblem::MissingFile, name, "cannot read " + modelFile);
    const VoiceOutcome checked = CheckVoiceConfig(name, configText, QualityRule::AnyKnown);  // the user's own file
    if (!checked.ok()) return checked;

    VoiceParts parts;
    if (VoiceOutcome bad = PrepareVoiceParts(name, destDir, &parts); !bad.ok()) return bad;
    fs::copy_file(model, parts.model, fs::copy_options::overwrite_existing, ec);
    if (ec) return Fail(VoiceProblem::CannotWrite, name, "cannot copy the model: " + ec.message());
    // The config put in place is the text that was checked, not a second read of a file that
    // may have changed since.
    {
        std::ofstream out(parts.config, std::ios::binary | std::ios::trunc);
        out.write(configText.data(), static_cast<std::streamsize>(configText.size()));
        out.close();
        if (!out) return Fail(VoiceProblem::CannotWrite, name, "cannot write the config");
    }
    return FinishVoice(checked, destDir, parts);
}

VoiceOutcome AddVoiceZip(const fs::path& zipFile, const fs::path& destDir) {
    ZipReader z;
    z.in.open(zipFile, std::ios::binary);
    if (!z.in) return Fail(VoiceProblem::MissingFile, {}, "cannot read " + zipFile.u8string());
    z.in.seekg(0, std::ios::end);
    const auto size = static_cast<mz_uint64>(z.in.tellg());
    z.zip.m_pRead = ZipRead;
    z.zip.m_pIO_opaque = &z.in;
    if (!mz_zip_reader_init(&z.zip, size, 0)) return Fail(VoiceProblem::BadZip, {}, "not a zip file");
    z.open = true;

    // The whole listing is checked before anything is written.
    struct Entry { mz_uint index; mz_uint64 size; std::string base; };
    std::vector<Entry> files;
    mz_uint64 total = 0;
    const mz_uint count = mz_zip_reader_get_num_files(&z.zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z.zip, i, &st)) return Fail(VoiceProblem::BadZip, {}, "damaged zip");
        const std::string path = st.m_filename;
        if (!IsSafeRelativePath(path)) return Fail(VoiceProblem::BadZip, {}, "the zip has an unsafe path: " + path);
        if (st.m_is_directory) continue;
        if (st.m_is_encrypted || !st.m_is_supported) {
            return Fail(VoiceProblem::BadZip, {}, "the zip has an encrypted or unsupported entry: " + path);
        }
        // Compared before adding, so a zip64 size near 2^64 cannot wrap `total` back under the limit.
        if (st.m_uncomp_size > kMaxUnpackedBytes - total) {
            return Fail(VoiceProblem::BadZip, {}, "the zip unpacks to more than 1 GiB");
        }
        total += st.m_uncomp_size;
        const size_t slash = path.find_last_of("/\\");
        files.push_back({i, st.m_uncomp_size, path.substr(slash == std::string::npos ? 0 : slash + 1)});
    }
    if (files.size() != 2) {
        return Fail(VoiceProblem::BadZip, {},
                    "a Voice zip holds exactly two files, <name>.onnx and <name>.onnx.json; this has " + std::to_string(files.size()));
    }
    const Entry* model = nullptr;
    const Entry* config = nullptr;
    for (const Entry& e : files) {
        if (HasExtension(e.base, ".onnx.json")) config = &e;
        else if (HasExtension(e.base, ".onnx")) model = &e;
    }
    if (!model || !config || ToLowerAscii(config->base) != ToLowerAscii(model->base) + ".json") {
        return Fail(VoiceProblem::BadZip, {}, "a Voice zip holds <name>.onnx and <name>.onnx.json with the same name");
    }
    const std::string name = model->base.substr(0, model->base.size() - 5);
    // Before the name goes into a path: the temporary files below are named after it, and
    // fs::u8path throws on a name that is not UTF-8.
    if (VoiceOutcome bad = CheckName(name); !bad.ok()) return bad;
    if (config->size > kMaxConfigBytes) return Fail(VoiceProblem::BadZip, name, "the config is too big");

    VoiceParts parts;
    if (VoiceOutcome bad = PrepareVoiceParts(name, destDir, &parts); !bad.ok()) return bad;

    // Config first: it is small, and a refused Voice never unpacks its model.
    if (!ExtractEntry(z, config->index, config->size, parts.config)) {
        return Fail(VoiceProblem::BadZip, name, "cannot unpack the config");
    }
    std::string configText;
    if (!ReadFileToString(parts.config, &configText, kMaxConfigBytes)) {
        return Fail(VoiceProblem::CannotWrite, name, "cannot read back the config");
    }
    const VoiceOutcome checked = CheckVoiceConfig(name, configText, QualityRule::AnyKnown);  // the user's own file
    if (!checked.ok()) return checked;
    if (!ExtractEntry(z, model->index, model->size, parts.model)) {
        return Fail(VoiceProblem::BadZip, name, "cannot unpack the model (damaged zip, or the disk is full)");
    }
    return FinishVoice(checked, destDir, parts);
}

VoiceOutcome AddVoice(const fs::path& path, const fs::path& destDir) {
    const std::string file = path.filename().u8string();
    if (HasExtension(file, ".zip")) return AddVoiceZip(path, destDir);
    if (HasExtension(file, ".onnx.json")) {
        fs::path model = path;
        model.replace_extension();  // drops ".json"
        return AddVoiceFiles(model, path, destDir);
    }
    if (HasExtension(file, ".onnx")) return AddVoiceFiles(path, fs::path(path) += ".json", destDir);
    return Fail(VoiceProblem::MissingFile, {}, "give a .zip, a .onnx or a .onnx.json file");
}

VoiceOutcome InstallVoice(const std::string& name, const fs::path& destDir, const fs::path& modelPart,
                          const fs::path& configPart, const RenameFn& renameFn) {
    const RenameFn move = renameFn ? renameFn
                                   : RenameFn([](const fs::path& from, const fs::path& to, std::error_code& ec) {
                                         fs::rename(from, to, ec);
                                     });
    const fs::path model = destDir / fs::u8path(name + ".onnx");
    const fs::path config = destDir / fs::u8path(name + ".onnx.json");
    const fs::path modelOld = destDir / fs::u8path(name + ".onnx.old");
    const fs::path configOld = destDir / fs::u8path(name + ".onnx.json.old");

    std::error_code ec;
    fs::remove(modelOld, ec);  // leftovers of an install that was cut short
    fs::remove(configOld, ec);

    // An old Voice is moved aside, not overwritten, so that it can come back if the new one cannot
    // be put in. The undo uses plain renames, not the seam.
    const bool hadModel = fs::exists(model, ec);
    const bool hadConfig = fs::exists(config, ec);
    bool modelAside = false;
    bool configAside = false;
    bool modelIn = false;
    bool configIn = false;
    auto rollBack = [&]() {
        std::error_code e;
        if (modelIn) fs::remove(model, e);
        if (configIn) fs::remove(config, e);
        if (modelAside) fs::rename(modelOld, model, e);
        if (configAside) fs::rename(configOld, config, e);
    };

    if (hadModel) {
        move(model, modelOld, ec);
        if (ec) return Fail(VoiceProblem::CannotWrite, name, "could not move the old model aside: " + ec.message());
        modelAside = true;
    }
    if (hadConfig) {
        move(config, configOld, ec);
        if (ec) {
            rollBack();
            return Fail(VoiceProblem::CannotWrite, name, "could not move the old config aside: " + ec.message());
        }
        configAside = true;
    }
    move(modelPart, model, ec);
    if (ec) {
        rollBack();
        return Fail(VoiceProblem::CannotWrite, name, "could not move the model into place: " + ec.message());
    }
    modelIn = true;
    move(configPart, config, ec);
    if (ec) {
        rollBack();
        return Fail(VoiceProblem::CannotWrite, name, "could not move the config into place: " + ec.message());
    }
    fs::remove(modelOld, ec);  // both are in place: the old Voice can go
    fs::remove(configOld, ec);
    VoiceOutcome ok;
    ok.voiceName = name;
    return ok;
}

const InstalledVoice* PickVoice(const std::vector<InstalledVoice>& installed, const std::string& languageCode,
                                const std::string& preferredName) {
    const InstalledVoice* first = nullptr;
    for (const InstalledVoice& v : installed) {
        if (v.languageCode != languageCode) continue;
        if (!preferredName.empty() && v.name == preferredName) return &v;
        if (!first) first = &v;
    }
    return first;
}

VoiceOutcome RemoveVoice(const fs::path& userVoicesDir, std::string_view name) {
    const std::string voice(name);
    const VoiceOutcome bad = CheckName(voice);  // before the name goes into a path
    if (!bad.ok()) return bad;

    const fs::path files[2] = {userVoicesDir / (voice + ".onnx"), userVoicesDir / (voice + ".onnx.json")};
    bool any = false;
    for (const fs::path& file : files) {
        std::error_code ec;
        if (!fs::exists(file, ec)) continue;
        any = true;
        fs::remove(file, ec);
        if (ec) {
            return Fail(VoiceProblem::CannotWrite, voice,
                        "could not delete '" + file.filename().u8string() + "': " + ec.message());
        }
    }
    if (!any) return Fail(VoiceProblem::MissingFile, voice, "the Voice '" + voice + "' is not in the Voice folder");
    VoiceOutcome ok;
    ok.voiceName = voice;
    return ok;
}

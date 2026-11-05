#include "tts/voice_files.h"

#include <system_error>

#include <nlohmann/json.hpp>

#include "common.h"

// Helpers that the Voice library's two ways in (add from file, download) share.
namespace fs = std::filesystem;

// A refusal: the problem, the Voice it is about, and a developer detail for the log (never UI text).
VoiceOutcome Fail(VoiceProblem problem, const std::string& name, const std::string& detail) {
    VoiceOutcome o;
    o.problem = problem;
    o.voiceName = name;
    o.detail = detail;
    return o;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// `ext` must be lower case: "Voice.ONNX" has the extension ".onnx".
bool HasExtension(const std::string& file, const std::string& ext) { return EndsWith(ToLowerAscii(file), ext); }

// parent[key] when it is a string, else "" (missing key, other type, or parent not an object).
std::string StringOf(const nlohmann::json& parent, const char* key) {
    if (!parent.is_object()) return {};
    const auto it = parent.find(key);
    return it != parent.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// Deletes the temporary files that are still listed, so a failed or cancelled add leaves nothing
// behind. Done() empties the list once the files are installed. Errors are ignored: a file that is
// already gone is fine.
TempFiles::~TempFiles() {
    for (const fs::path& f : files) {
        std::error_code ec;
        fs::remove(f, ec);
    }
}

// Step 1 of an add: the two files are first written as <name>.onnx.part and <name>.onnx.json.part in
// the Voices folder, so a half-written Voice is never seen as installed.
VoiceOutcome PrepareVoiceParts(const std::string& name, const fs::path& destDir, VoiceParts* parts) {
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) return Fail(VoiceProblem::CannotWrite, name, "cannot create " + destDir.u8string());
    parts->model = destDir / fs::u8path(name + ".onnx.part");
    parts->config = destDir / fs::u8path(name + ".onnx.json.part");
    parts->temp.files = {parts->model, parts->config};
    return {};
}

// Step 2, after the parts were checked: rename them into place (InstallVoice) and keep them.
// Returns `checked` (the Voice's name and Language) on success, else the install refusal.
VoiceOutcome FinishVoice(const VoiceOutcome& checked, const fs::path& destDir, VoiceParts& parts) {
    const VoiceOutcome r = InstallVoice(checked.voiceName, destDir, parts.model, parts.config);
    if (!r.ok()) return r;
    parts.temp.Done();
    return checked;
}

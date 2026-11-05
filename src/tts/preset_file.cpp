#include "tts/preset_file.h"

#include <cmath>

#include <nlohmann/json.hpp>

// The Preset file (D8): a Voice's controls saved by the user as a small JSON file, to load again later
// or share. The format:
//   { "lexiaPreset": 1, "voice": "<Voice name>", "speed": 1.0, "pitch": 0.0, "volume": 1.0 }
// "lexiaPreset" is the format version and marks the file as Lexia's. On load, a control that is missing
// or not a number keeps its default, a value out of range is clamped, and other keys are ignored.
namespace {

// Three decimals: the controls' steps are coarser, and the text reads 0.9, not 0.8999999761581421.
double Short(float v) { return std::round(static_cast<double>(v) * 1000.0) / 1000.0; }

void ReadControl(const nlohmann::json& j, const char* key, VoiceControl control, VoiceControls& out) {
    const auto it = j.find(key);
    if (it != j.end() && it->is_number()) Set(out, control, it->get<float>());
}

}  // namespace

std::string PresetToJson(const VoiceControls& controls, std::string_view voice) {
    nlohmann::json j;
    j["lexiaPreset"] = 1;
    j["voice"] = std::string(voice);
    j["speed"] = Short(ClampControl(VoiceControl::Speed, Get(controls, VoiceControl::Speed)));
    j["pitch"] = Short(ClampControl(VoiceControl::Pitch, Get(controls, VoiceControl::Pitch)));
    j["volume"] = Short(ClampControl(VoiceControl::Volume, Get(controls, VoiceControl::Volume)));
    return j.dump(2) + "\n";
}

// The problem name sent to the page (presetFailed). Both kinds of bad file show the same text there.
const char* PresetFileProblemId(PresetFileProblem problem) {
    switch (problem) {
        case PresetFileProblem::None: return "ok";
        case PresetFileProblem::NotJson:
        case PresetFileProblem::NotAPreset: return "badFile";
    }
    return "badFile";
}

PresetFileOutcome ParsePresetFile(std::string_view text) {
    PresetFileOutcome out;
    const nlohmann::json j = nlohmann::json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        out.problem = PresetFileProblem::NotJson;
        return out;
    }
    const auto version = j.is_object() ? j.find("lexiaPreset") : j.end();
    if (!j.is_object() || version == j.end() || !version->is_number_integer() || version->get<long long>() != 1) {
        out.problem = PresetFileProblem::NotAPreset;
        return out;
    }
    const auto voice = j.find("voice");
    if (voice != j.end() && voice->is_string()) out.voice = voice->get<std::string>();
    ReadControl(j, "speed", VoiceControl::Speed, out.controls);
    ReadControl(j, "pitch", VoiceControl::Pitch, out.controls);
    ReadControl(j, "volume", VoiceControl::Volume, out.controls);
    return out;
}

// %LOCALAPPDATA%\Lexia\presets: where the file dialogs open. Empty when the data folder is not known.
std::filesystem::path PresetsDir(const std::filesystem::path& dataDir) {
    return dataDir.empty() ? dataDir : dataDir / L"presets";
}

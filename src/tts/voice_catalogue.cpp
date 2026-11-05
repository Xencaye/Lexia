#include "tts/voice_catalogue.h"

#include <algorithm>
#include <cctype>

#include <nlohmann/json.hpp>

#include "common.h"
#include "tts/voice_files.h"

// The Voice catalogue: Hugging Face's voices.json of rhasspy/piper-voices, an object of
//   "<voice name>": { "language": { "code": "en_US", "name_english": "English" }, "quality": "medium",
//                     "num_speakers": 1, "files": { "<path>": { "size_bytes": N, "md5_digest": "..." } } }
// The text comes from the network, so every field is checked and a bad entry is skipped, not trusted.
namespace {

// The path goes into a URL, so only plain path characters are allowed in it.
bool PlainPath(const std::string& path) {
    if (!IsSafeRelativePath(path)) return false;
    for (unsigned char c : path) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/')) return false;
    }
    return true;
}

// One file of a Voice: a plain path, a size above 0 and a 32-digit MD5, or it is refused.
bool ReadFileEntry(const std::string& path, const nlohmann::json& info, CatalogueFile* out) {
    if (!PlainPath(path) || !info.is_object()) return false;
    const auto size = info.find("size_bytes");
    const std::string md5 = ToLowerAscii(StringOf(info, "md5_digest"));
    if (size == info.end() || !size->is_number_unsigned() || size->get<uint64_t>() == 0 || md5.size() != 32) return false;
    out->path = path;
    out->sizeBytes = size->get<uint64_t>();
    out->md5 = md5;
    return true;
}

}  // namespace

// Every Voice of the catalogue that has a Language code and both of its files (.onnx and .onnx.json).
// Filtering to what Lexia accepts is a separate step (FilterCatalogueForLexia).
std::vector<CatalogueVoice> ParseVoiceCatalogue(const std::string& jsonText, std::string* error) {
    std::vector<CatalogueVoice> out;
    const nlohmann::json doc = nlohmann::json::parse(jsonText, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) {
        if (error) *error = "the Voice catalogue is not valid JSON";
        return out;
    }
    for (const auto& [key, v] : doc.items()) {
        if (!v.is_object()) continue;
        CatalogueVoice voice;
        voice.name = key;
        const auto lang = v.find("language");
        if (lang != v.end()) {
            voice.languageCode = StringOf(*lang, "code");
            voice.languageName = StringOf(*lang, "name_english");
        }
        voice.quality = ToLowerAscii(StringOf(v, "quality"));
        const auto speakers = v.find("num_speakers");
        if (speakers != v.end() && speakers->is_number_integer()) voice.numSpeakers = speakers->get<int>();
        const auto files = v.find("files");
        if (voice.languageCode.empty() || files == v.end() || !files->is_object()) continue;
        bool hasModel = false, hasConfig = false;
        for (const auto& [path, info] : files->items()) {
            // The size guards keep a bare ".onnx.json" from counting as either file.
            const bool config = path.size() > 10 && EndsWith(path, ".onnx.json");
            const bool model = !config && path.size() > 5 && EndsWith(path, ".onnx");
            if (model && !hasModel) hasModel = ReadFileEntry(path, info, &voice.model);
            else if (config && !hasConfig) hasConfig = ReadFileEntry(path, info, &voice.config);
        }
        if (hasModel && hasConfig) out.push_back(std::move(voice));
    }
    return out;
}

VoiceOutcome CheckCatalogueVoice(const CatalogueVoice& voice) {
    return CheckVoiceAccepted(voice.name, voice.languageCode, voice.languageName, voice.quality);
}

std::vector<CatalogueVoice> FilterCatalogueForLexia(const std::vector<CatalogueVoice>& voices) {
    std::vector<CatalogueVoice> out;
    for (const CatalogueVoice& v : voices) {
        if (CheckCatalogueVoice(v).ok()) out.push_back(v);
    }
    std::sort(out.begin(), out.end(), [](const CatalogueVoice& a, const CatalogueVoice& b) { return a.name < b.name; });
    return out;
}

#include "voice_names.h"

#include "strings/string_ids.h"
#include "strings/strings.h"
#include "tts/voice_library.h"

namespace {

// Dataset ids whose usual spelling is not "each word with a capital". They are proper names (of datasets
// and their speakers), the same in every Interface language, so they are data here, not UI words for
// strings/. "Jenny (Dioco)" is the form the Jenny dataset's licence asks for.
struct KnownName {
    const char* id;
    const char* shown;
};
constexpr KnownName kKnownNames[] = {
    {"ljspeech", "LJ Speech"},     {"jenny_dioco", "Jenny (Dioco)"}, {"libritts", "LibriTTS"},
    {"libritts_r", "LibriTTS-R"},  {"hfc_female", "HFC Female"},     {"hfc_male", "HFC Male"},
    {"l2arctic", "L2-ARCTIC"},     {"arctic", "ARCTIC"},             {"vctk", "VCTK"},
};

// "northern_english_male" -> "Northern English Male": each '_'-word with an ASCII capital. Other bytes
// (UTF-8 in a user's own name) are kept as they are.
std::string TitleWords(const std::string& id) {
    std::string out;
    bool start = true;
    for (char ch : id) {
        if (ch == '_') {
            out += ' ';
            start = true;
            continue;
        }
        out += (start && ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
        start = false;
    }
    return out;
}

}  // namespace

std::string VoiceShortName(const std::string& voiceName) {
    const size_t first = voiceName.find('-');
    const size_t last = voiceName.rfind('-');
    // Not "<locale>-<id>-<quality>": the user's own name is shown unchanged.
    if (first == std::string::npos || last == first || last == first + 1) return voiceName;
    const std::string id = voiceName.substr(first + 1, last - first - 1);
    for (const KnownName& k : kKnownNames) {
        if (id == k.id) return k.shown;
    }
    return TitleWords(id);
}

std::string VoiceQualityWord(const std::string& interfaceLanguage, const std::string& quality) {
    namespace sid = strings::id;
    const char* id = quality == "x_low"    ? sid::kWindowVoiceQualityXLow
                     : quality == "low"    ? sid::kWindowVoiceQualityLow
                     : quality == "medium" ? sid::kWindowVoiceQualityMedium
                     : quality == "high"   ? sid::kWindowVoiceQualityHigh
                                           : nullptr;
    return id ? strings::Text(interfaceLanguage, id) : std::string();
}

std::string VoiceLabel(const std::string& interfaceLanguage, const std::string& voiceName, const std::string& quality) {
    const std::string name = VoiceShortName(voiceName);
    const std::string word = VoiceQualityWord(interfaceLanguage, quality);
    if (word.empty()) return name;
    return strings::Format(interfaceLanguage, strings::id::kWindowVoiceLabel, {{"name", name}, {"quality", word}});
}

std::string VoiceLabelFromName(const std::string& interfaceLanguage, const std::string& voiceName) {
    return VoiceLabel(interfaceLanguage, voiceName, VoiceQualityFromName(voiceName));
}

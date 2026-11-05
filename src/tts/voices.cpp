#include "tts/voices.h"

#include <nlohmann/json.hpp>

// voices/voices.json: { "voices": [ { "language": "<code>", "name": "<Voice name>", ... } ] }.
// The other fields (source, path, ship, the SHA-256 hashes) are for fetch_deps.ps1 and the package
// step, not read here. One bad
// entry fails the whole list: the file is Lexia's own, so a mistake in it is a bug to show, not skip.
std::vector<VoiceEntry> ParseVoiceList(const std::string& jsonText, std::string* error) {
    std::vector<VoiceEntry> out;
    // Sets *error (when asked for) and returns the entries read so far.
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return out;
    };
    const nlohmann::json doc = nlohmann::json::parse(jsonText, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) return fail("voices.json is not valid JSON");
    const auto list = doc.find("voices");
    if (list == doc.end() || !list->is_array()) return fail("voices.json has no \"voices\" list");
    for (const auto& v : *list) {
        const auto lang = v.is_object() ? v.find("language") : v.end();
        const auto name = v.is_object() ? v.find("name") : v.end();
        if (v.is_object() && lang != v.end() && lang->is_string() && name != v.end() && name->is_string()) {
            out.push_back({lang->get<std::string>(), name->get<std::string>()});
        } else {
            return fail("a voices.json entry needs a \"language\" and a \"name\" string");
        }
    }
    return out;
}

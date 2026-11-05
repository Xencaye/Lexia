#include "strings/strings.h"

#include <algorithm>

#include "strings/string_ids.h"

// Lexia's own user-facing text (I1): look up a string id in the table of the Interface language,
// fall back to English, and fill {name} placeholders.
namespace strings {

// Every id of string_ids.h, in that order (the X-macro expands each line to its "id" text).
const std::vector<const char*>& AllIds() {
    static const std::vector<const char*> ids = {
#define LEXIA_STRING_ID_ENTRY(name, text) text,
        LEXIA_STRING_IDS(LEXIA_STRING_ID_ENTRY)
#undef LEXIA_STRING_ID_ENTRY
    };
    return ids;
}

namespace {

struct Registered {
    InterfaceLanguage info;
    const Table& (*table)();
};

// The list of Interface languages; the first is the default and must be English (every other
// table falls back to it).
const std::vector<Registered>& Registry() {
    static const std::vector<Registered> all = {
        {{"en", "English"}, &EnglishTable},
        {{"bg", u8"Български", /*offered=*/false}, &BulgarianTable},  // not yet: ADR-0007
    };
    return all;
}

}  // namespace

const std::vector<InterfaceLanguage>& InterfaceLanguages() {
    static const std::vector<InterfaceLanguage> all = [] {
        std::vector<InterfaceLanguage> v;
        for (const Registered& r : Registry()) v.push_back(r.info);
        return v;
    }();
    return all;
}

const std::string& DefaultInterfaceLanguage() { return InterfaceLanguages().front().code; }

bool IsInterfaceLanguage(const std::string& code) { return TableFor(code) != nullptr; }

bool IsOfferedInterfaceLanguage(const std::string& code) {
    for (const Registered& r : Registry()) {
        if (r.info.code == code) return r.info.offered;
    }
    return false;
}

const Table* TableFor(const std::string& code) {
    for (const Registered& r : Registry()) {
        if (r.info.code == code) return &r.table();
    }
    return nullptr;
}

const char* Find(const Table& table, const std::string& id) {
    for (const Entry& e : table) {
        if (id == e.id) return e.text;
    }
    return nullptr;
}

// The text of `id`: from the chosen table, else from English, else the id itself (a missing English
// text is a test failure, so the id should never reach the user).
std::string Resolve(const Table* chosen, const Table& english, const std::string& id) {
    if (chosen) {
        if (const char* text = Find(*chosen, id)) return text;
    }
    if (const char* text = Find(english, id)) return text;
    return id;
}

// Replaces each {name} that has an argument with its value. A {name} without an argument, or a lone
// "{", stays as it is. A value is not expanded again, so a "{" in it is safe.
std::string Expand(const std::string& text, const Args& args) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            const size_t close = text.find('}', i + 1);
            if (close != std::string::npos) {
                const std::string name = text.substr(i + 1, close - i - 1);
                const Args::const_iterator hit =
                    std::find_if(args.begin(), args.end(),
                                 [&](const auto& a) { return a.first == name; });
                if (hit != args.end()) {
                    out += hit->second;
                    i = close + 1;
                    continue;
                }
            }
        }
        out += text[i++];
    }
    return out;
}

std::string Text(const std::string& code, const std::string& id) {
    return Resolve(TableFor(code), EnglishTable(), id);
}

std::string Format(const std::string& code, const std::string& id, const Args& args) {
    return Expand(Text(code, id), args);
}

std::string LanguageName(const std::string& code, const std::string& languageId) {
    const std::string key = "language." + languageId;
    const std::string name = Text(code, key);
    return name == key ? languageId : name;  // no text for this Language: its id
}

}  // namespace strings

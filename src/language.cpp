#include "language.h"

#include "codepoint.h"

// The Language table: each Language's ids, names, script, OCR models and its own letters. The first
// is the default. Fields in order: id, code, name, ownName, script, recModel, dictFile, ttsLcid,
// ownLetters, offered (see language.h).
const std::vector<Language>& AllLanguages() {
    static const std::vector<Language> kLanguages = {
        {"eng", "en", "English", "English", Script::Latin, "rec_en.onnx", "dict_en.txt", 0, {}},
        // The Cyrillic recogniser reads Cyrillic + Latin, including the letters of other Cyrillic
        // languages (ы, э, ё, і, ў ...), which the Alphabet leaves out. Bulgarian's 30 letters are
        // U+0410-U+042F without Ы and Э (Ё is outside that block), in both cases, plus ѝ.
        {"bul", "bg", "Bulgarian", "Български", Script::Cyrillic, "rec_cyrillic.onnx", "dict_cyrillic.txt", 0x402,
         {{0x040D, 0x040D}, {0x0410, 0x042A}, {0x042C, 0x042C}, {0x042E, 0x044A}, {0x044C, 0x044C},
          {0x044E, 0x044F}, {0x045D, 0x045D}},
         /*offered=*/false},  // not yet: ADR-0007
    };
    return kLanguages;
}

const Language& DefaultLanguage() { return AllLanguages().front(); }

// By the three-letter id of settings.json ("eng"); nullptr if there is none.
const Language* FindLanguage(const std::string& id) {
    for (const Language& l : AllLanguages()) {
        if (l.id == id) return &l;
    }
    return nullptr;
}

// By the two-letter ISO 639-1 code ("en"), as lexia_cli --lang and the Voices use; nullptr if none.
const Language* FindLanguageByCode(const std::string& code) {
    for (const Language& l : AllLanguages()) {
        if (l.code == code) return &l;
    }
    return nullptr;
}

// A rough guess from the letters: more Cyrillic than Latin letters gives the Cyrillic Language, else
// the default.
const Language& GuessLanguage(const std::string& utf8) {
    size_t cyr = 0, lat = 0;
    for (size_t i = 0; i < utf8.size();) {
        const char32_t c = NextCodePoint(utf8, i);
        if (c >= 0x0400 && c <= 0x04FF) ++cyr;
        else if ((c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z')) ++lat;
    }
    if (cyr > lat) {
        for (const Language& l : AllLanguages()) {
            if (l.script == Script::Cyrillic) return l;
        }
    }
    return DefaultLanguage();
}

// The Alphabet of a Language (E3): its own letters, plus Latin, digits, punctuation and currency
// signs, which appear in text of any Language.
bool InAlphabet(const Language& lang, char32_t cp) {
    for (const CodePointRange& r : lang.ownLetters) {
        if (cp >= r.first && cp <= r.last) return true;
    }
    if (cp >= 0x20 && cp <= 0x7E) return true;  // printable ASCII: Latin, digits, punctuation
    if (cp >= 0xA0 && cp <= 0xBF) return true;  // Latin-1 punctuation and symbols
    if (IsLatinLetter(cp)) return true;  // Latin letters with accents, Latin Extended Additional
    if (cp >= 0x2000 && cp <= 0x206F) return true;  // general punctuation (dashes, quotes)
    if (cp >= 0x20A0 && cp <= 0x20CF) return true;  // currency signs
    return false;
}

// A dictionary entry (one or more code points) is in the Alphabet when every code point of it is.
bool InAlphabet(const Language& lang, const std::string& utf8) {
    if (utf8.empty()) return false;
    for (size_t i = 0; i < utf8.size();) {
        if (!InAlphabet(lang, NextCodePoint(utf8, i))) return false;
    }
    return true;
}

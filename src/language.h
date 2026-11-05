#pragma once
// Everything that differs between languages lives here, so adding a language means
// adding one entry to the table in language.cpp (plus its OCR model files) instead of
// touching the rest of the code. Every entry needs both a three-letter `id` (settings.json)
// and its own two-letter ISO 639-1 `code` (lexia_cli's --lang), unique across the table.
#include <string>
#include <vector>

enum class Script { Latin, Cyrillic };

struct CodePointRange {
    char32_t first, last;  // inclusive
};

struct Language {
    std::string id;        // short code used on the console / in settings ("eng", "bul")
    std::string code;      // two-letter ISO 639-1 code, for lexia_cli's --lang ("en", "bg")
    std::string name;      // display name, in English (log lines, and the fallback in text)
    std::string ownName;   // its name in itself ("Български"); the Language-switch Announcement says it
    Script script;         // main writing system
    std::string recModel;  // text recogniser file in the models folder
    std::string dictFile;  // matching character dictionary in the models folder
    int ttsLcid;           // Windows locale id of the voice to look for; 0 = system default voice
    // The Alphabet (docs/GLOSSARY.md): these letters, plus the Latin letters, digits and punctuation
    // every Language allows. A letter of another script, or of another Language's own alphabet
    // (Russian ы in Bulgarian), is outside it. English has no letters of its own beyond Latin.
    std::vector<CodePointRange> ownLetters;
    // Lexia offers it to the user: the app reads it, switches to it and announces it
    // (docs/adr/0007-english-only-for-now.md). A Language not offered stays in the table, with
    // its tests and lexia_cli's --lang, so offering it again is this one flag (and its models in
    // the package, cmake/package.cmake).
    bool offered = true;
};

const std::vector<Language>& AllLanguages();  // the first entry is the default
const Language& DefaultLanguage();
const Language* FindLanguage(const std::string& id);  // nullptr if unknown
const Language* FindLanguageByCode(const std::string& code);  // by `code`; nullptr if unknown

// Is this code point in the Language's Alphabet? Decides by code point, no tables beyond the
// Language entry: own letters, printable ASCII, Latin letters (incl. accented), Latin-1 and general
// punctuation, currency signs. Anything else (Greek, CJK, U+FFFD, other Cyrillic letters) is out.
bool InAlphabet(const Language& lang, char32_t cp);
// The same for UTF-8 text: true when it is not empty and every code point is in the Alphabet.
bool InAlphabet(const Language& lang, const std::string& utf8);

// Which language does this UTF-8 text look like? (By script only, for now.)
const Language& GuessLanguage(const std::string& utf8);

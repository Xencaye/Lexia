#pragma once
// Lexia's own user-facing text, in every Interface language (see GLOSSARY: the Interface
// language is chosen separately from the Language). Win32-free.
//
// The tables are compiled in, one file per Interface language (table_en.cpp, table_bg.cpp):
// nothing can be missing from the install, a text can't be read half-written, and the tests see
// exactly what ships. Adding an Interface language = one new table_xx.cpp, one line in the list
// in strings.cpp and one in CMakeLists.txt.
//
// A text is asked for by id (string_ids.h). Missing in the chosen language: the English text.
// Missing in English: the id itself, so the gap is visible, and the tests fail. Texts are UTF-8.
// Log lines and lexia_cli's output are developer text and do not come through here.
//
//   strings::Text(code, strings::id::kTrayQuit)
//   strings::Format(code, strings::id::kControlBarSkipTip, {{"key", "F6"}})   // "Skip (F6)"
#include <string>
#include <utility>
#include <vector>

namespace strings {

struct Entry {
    const char* id;
    const char* text;
};
using Table = std::vector<Entry>;

// Placeholder values for Format: {name} in the text becomes the value.
using Args = std::vector<std::pair<std::string, std::string>>;

struct InterfaceLanguage {
    std::string code;     // two-letter ISO 639-1, as in settings.json's "interfaceLanguage" ("en")
    std::string ownName;  // its name in itself, as a picker shows it ("Български")
    // Lexia offers it to the user (ADR-0007). One not offered keeps its table and its tests.
    bool offered = true;
};

// All of them, offered or not; the first is the default (English).
const std::vector<InterfaceLanguage>& InterfaceLanguages();
const std::string& DefaultInterfaceLanguage();
bool IsInterfaceLanguage(const std::string& code);
// Is `code` an Interface language that Lexia offers? (A picker lists only these.)
bool IsOfferedInterfaceLanguage(const std::string& code);

// The table of an Interface language; nullptr if `code` is not one.
const Table* TableFor(const std::string& code);

// The text of `id` in a table; nullptr if it has none.
const char* Find(const Table& table, const std::string& id);

// The fallback rule on given tables: `chosen` (may be null), else `english`, else the id.
std::string Resolve(const Table* chosen, const Table& english, const std::string& id);

// Replaces each {name} with its value in `args`. A {name} with no value, and a brace that is not
// part of a {name}, stay as written. A value is not searched for placeholders itself.
std::string Expand(const std::string& text, const Args& args);

// The text of `id` in the Interface language `code` (an unknown code gives English).
std::string Text(const std::string& code, const std::string& id);

// Text + Expand.
std::string Format(const std::string& code, const std::string& id, const Args& args);

// The name of the Language with this id ("bul") for use in a sentence, in the Interface language.
// An unknown Language gives its id.
std::string LanguageName(const std::string& code, const std::string& languageId);

// The tables themselves, one per file.
const Table& EnglishTable();
const Table& BulgarianTable();

}  // namespace strings

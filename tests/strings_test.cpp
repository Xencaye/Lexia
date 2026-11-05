// Unit tests for strings/strings.h: the Interface language's string table. The tables are
// checked against the list of ids in strings/string_ids.h, so a typo or a forgotten English text
// fails here instead of showing an id on screen.
#include <doctest/doctest.h>

#include <set>
#include <string>
#include <vector>

#include "language.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

namespace {

// The {names} a text mentions.
std::set<std::string> Placeholders(const std::string& text) {
    std::set<std::string> names;
    size_t open = text.find('{');
    while (open != std::string::npos) {
        const size_t close = text.find('}', open);
        if (close == std::string::npos) break;
        names.insert(text.substr(open + 1, close - open - 1));
        open = text.find('{', close);
    }
    return names;
}

std::set<std::string> DeclaredIds() {
    const std::vector<const char*>& all = strings::AllIds();
    return std::set<std::string>(all.begin(), all.end());
}

}  // namespace

TEST_CASE("every declared id has an English text") {
    for (const char* id : strings::AllIds()) {
        const char* text = strings::Find(*strings::TableFor("en"), id);
        INFO("id: ", id);
        REQUIRE(text != nullptr);
        CHECK(std::string(text) != "");
    }
}

TEST_CASE("ids are unique, and no table has an id that is not declared") {
    const std::vector<const char*>& all = strings::AllIds();
    CHECK(DeclaredIds().size() == all.size());
    const std::set<std::string> declared = DeclaredIds();
    for (const strings::InterfaceLanguage& lang : strings::InterfaceLanguages()) {
        const strings::Table* table = strings::TableFor(lang.code);
        REQUIRE(table != nullptr);
        std::set<std::string> seen;
        for (const strings::Entry& e : *table) {
            INFO(lang.code, ": ", e.id);
            CHECK(declared.count(e.id) == 1);
            CHECK(seen.insert(e.id).second);
        }
    }
}

TEST_CASE("a translation uses the same {placeholders} as the English text") {
    const strings::Table& en = *strings::TableFor("en");
    for (const strings::InterfaceLanguage& lang : strings::InterfaceLanguages()) {
        for (const strings::Entry& e : *strings::TableFor(lang.code)) {
            const char* english = strings::Find(en, e.id);
            REQUIRE(english != nullptr);
            INFO(lang.code, ": ", e.id);
            CHECK(Placeholders(e.text) == Placeholders(english));
        }
    }
}

TEST_CASE("every Language has a name in English") {
    for (const Language& l : AllLanguages()) {
        CHECK(strings::Find(*strings::TableFor("en"), ("language." + l.id).c_str()) != nullptr);
    }
}

TEST_CASE("an Interface language that misses ids is reported, not failed") {
    // A missing translation falls back to English, so it is allowed; this lists what is left.
    for (const strings::InterfaceLanguage& lang : strings::InterfaceLanguages()) {
        for (const char* id : strings::AllIds()) {
            const bool has = strings::Find(*strings::TableFor(lang.code), id) != nullptr;
            WARN_MESSAGE(has, lang.code, " has no text for ", id, " (English is used)");
        }
    }
}

TEST_CASE("the Interface languages are English (the default) and Bulgarian") {
    const std::vector<strings::InterfaceLanguage>& all = strings::InterfaceLanguages();
    REQUIRE(all.size() >= 2);
    CHECK(all.front().code == "en");
    CHECK(strings::DefaultInterfaceLanguage() == "en");
    CHECK(strings::IsInterfaceLanguage("en"));
    CHECK(strings::IsInterfaceLanguage("bg"));
    CHECK_FALSE(strings::IsInterfaceLanguage("xx"));
    CHECK_FALSE(strings::IsInterfaceLanguage(""));
    CHECK_FALSE(strings::IsInterfaceLanguage("EN"));
    // Only English is offered for now (ADR-0007); Bulgarian keeps its table.
    CHECK(strings::IsOfferedInterfaceLanguage("en"));
    CHECK_FALSE(strings::IsOfferedInterfaceLanguage("bg"));
    CHECK_FALSE(strings::IsOfferedInterfaceLanguage("xx"));
    // Each one is shown by its own name.
    CHECK(all[0].ownName == "English");
    CHECK(all[1].ownName == u8"Български");
}

TEST_CASE("Text: the chosen language's text, else English, else the id") {
    CHECK(strings::Text("en", strings::id::kTrayQuit) == "Quit");
    CHECK(strings::Text("bg", strings::id::kTrayQuit) != "Quit");
    CHECK(strings::Text("bg", strings::id::kTrayQuit) != "");
    CHECK(strings::Text("xx", strings::id::kTrayQuit) == "Quit");  // not an Interface language
    CHECK(strings::Text("en", "no.such.id") == "no.such.id");       // visible, never empty
}

TEST_CASE("Resolve: a missing key falls back to English") {
    const strings::Table english = {{"a", "Alpha"}, {"b", "Bravo"}};
    const strings::Table partial = {{"a", "Alfa"}};
    CHECK(strings::Resolve(&partial, english, "a") == "Alfa");
    CHECK(strings::Resolve(&partial, english, "b") == "Bravo");  // not translated
    CHECK(strings::Resolve(nullptr, english, "a") == "Alpha");   // no such language
    CHECK(strings::Resolve(&partial, english, "c") == "c");      // not even in English
}

TEST_CASE("Expand fills {name} placeholders") {
    CHECK(strings::Expand("Skip ({key})", {{"key", "F6"}}) == "Skip (F6)");
    CHECK(strings::Expand("{a} and {b}, {a}", {{"a", "1"}, {"b", "2"}}) == "1 and 2, 1");
    CHECK(strings::Expand("no placeholders", {{"key", "F6"}}) == "no placeholders");
    CHECK(strings::Expand("", {{"key", "F6"}}) == "");
}

TEST_CASE("Expand leaves what it cannot fill as written") {
    CHECK(strings::Expand("The {language} voice", {}) == "The {language} voice");
    CHECK(strings::Expand("open { brace", {{"x", "y"}}) == "open { brace");
    CHECK(strings::Expand("a } b", {{"x", "y"}}) == "a } b");
    // A value is never read for placeholders again.
    CHECK(strings::Expand("{a}", {{"a", "{b}"}, {"b", "no"}}) == "{b}");
}

TEST_CASE("Format: Text plus Expand, in the chosen language") {
    CHECK(strings::Format("en", strings::id::kControlBarSkipTip, {{"key", "F6"}}) == "Skip (F6)");
    const std::string bg = strings::Format("bg", strings::id::kControlBarSkipTip, {{"key", "F6"}});
    CHECK(bg.find("F6") != std::string::npos);
    CHECK(bg.find("{key}") == std::string::npos);
    CHECK(strings::Format("en", strings::id::kAnnounceVoiceMissingFor, {{"language", "Bulgarian"}}) ==
          "The Bulgarian voice is missing.");
}

TEST_CASE("LanguageName: a Language's name in the Interface language") {
    CHECK(strings::LanguageName("en", "bul") == "Bulgarian");
    CHECK(strings::LanguageName("bg", "bul") != "Bulgarian");
    CHECK(strings::LanguageName("bg", "bul") != "");
    CHECK(strings::LanguageName("en", "zzz") == "zzz");  // unknown Language: its id, never empty
}

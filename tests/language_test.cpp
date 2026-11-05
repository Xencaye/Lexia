// Unit tests for language.h: the Language table and GuessLanguage (UTF-8 in, by script only).
// Cyrillic strings are written as UTF-8 byte escapes so the test does not depend on how the
// editor saved the file.
#include <doctest/doctest.h>

#include <set>
#include <string>

#include "language.h"

TEST_CASE("GuessLanguage: Latin text -> English") {
    CHECK(GuessLanguage("Hello, world").id == "eng");
}

TEST_CASE("GuessLanguage: Cyrillic text -> Bulgarian") {
    // "Zdraveyte" in Cyrillic
    CHECK(GuessLanguage("\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB5\xD0\xB9\xD1\x82\xD0\xB5").id ==
          "bul");
}

TEST_CASE("GuessLanguage: mixed text follows the majority of letters") {
    // 4 Cyrillic letters (D0 97 D0 B4 D1 80 D0 B0) + 2 Latin letters -> Cyrillic wins.
    CHECK(GuessLanguage("\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0 ab").id == "bul");
    // 1 Cyrillic letter + 3 Latin letters -> Latin wins.
    CHECK(GuessLanguage("\xD0\x97 abc").id == "eng");
}

TEST_CASE("GuessLanguage: a tie goes to the default Language") {
    CHECK(GuessLanguage("\xD0\x97 a").id == DefaultLanguage().id);
}

TEST_CASE("GuessLanguage: empty input and text without letters -> default Language") {
    CHECK(GuessLanguage("").id == DefaultLanguage().id);
    CHECK(GuessLanguage("1234 !?.").id == DefaultLanguage().id);
}

TEST_CASE("GuessLanguage: digits and punctuation do not outvote letters") {
    CHECK(GuessLanguage("2024 \xD0\x97 !!! ???").id == "bul");
}

TEST_CASE("GuessLanguage: malformed UTF-8 does not crash and counts as neither script") {
    CHECK(GuessLanguage("\xD0").id == DefaultLanguage().id);          // cut-off sequence
    CHECK(GuessLanguage("\xFF\xFE ab").id == "eng");                  // stray bytes
    CHECK(GuessLanguage("\xD0\x97\xFF").id == "bul");                 // bad byte after a letter
}

TEST_CASE("FindLanguage: known and unknown ids") {
    REQUIRE(FindLanguage("bul") != nullptr);
    CHECK(FindLanguage("bul")->script == Script::Cyrillic);
    CHECK(FindLanguage("xx") == nullptr);
    CHECK(FindLanguage("") == nullptr);
}

TEST_CASE("FindLanguageByCode: the two-letter codes lexia_cli takes") {
    REQUIRE(FindLanguageByCode("en") != nullptr);
    CHECK(FindLanguageByCode("en")->id == "eng");
    REQUIRE(FindLanguageByCode("bg") != nullptr);
    CHECK(FindLanguageByCode("bg")->id == "bul");
    CHECK(FindLanguageByCode("eng") == nullptr);  // an id is not a code
    CHECK(FindLanguageByCode("") == nullptr);
}

TEST_CASE("AllLanguages: every Language has a three-letter id and a unique two-letter code") {
    std::set<std::string> ids, codes;
    for (const Language& l : AllLanguages()) {
        CAPTURE(l.name);
        CHECK(l.id.size() == 3);
        CHECK(l.code.size() == 2);
        CHECK(ids.insert(l.id).second);
        CHECK(codes.insert(l.code).second);
    }
}

TEST_CASE("offered: English (the default) is offered, Bulgarian stays in the table but is not, for now") {
    CHECK(DefaultLanguage().offered);  // the fallback for a Language not offered must be one
    REQUIRE(FindLanguage("bul") != nullptr);
    CHECK_FALSE(FindLanguage("bul")->offered);
}

// ---- Alphabet (E3): which code points a Language's recogniser may produce ----

TEST_CASE("InAlphabet: English allows Latin letters, digits and punctuation, not Cyrillic") {
    const Language& en = *FindLanguage("eng");
    for (char32_t c : std::u32string(U"aZ09 .,;:!?'\"()-/@#%&*+=<>[]{}_~`^|$")) CHECK(InAlphabet(en, c));
    CHECK(InAlphabet(en, U'é'));  // é: a Latin letter
    CHECK(InAlphabet(en, U'’'));  // right single quote: punctuation
    CHECK(InAlphabet(en, U'—'));  // em dash
    CHECK_FALSE(InAlphabet(en, U'Д'));  // Д
    CHECK_FALSE(InAlphabet(en, U'я'));  // я
    CHECK_FALSE(InAlphabet(en, U'ж'));  // ж
}

TEST_CASE("InAlphabet: Bulgarian allows its 30 letters in both cases") {
    const Language& bg = *FindLanguage("bul");
    int upper = 0, lower = 0;
    for (char32_t c = 0x0410; c <= 0x042F; ++c) upper += InAlphabet(bg, c);
    for (char32_t c = 0x0430; c <= 0x044F; ++c) lower += InAlphabet(bg, c);
    CHECK(upper == 30);  // 32 capitals in the block minus Ы Э
    CHECK(lower == 30);
    CHECK(InAlphabet(bg, U'Ъ'));  // Ъ
    CHECK(InAlphabet(bg, U'ь'));  // ь
    CHECK(InAlphabet(bg, U'Й'));  // Й
    CHECK(InAlphabet(bg, U'ю'));  // ю
    CHECK(InAlphabet(bg, U'ѝ'));  // ѝ, the stressed и of Bulgarian spelling
}

TEST_CASE("InAlphabet: Bulgarian rejects Cyrillic letters of other languages") {
    const Language& bg = *FindLanguage("bul");
    // ы э ё ђ і ў ї є ґ ј љ њ ћ џ ѓ ќ ѕ and the capitals Ы Э Ё
    for (char32_t c : std::u32string(U"ыэёђіўїєґјљњ"
                                     U"ћџѓќѕЫЭЁІЎЄҐ"))
        CHECK_FALSE(InAlphabet(bg, c));
}

TEST_CASE("InAlphabet: Bulgarian keeps Latin, digits and punctuation") {
    const Language& bg = *FindLanguage("bul");
    for (char32_t c : std::u32string(U"aZ09 .,;:!?'\"()-/@#%&*+=Éé–„“")) CHECK(InAlphabet(bg, c));
}

TEST_CASE("InAlphabet: letters of unrelated scripts are out for every Language") {
    for (const Language& l : AllLanguages()) {
        CAPTURE(l.name);
        CHECK_FALSE(InAlphabet(l, U'α'));  // Greek alpha
        CHECK_FALSE(InAlphabet(l, U'中'));  // CJK
        CHECK_FALSE(InAlphabet(l, U'�'));  // replacement character
    }
}

TEST_CASE("InAlphabet (UTF-8): every code point of the text must be in the Alphabet") {
    const Language& bg = *FindLanguage("bul");
    CHECK(InAlphabet(bg, "\xD0\x97\xD0\xB4\xD1\x80"));   // Здр
    CHECK_FALSE(InAlphabet(bg, "\xD1\x8B"));             // ы
    CHECK_FALSE(InAlphabet(bg, "a\xD1\x8B"));            // a then ы
    CHECK_FALSE(InAlphabet(bg, "\xD0"));                 // malformed
    CHECK_FALSE(InAlphabet(bg, ""));                     // nothing is not a character
}

TEST_CASE("Language::ownName: each Language's name in itself, for the Language-switch Announcement") {
    REQUIRE(FindLanguageByCode("en") != nullptr);
    REQUIRE(FindLanguageByCode("bg") != nullptr);
    CHECK(FindLanguageByCode("en")->ownName == "English");
    // "Bulgarski" in Cyrillic, as UTF-8 byte escapes
    CHECK(FindLanguageByCode("bg")->ownName == "\xD0\x91\xD1\x8A\xD0\xBB\xD0\xB3\xD0\xB0\xD1\x80\xD1\x81\xD0\xBA\xD0\xB8");
    for (const Language& l : AllLanguages()) {
        CAPTURE(l.name);
        CHECK_FALSE(l.ownName.empty());
        // Spoken by that Language's Voice, so it must be in its script.
        CHECK(GuessLanguage(l.ownName).id == l.id);
    }
}

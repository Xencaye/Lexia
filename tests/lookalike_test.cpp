// Unit tests for lookalike.h: the Lookalike fix on hand-made words. Cyrillic is written as UTF-8
// byte escapes (see the constants) so the test does not depend on how the editor saved the file;
// a hex escape swallows the hex digits after it, so every escape sits in its own constant.
#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "lookalike.h"

namespace {
const std::string kCyrA = "\xD0\x90";    // А  (lookalike of Latin A)
const std::string kCyrT = "\xD0\xA2";    // Т  (lookalike of Latin T)
const std::string kCyrE = "\xD0\xB5";    // е  (lookalike of Latin e)
const std::string kCyrO = "\xD0\xBE";    // о  (lookalike of Latin o)
const std::string kCyrD = "\xD0\xB4";    // д  (Cyrillic only)
const std::string kCyrM = "\xD0\xBC";    // м  (Cyrillic only)
const std::string kCyrB = "\xD0\x91";    // Б  (Cyrillic only)
const std::string kCyrDe = "\xD0\x94";   // Д  (Cyrillic only)
const std::string kCyrDzh = "\xD0\xB6";  // ж  (Cyrillic only)

const Script kLat = Script::Latin;
const Script kCyr = Script::Cyrillic;
}  // namespace

TEST_CASE("FixLookalikes: Latin lookalikes in a Cyrillic word become Cyrillic") {
    // д + Latin o + м -> дом
    CHECK(FixLookalikes(kCyrD + "o" + kCyrM, kLat) == kCyrD + kCyrO + kCyrM);
}

TEST_CASE("FixLookalikes: Cyrillic lookalikes in a Latin word become Latin") {
    // h + Cyrillic е + l + l + Cyrillic о -> hello
    CHECK(FixLookalikes("h" + kCyrE + "ll" + kCyrO, kCyr) == "hello");
}

TEST_CASE("FixLookalikes: both cases") {
    CHECK(FixLookalikes(kCyrB + "A" + kCyrDe, kLat) == kCyrB + kCyrA + kCyrDe);
    CHECK(FixLookalikes(kCyrT + "he", kCyr) == "The");
}

TEST_CASE("FixLookalikes: every pair of the table, both ways") {
    // The pairs are written out here on purpose: they are the spec, not a copy of the table.
    const std::vector<std::pair<std::string, std::string>> pairs = {
        {"A", "\xD0\x90"}, {"B", "\xD0\x92"}, {"C", "\xD0\xA1"}, {"E", "\xD0\x95"},
        {"H", "\xD0\x9D"}, {"K", "\xD0\x9A"}, {"M", "\xD0\x9C"}, {"O", "\xD0\x9E"},
        {"P", "\xD0\xA0"}, {"T", "\xD0\xA2"}, {"X", "\xD0\xA5"}, {"a", "\xD0\xB0"},
        {"c", "\xD1\x81"}, {"e", "\xD0\xB5"}, {"o", "\xD0\xBE"}, {"p", "\xD1\x80"},
        {"x", "\xD1\x85"}, {"y", "\xD1\x83"},
    };
    for (const auto& [lat, cyr] : pairs) {
        CAPTURE(lat);
        // The Latin form next to a Cyrillic-only letter becomes the Cyrillic form ...
        CHECK(FixLookalikes(kCyrD + lat, kLat) == kCyrD + cyr);
        // ... and the Cyrillic form next to a Latin-only letter becomes the Latin form.
        CHECK(FixLookalikes("d" + cyr, kCyr) == "d" + lat);
    }
}

TEST_CASE("FixLookalikes: lookalike letters do not vote") {
    // Three Cyrillic lookalikes and one Latin-only letter: the Latin-only letter decides.
    CHECK(FixLookalikes(kCyrO + kCyrO + kCyrO + "d", kCyr) == "oood");
}

TEST_CASE("FixLookalikes: letters that are not lookalikes are never converted") {
    // Latin b is not a lookalike of anything; the word is Cyrillic, only the o changes.
    CHECK(FixLookalikes(kCyrD + "o" + kCyrM + "b", kLat) == kCyrD + kCyrO + kCyrM + "b");
    // Cyrillic д in a Latin word stays.
    CHECK(FixLookalikes("h" + kCyrE + "llo" + kCyrD, kCyr) == "hello" + kCyrD);
}

TEST_CASE("FixLookalikes: a word of one script is untouched") {
    CHECK(FixLookalikes("hello", kCyr) == "hello");
    CHECK(FixLookalikes(kCyrD + kCyrO + kCyrM, kLat) == kCyrD + kCyrO + kCyrM);
    // Only lookalike letters, all of one script: not mixed, so not converted even against the tie-break.
    CHECK(FixLookalikes("COM", kCyr) == "COM");
    CHECK(FixLookalikes(kCyrO + kCyrO, kLat) == kCyrO + kCyrO);
}

TEST_CASE("FixLookalikes: a tie goes to the tie-break script") {
    // д and d are one vote each; the o is a Cyrillic lookalike.
    const std::string word = kCyrD + "d" + kCyrO;
    CHECK(FixLookalikes(word, kCyr) == word);
    CHECK(FixLookalikes(word, kLat) == kCyrD + "do");
}

TEST_CASE("FixLookalikes: a mixed word of lookalikes only goes to the tie-break script") {
    const std::string word = "o" + kCyrO;
    CHECK(FixLookalikes(word, kCyr) == kCyrO + kCyrO);
    CHECK(FixLookalikes(word, kLat) == "oo");
}

TEST_CASE("FixLookalikes: each word is decided on its own") {
    const std::string in = kCyrD + "o" + kCyrM + " h" + kCyrE + "llo";
    const std::string out = kCyrD + kCyrO + kCyrM + " hello";
    CHECK(FixLookalikes(in, kLat) == out);
}

TEST_CASE("FixLookalikes: whitespace and punctuation end a word") {
    // The Latin o after the comma is its own word, with no Cyrillic in it: untouched.
    CHECK(FixLookalikes(kCyrD + kCyrO + kCyrM + ",o", kCyr) == kCyrD + kCyrO + kCyrM + ",o");
    CHECK(FixLookalikes("(" + kCyrD + "o" + kCyrM + ")-\"h" + kCyrE + "llo\"\n", kLat) ==
          "(" + kCyrD + kCyrO + kCyrM + ")-\"hello\"\n");
}

TEST_CASE("FixLookalikes: digits stay and do not vote") {
    CHECK(FixLookalikes(kCyrD + "o2", kLat) == kCyrD + kCyrO + "2");
    CHECK(FixLookalikes("42 3D", kCyr) == "42 3D");
    // Digits do not end a word: the Cyrillic е after the 1 belongs to the Latin word.
    CHECK(FixLookalikes("ab1" + kCyrE, kCyr) == "ab1e");
}

TEST_CASE("FixLookalikes: accented Latin letters count as Latin") {
    // é (C3 A9) is Latin only, so with the Cyrillic е it makes a Latin word.
    CHECK(FixLookalikes("\xC3\xA9" "t" + kCyrE, kCyr) == "\xC3\xA9" "te");
}

TEST_CASE("FixLookalikes: empty text and malformed bytes") {
    CHECK(FixLookalikes("", kLat).empty());
    CHECK(FixLookalikes("ab\xFF" "c", kLat) == "ab\xFF" "c");
    // A cut-off two-byte letter at the end is kept as it is.
    CHECK(FixLookalikes(kCyrD + "o\xD0", kLat) == kCyrD + kCyrO + "\xD0");
    // C1 81 is an overlong 'A', not UTF-8: it is not turned into the Cyrillic lookalike А.
    CHECK(FixLookalikes(kCyrD + "\xC1\x81" + kCyrM, kLat) == kCyrD + "\xC1\x81" + kCyrM);
}

TEST_CASE("FixLookalikes: Cyrillic signs and combining marks (U+0482..U+0489) are not letters") {
    // Latin o, U+0482 (Cyrillic thousands sign), д: the sign ends the word, so the o stands alone
    // and is left as it is (it would become Cyrillic if the sign counted as a Cyrillic letter).
    CHECK(FixLookalikes("o\xD2\x82" + kCyrD, kCyr) == "o\xD2\x82" + kCyrD);
}

// Unit tests for stretch.h: cutting a Block's text into stretches of one script, one Voice each.
// Cyrillic is written as UTF-8 escapes so the test does not depend on how the editor saved it.
#include <doctest/doctest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "stretch.h"

namespace {

const char* const kThisIs = "\xD0\xA2\xD0\xBE\xD0\xB2\xD0\xB0 \xD0\xB5";                      // "Това е"
const char* const kGood = "\xD0\xB4\xD0\xBE\xD0\xB1\xD1\x8A\xD1\x80";                        // "добър"
const char* const kProduct = "\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB4\xD1\x83\xD0\xBA\xD1\x82";    // "Продукт"
const char* const kWorks = "\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xB8";               // "работи"
const char* const kAnd = "\xD0\xB8";                                                          // "и"
const char* const kToday = "\xD0\xB4\xD0\xBD\xD0\xB5\xD1\x81";                                // "днес"

std::string Join(const std::vector<Stretch>& s) {
    std::string all;
    for (const Stretch& x : s) all += x.text;
    return all;
}

std::string Cat(std::initializer_list<std::string> parts) {
    std::string all;
    for (const std::string& p : parts) all += p;
    return all;
}

}  // namespace

TEST_CASE("Stretches: empty text gives none") {
    CHECK(SplitIntoStretches("").empty());
}

TEST_CASE("Stretches: text of one script is one stretch") {
    auto s = SplitIntoStretches("Hello there, world.");
    REQUIRE(s.size() == 1);
    CHECK(s[0].text == "Hello there, world.");
    CHECK(s[0].language->id == "eng");

    const std::string bg = Cat({kProduct, " ", kWorks, " ", kGood});
    s = SplitIntoStretches(bg);
    REQUIRE(s.size() == 1);
    CHECK(s[0].text == bg);
    CHECK(s[0].language->id == "bul");
}

TEST_CASE("Stretches: text with no letters is one stretch in the default Language") {
    auto s = SplitIntoStretches("2024 - 15.5 !");
    REQUIRE(s.size() == 1);
    CHECK(s[0].text == "2024 - 15.5 !");
    CHECK(s[0].language->id == DefaultLanguage().id);
}

TEST_CASE("Stretches: a phrase of two or more words in another script is a stretch of its own") {
    const std::string bg1 = Cat({kProduct, " ", kWorks, " ", kGood, ", "});
    const std::string en = "Hello world. ";
    const std::string bg2 = Cat({kThisIs, " ", kGood});
    const auto s = SplitIntoStretches(bg1 + en + bg2);
    REQUIRE(s.size() == 3);
    CHECK(s[0].language->id == "bul");
    CHECK(s[0].text == bg1);
    CHECK(s[1].language->id == "eng");
    CHECK(s[1].text == en);
    CHECK(s[2].language->id == "bul");
    CHECK(s[2].text == bg2);
}

TEST_CASE("Stretches: one word of another script stays in the surrounding stretch") {
    const std::string text = Cat({kProduct, " Windows ", kWorks, " ", kGood});
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
    CHECK(s[0].text == text);
}

TEST_CASE("Stretches: an island at the end stays in the stretch before it") {
    const std::string text = Cat({kProduct, " ", kWorks, " ", kGood, " Linux"});
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
}

TEST_CASE("Stretches: an island at the start joins the stretch after it") {
    const std::string text = Cat({"Windows ", kProduct, " ", kWorks});
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
    CHECK(s[0].text == text);
}

TEST_CASE("Stretches: two one-word islands with a word of the surrounding script between stay absorbed") {
    // Bulgarian words, Windows, "and", Linux, Bulgarian words
    const std::string text = Cat({kProduct, " ", kWorks, " ", kGood, " Windows ", kAnd, " Linux ", kThisIs});
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
    CHECK(s[0].text == text);
}

TEST_CASE("Stretches: a one-word island after a real stretch stays in it") {
    // Bulgarian x3, English x2, then one Bulgarian word: it is an island and stays in the English
    // stretch before it.
    const std::string bg = Cat({kProduct, " ", kWorks, " ", kGood, " "});
    const auto s = SplitIntoStretches(bg + "Hello world " + kToday);
    REQUIRE(s.size() == 2);
    CHECK(s[0].text == bg);
    CHECK(s[1].language->id == "eng");
    CHECK(s[1].text == Cat({"Hello world ", kToday}));
}

TEST_CASE("Stretches: words with no letters join the surrounding stretch and are not counted") {
    // "42" and "-" are not words, so "Hello 42 -" is still one word: an island.
    const std::string a = Cat({kProduct, " ", kWorks, " 42 - Hello ", kGood, " ", kThisIs});
    auto s = SplitIntoStretches(a);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");

    // Between two stretches they go with the earlier one, inside a stretch they stay in it.
    const std::string bg = Cat({kProduct, " ", kWorks, " 2024 - "});
    s = SplitIntoStretches(bg + "Hello 7 world 8");
    REQUIRE(s.size() == 2);
    CHECK(s[0].text == bg);
    CHECK(s[1].text == "Hello 7 world 8");
    CHECK(s[1].language->id == "eng");
}

TEST_CASE("Stretches: a word in a script no Language uses (Greek) joins the surrounding stretch") {
    const std::string text = "Hello world \xCE\xB1\xCE\xB2\xCE\xB3 again";
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].text == text);
}

TEST_CASE("Stretches: a word's script is the one most of its letters belong to") {
    // "ab" + 3 Cyrillic letters is a Cyrillic word; with the next word it makes a Bulgarian phrase
    const std::string mixed = Cat({"ab\xD0\xB4\xD0\xBE\xD0\xB1", " ", kWorks});
    const auto s = SplitIntoStretches("Hello world " + mixed);
    REQUIRE(s.size() == 2);
    CHECK(s[1].language->id == "bul");
    CHECK(s[1].text == mixed);
}

TEST_CASE("Stretches: punctuation stuck to a word does not change its script") {
    const auto s = SplitIntoStretches("(Hello), \"world\"! " + std::string(kProduct) + ", " + kWorks + ".");
    REQUIRE(s.size() == 2);
    CHECK(s[0].text == "(Hello), \"world\"! ");
    CHECK(s[1].language->id == "bul");
}

TEST_CASE("Stretches: no phrase of two words anywhere gives one stretch in the script with most words") {
    // Bulgarian, English, Bulgarian, English, Bulgarian: every run is one word
    const std::string text = Cat({kProduct, " Windows ", kAnd, " Linux ", kGood});
    auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
    CHECK(s[0].text == text);

    // A single word alone is read in its own script
    s = SplitIntoStretches("Hello");
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "eng");
    s = SplitIntoStretches(kProduct);
    REQUIRE(s.size() == 1);
    CHECK(s[0].language->id == "bul");
}

TEST_CASE("Stretches: joined, they give back the original text exactly") {
    const std::vector<std::string> texts = {
        Cat({"  ", kProduct, "  ", kWorks, "\n\nHello   world \t", kGood, " ", kThisIs, "  \n"}),
        Cat({"Hello world", "\xC2\xA0", kProduct, " ", kWorks, " 12 ... "}),
        "  \n ",
        "x",
        Cat({kProduct, " Hello ", kWorks, " ", kGood, " a b c ", kAnd, " d"}),
        "bad \xFF\xFE bytes here",
    };
    for (const std::string& t : texts) {
        CHECK(Join(SplitIntoStretches(t)) == t);
    }
}

TEST_CASE("Stretches: leading whitespace stays with the first stretch, trailing with the last") {
    const auto s = SplitIntoStretches("  Hello world  ");
    REQUIRE(s.size() == 1);
    CHECK(s[0].text == "  Hello world  ");
}

TEST_CASE("Stretches: phrases of one script around an absorbed island are one stretch") {
    const std::string text = Cat({kProduct, " ", kWorks, " Hello ", kGood, " ", kThisIs});
    const auto s = SplitIntoStretches(text);
    REQUIRE(s.size() == 1);
}

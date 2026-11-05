// Unit tests for ctc.h: the Alphabet mask over a recogniser's classes and the masked CTC
// greedy decode, on hand-made dictionaries and logits. Cyrillic is written as UTF-8 escapes.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "ctc.h"
#include "language.h"

namespace {
// Class layout: 0 = blank, 1..dict.size() = dict entries, then (optionally) the space.
// One row per time step; `hot` gets the high score, `second` (if >= 0) the next best.
std::vector<float> Logits(const std::vector<std::pair<int, int>>& steps, int classes) {
    std::vector<float> v(steps.size() * classes, 0.f);
    for (size_t t = 0; t < steps.size(); ++t) {
        if (steps[t].second >= 0) v[t * classes + steps[t].second] = 0.5f;
        v[t * classes + steps[t].first] = 0.9f;
    }
    return v;
}
const char* kY = "\xD1\x8B";   // ы
const char* kI = "\xD0\xB8";   // и
const char* kYo = "\xD1\x91";  // ё
const char* kE = "\xD0\xB5";   // е
}  // namespace

TEST_CASE("BuildAlphabetMask: blank and space are always allowed, dict entries by Alphabet") {
    const std::vector<std::string> dict = {"a", kI, kY, "7", kYo};
    const std::vector<char> m = BuildAlphabetMask(dict, true, *FindLanguage("bul"));
    REQUIRE(m.size() == dict.size() + 2);
    CHECK(m[0]);        // blank
    CHECK(m[1]);        // a
    CHECK(m[2]);        // и
    CHECK_FALSE(m[3]);  // ы
    CHECK(m[4]);        // 7
    CHECK_FALSE(m[5]);  // ё
    CHECK(m[6]);        // space
}

TEST_CASE("BuildAlphabetMask: no space class when the model has none") {
    const std::vector<char> m = BuildAlphabetMask({"a", "b"}, false, DefaultLanguage());
    CHECK(m.size() == 3);
}

TEST_CASE("BuildAlphabetMask: English drops Cyrillic dictionary entries") {
    const std::vector<char> m = BuildAlphabetMask({"a", kI}, false, *FindLanguage("eng"));
    CHECK(m[1]);
    CHECK_FALSE(m[2]);
}

TEST_CASE("CtcGreedyDecode: unmasked decode merges repeats and drops blanks") {
    const std::vector<std::string> dict = {"a", "b"};
    const std::vector<char> all(4, 1);
    // a a blank a b b -> "aab"
    const auto lg = Logits({{1, -1}, {1, -1}, {0, -1}, {1, -1}, {2, -1}, {2, -1}}, 4);
    const CtcResult r = CtcGreedyDecode(lg.data(), 6, 4, dict, true, all);
    CHECK(r.text == "aab");
    CHECK(r.conf == doctest::Approx(0.9f));
}

TEST_CASE("CtcGreedyDecode: the class after the dictionary is a space when the model has one") {
    const std::vector<std::string> dict = {"a"};
    const std::vector<char> all(3, 1);
    const auto lg = Logits({{1, -1}, {2, -1}, {1, -1}}, 3);
    CHECK(CtcGreedyDecode(lg.data(), 3, 3, dict, true, all).text == "a a");
}

TEST_CASE("CtcGreedyDecode: an out-of-Alphabet winner gives way to the next-best letter") {
    const std::vector<std::string> dict = {kE, kY, kI};  // classes 1 е, 2 ы, 3 и
    const std::vector<char> mask = BuildAlphabetMask(dict, false, *FindLanguage("bul"));
    // The model's top choice is ы (2); the runner-up is и (3).
    const auto lg = Logits({{2, 3}}, 4);
    const CtcResult r = CtcGreedyDecode(lg.data(), 1, 4, dict, false, mask);
    CHECK(r.text == kI);
    CHECK(r.conf == doctest::Approx(0.5f));
}

TEST_CASE("CtcGreedyDecode: when the runner-up is blank, the out-of-Alphabet letter is dropped") {
    const std::vector<std::string> dict = {kE, kY};
    const std::vector<char> mask = BuildAlphabetMask(dict, false, *FindLanguage("bul"));
    const auto lg = Logits({{2, 0}, {1, -1}}, 3);  // ы (blank next best), then е
    CHECK(CtcGreedyDecode(lg.data(), 2, 3, dict, false, mask).text == kE);
}

TEST_CASE("CtcGreedyDecode: no Bulgarian output contains a letter outside the Alphabet") {
    // Every class of a Cyrillic-like dictionary wins in turn; the output is always in the Alphabet.
    const std::vector<std::string> dict = {"a", kE, kY, kYo, kI, "\xD1\x96", "\xD1\x9E", "\xD1\x8D"};
    const Language& bg = *FindLanguage("bul");
    const std::vector<char> mask = BuildAlphabetMask(dict, true, bg);
    const int C = static_cast<int>(dict.size()) + 2;
    for (int hot = 0; hot < C; ++hot) {
        const auto lg = Logits({{hot, 0}, {0, -1}}, C);
        const CtcResult r = CtcGreedyDecode(lg.data(), 2, C, dict, true, mask);
        CAPTURE(hot);
        if (!r.text.empty() && r.text != " ") CHECK(InAlphabet(bg, r.text));
    }
}

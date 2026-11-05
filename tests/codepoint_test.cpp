// Unit tests for codepoint.h: the UTF-8 decoder and the letter tests every text module shares.
// Non-ASCII text is written as UTF-8 escapes so the test does not depend on how the editor saved it.
#include <doctest/doctest.h>

#include <string>

#include "codepoint.h"

TEST_CASE("NextCodePoint decodes one to four byte sequences and advances") {
    const std::string s = "a\xD0\xB4\xE2\x82\xAC\xF0\x9F\x98\x80";  // a, d (Cyrillic), euro sign, grinning face
    size_t i = 0;
    CHECK(NextCodePoint(s, i) == U'a');
    CHECK(i == 1);
    CHECK(NextCodePoint(s, i) == 0x0434);
    CHECK(i == 3);
    CHECK(NextCodePoint(s, i) == 0x20AC);
    CHECK(i == 6);
    CHECK(NextCodePoint(s, i) == 0x1F600);
    CHECK(i == 10);
}

TEST_CASE("NextCodePoint: a stray continuation byte or a byte that is never valid is one U+FFFD") {
    const std::string s = "\x80z\xFFz";
    size_t i = 0;
    CHECK(NextCodePoint(s, i) == 0xFFFD);
    CHECK(i == 1);
    CHECK(NextCodePoint(s, i) == U'z');
    CHECK(NextCodePoint(s, i) == 0xFFFD);
    CHECK(i == 3);
}

TEST_CASE("NextCodePoint: 0xC0, 0xC1 and 0xF5..0xFF are never lead bytes: one U+FFFD for one byte") {
    // As lead bytes they would decode "\xC1\x81" to a valid 'A' (an overlong form) and 0xF5.. to
    // values past U+10FFFF.
    for (const char* bad : {"\xC0\x80", "\xC1\x81", "\xF5\x80\x80\x80", "\xF7\xBF\xBF\xBF", "\xFE", "\xFF"}) {
        CAPTURE(bad);
        const std::string s = bad;
        size_t i = 0;
        CHECK(NextCodePoint(s, i) == 0xFFFD);
        CHECK(i == 1);
        while (i < s.size()) CHECK(NextCodePoint(s, i) == 0xFFFD);  // the continuation bytes left over
    }
    // The lead bytes next to them still lead: U+0080 and U+10FFFF.
    const std::string low = "\xC2\x80";
    size_t i = 0;
    CHECK(NextCodePoint(low, i) == 0x80);
    CHECK(i == 2);
    const std::string high = "\xF4\x8F\xBF\xBF";
    i = 0;
    CHECK(NextCodePoint(high, i) == 0x10FFFF);
    CHECK(i == 4);
}

TEST_CASE("NextCodePoint: a truncated sequence is one U+FFFD, and the byte that broke it is kept") {
    SUBCASE("cut by an ASCII byte") {
        const std::string s = "\xE2\x82z";  // two thirds of the euro sign, then z
        size_t i = 0;
        CHECK(NextCodePoint(s, i) == 0xFFFD);
        CHECK(i == 2);
        CHECK(NextCodePoint(s, i) == U'z');
    }
    SUBCASE("cut by the end of the text") {
        const std::string s = "\xD0";
        size_t i = 0;
        CHECK(NextCodePoint(s, i) == 0xFFFD);
        CHECK(i == 1);
    }
    SUBCASE("cut by another lead byte") {
        const std::string s = "\xD0\xD0\xB4";  // broken lead, then Cyrillic d
        size_t i = 0;
        CHECK(NextCodePoint(s, i) == 0xFFFD);
        CHECK(NextCodePoint(s, i) == 0x0434);
    }
}

TEST_CASE("IsLatinLetter") {
    CHECK(IsLatinLetter(U'a'));
    CHECK(IsLatinLetter(U'Z'));
    CHECK(IsLatinLetter(0xE9));    // e acute
    CHECK(IsLatinLetter(0x24F));   // last of Latin Extended-B
    CHECK(IsLatinLetter(0x1EA0));  // Latin Extended Additional
    CHECK_FALSE(IsLatinLetter(0xD7));  // multiplication sign
    CHECK_FALSE(IsLatinLetter(0xF7));  // division sign
    CHECK_FALSE(IsLatinLetter(U'7'));
    CHECK_FALSE(IsLatinLetter(U' '));
    CHECK_FALSE(IsLatinLetter(0x0434));
    CHECK_FALSE(IsLatinLetter(0xFFFD));
}

TEST_CASE("IsCyrillicLetter") {
    CHECK(IsCyrillicLetter(0x0400));
    CHECK(IsCyrillicLetter(0x0434));
    CHECK(IsCyrillicLetter(0x0481));  // koppa, the last letter before the signs
    CHECK(IsCyrillicLetter(0x048A));  // first letter after them
    CHECK(IsCyrillicLetter(0x052F));  // last of Cyrillic Supplement
    CHECK_FALSE(IsCyrillicLetter(0x03FF));
    CHECK_FALSE(IsCyrillicLetter(0x0530));
    CHECK_FALSE(IsCyrillicLetter(U'a'));
}

TEST_CASE("IsCyrillicLetter: U+0482..U+0489 are Cyrillic signs and combining marks, not letters") {
    for (char32_t c = 0x0482; c <= 0x0489; ++c) CHECK_FALSE(IsCyrillicLetter(c));
}

TEST_CASE("ScriptOfLetter") {
    CHECK(ScriptOfLetter(U'a') == Script::Latin);
    CHECK(ScriptOfLetter(0x0434) == Script::Cyrillic);
    CHECK_FALSE(ScriptOfLetter(U'7').has_value());
    CHECK_FALSE(ScriptOfLetter(0x03B1).has_value());  // Greek alpha: no Language uses it
    CHECK_FALSE(ScriptOfLetter(0x0483).has_value());
}

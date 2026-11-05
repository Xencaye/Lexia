// Unit tests for svg_recolour.h: the icon's colour is replaced, nothing else is.
#include <doctest/doctest.h>

#include <string>

#include "svg_recolour.h"

TEST_CASE("RecolourSvg: a stroke colour becomes the given colour") {
    const std::string in = R"(<svg><path d="M7 20V4" stroke-width="2" stroke="#6E504A"/></svg>)";
    CHECK(RecolourSvg(in, 0xF5EFE8) == R"(<svg><path d="M7 20V4" stroke-width="2" stroke="#F5EFE8"/></svg>)");
}

TEST_CASE("RecolourSvg: a fill colour too, and both quote styles") {
    CHECK(RecolourSvg("<path fill='#abc' stroke=\"#112233\"/>", 0x010203) == "<path fill='#010203' stroke=\"#010203\"/>");
}

TEST_CASE("RecolourSvg: none, currentColor, url and named colours are left alone") {
    const std::string in = R"X(<svg fill="none"><path fill="currentColor" stroke="url(#g)"/><rect fill="red"/></svg>)X";
    CHECK(RecolourSvg(in, 0x123456) == in);
}

TEST_CASE("RecolourSvg: stroke-width, ids and path data are not touched") {
    const std::string in = R"(<path id="stroke-1" d="M1 1 fill #fff" stroke-width="2" stroke-linecap="round"/>)";
    CHECK(RecolourSvg(in, 0x123456) == in);
}

TEST_CASE("RecolourSvg: a style property is recoloured, an alpha part goes with the colour") {
    CHECK(RecolourSvg("<path style=\"fill: #AABBCCDD; stroke:#000;opacity:.5\"/>", 0xFFFFFF) ==
          "<path style=\"fill: #FFFFFF; stroke:#FFFFFF;opacity:.5\"/>");
}

TEST_CASE("RecolourSvg: the colour is six upper-case hex digits, also for a dark one") {
    CHECK(RecolourSvg("<path stroke=\"#6E504A\"/>", 0x0000FF) == "<path stroke=\"#0000FF\"/>");
    CHECK(RecolourSvg("<path stroke=\"#6E504A\"/>", 0x00000A) == "<path stroke=\"#00000A\"/>");
}

TEST_CASE("RecolourSvg: text with no colour, or empty text, comes back as it was") {
    CHECK(RecolourSvg("", 0xFFFFFF).empty());
    CHECK(RecolourSvg("<svg/>", 0xFFFFFF) == "<svg/>");
}

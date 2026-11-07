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

// K13: the engravings are SVG files; the canvas needs their own size to scale them into a box.
TEST_CASE("SvgRootSize: width and height of the root tag") {
    float w = 0, h = 0;
    CHECK(SvgRootSize(R"(<svg width="267" height="231" viewBox="0 0 267 231" fill="none"><path/></svg>)", w, h));
    CHECK(w == doctest::Approx(267));
    CHECK(h == doctest::Approx(231));
    CHECK(SvgRootSize(R"(<?xml version="1.0"?><svg height='20.5px' width='10'></svg>)", w, h));
    CHECK(w == doctest::Approx(10));
    CHECK(h == doctest::Approx(20.5));
}

TEST_CASE("SvgRootSize: the viewBox when there is no usable width and height") {
    float w = 0, h = 0;
    CHECK(SvgRootSize(R"(<svg viewBox="0 0 162 76"></svg>)", w, h));
    CHECK(w == doctest::Approx(162));
    CHECK(h == doctest::Approx(76));
    CHECK(SvgRootSize(R"(<svg width="100%" height="100%" viewBox="0,0,24,24"></svg>)", w, h));
    CHECK(w == doctest::Approx(24));
}

TEST_CASE("SvgRootSize: refuses what it cannot use") {
    float w = 1, h = 1;
    CHECK_FALSE(SvgRootSize("", w, h));
    CHECK_FALSE(SvgRootSize("<path width=\"5\" height=\"5\"/>", w, h));     // no root tag
    CHECK_FALSE(SvgRootSize("<svg width=\"0\" height=\"5\">", w, h));        // zero, and no viewBox
    CHECK_FALSE(SvgRootSize("<svg width=\"99999\" height=\"5\">", w, h));    // too large
    CHECK_FALSE(SvgRootSize("<svg stroke-width=\"5\" height=\"5\">", w, h)); // stroke-width is not width
    CHECK_FALSE(SvgRootSize("<svg viewBox=\"0 0 5\">", w, h));               // three numbers
    CHECK_FALSE(SvgRootSize("<svg width=\"5\" height=\"5\"", w, h));         // the tag never ends
    // A child's size does not count.
    CHECK_FALSE(SvgRootSize("<svg><rect width=\"5\" height=\"5\"/></svg>", w, h));
    CHECK(w == 1);  // unchanged on failure
}

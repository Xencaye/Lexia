// Unit tests for score.h: parsing expected.txt and the Fixture score.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "score.h"

namespace {
using Texts = std::vector<std::string>;

const std::string kHeading = "The Lighthouse at Cape Verde";
const std::string kFirst = "The old lighthouse stands at the edge of the cliff.";
const std::string kSecond = "In winter the storms are fierce and the waves crash.";
}  // namespace

TEST_CASE("ScoreLayout: identical Blocks score perfectly") {
    const Texts blocks = {kHeading, kFirst, kSecond};
    const LayoutScore s = ScoreLayout(blocks, blocks);
    CHECK(s.distance == 0.0);
    CHECK(s.editDistance == 0);
    CHECK(s.blockCountError == 0);
    CHECK(s.expectedBlocks == 3);
    CHECK(s.producedBlocks == 3);
}

TEST_CASE("ScoreLayout: one Block split in two costs a little text and one Block") {
    const Texts expected = {kHeading, "The old lighthouse stands at the edge of the cliff."};
    const Texts produced = {kHeading, "The old lighthouse stands", "at the edge of the cliff."};
    const LayoutScore s = ScoreLayout(expected, produced);
    CHECK(s.blockCountError == 1);
    CHECK(s.producedBlocks == 3);
    CHECK(s.expectedBlocks == 2);
    // The space between the halves became a blank line: one substitution and one insertion.
    CHECK(s.editDistance == 2);
    CHECK(s.distance > 0.0);
    CHECK(s.distance < 0.05);
}

TEST_CASE("ScoreLayout: two Blocks merged into one is the mirror of a split") {
    const Texts split = {kHeading, "The old lighthouse stands", "at the edge of the cliff."};
    const Texts merged = {kHeading, "The old lighthouse stands at the edge of the cliff."};
    const LayoutScore s = ScoreLayout(split, merged);
    CHECK(s.blockCountError == 1);
    CHECK(s.editDistance == 2);
}

TEST_CASE("ScoreLayout: two Blocks swapped is far from the expected order but has the right count") {
    const Texts expected = {kHeading, kFirst, kSecond};
    const Texts produced = {kHeading, kSecond, kFirst};
    const LayoutScore s = ScoreLayout(expected, produced);
    CHECK(s.blockCountError == 0);
    CHECK(s.distance > 0.3);
    CHECK(s.distance <= 1.0);
}

TEST_CASE("ScoreLayout: nothing produced is the worst score") {
    const LayoutScore s = ScoreLayout({kHeading, kFirst}, {});
    CHECK(s.distance == 1.0);
    CHECK(s.blockCountError == 2);
    CHECK(s.producedBlocks == 0);
    CHECK(s.expectedBlocks == 2);
}

TEST_CASE("ScoreLayout: expecting nothing and finding nothing is perfect; finding something is not") {
    const LayoutScore none = ScoreLayout({}, {});
    CHECK(none.distance == 0.0);
    CHECK(none.blockCountError == 0);
    const LayoutScore extra = ScoreLayout({}, {kHeading});
    CHECK(extra.distance == 1.0);
    CHECK(extra.blockCountError == 1);
}

TEST_CASE("ScoreLayout: Cyrillic is counted in code points, not bytes") {
    // "Здравей" against "Здравец": one letter differs. In bytes that letter is 2 bytes,
    // so a byte count would give 2 over a length of 14.
    const std::string expected = "\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB5\xD0\xB9";
    const std::string produced = "\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB5\xD1\x86";
    const LayoutScore s = ScoreLayout({expected}, {produced});
    CHECK(s.editDistance == 1);
    CHECK(s.distance == doctest::Approx(1.0 / 7.0));
}

TEST_CASE("ScoreLayout: white space inside a Block and empty Blocks do not count") {
    const Texts expected = {"one two\nthree", "four"};
    const Texts produced = {"  one   two three ", "", "four"};
    const LayoutScore s = ScoreLayout(expected, produced);
    CHECK(s.distance == 0.0);
    CHECK(s.producedBlocks == 2);
}

TEST_CASE("ScoreLayout: a long text is scored without trouble") {
    const std::string para(6000, 'a');
    std::string other = para;
    other[3000] = 'b';
    const LayoutScore s = ScoreLayout({para}, {other});
    CHECK(s.editDistance == 1);
}

TEST_CASE("ParseExpectedBlocks: Blocks are separated by blank lines") {
    const Texts b = ParseExpectedBlocks("Heading\n\nFirst paragraph.\n\nCaption\n");
    REQUIRE(b.size() == 3);
    CHECK(b[0] == "Heading");
    CHECK(b[1] == "First paragraph.");
    CHECK(b[2] == "Caption");
}

TEST_CASE("ParseExpectedBlocks: CRLF, a BOM, trailing white space and extra blank lines") {
    const Texts b = ParseExpectedBlocks("\xEF\xBB\xBF" "Heading  \r\n\r\n\r\n   \r\nSecond\r\nline  of it\r\n\r\n\r\n");
    REQUIRE(b.size() == 2);
    CHECK(b[0] == "Heading");
    CHECK(b[1] == "Second line of it");
}

TEST_CASE("ParseExpectedBlocks: an empty or blank file has no Blocks") {
    CHECK(ParseExpectedBlocks("").empty());
    CHECK(ParseExpectedBlocks("\n\r\n  \n").empty());
    CHECK(ParseExpectedBlocks("\xEF\xBB\xBF").empty());
}

TEST_CASE("ParseExpectedBlocks: a file with no final newline still gives its last Block") {
    const Texts b = ParseExpectedBlocks("One\n\nTwo");
    REQUIRE(b.size() == 2);
    CHECK(b[1] == "Two");
}

TEST_CASE("BlockTexts: the text of each Block, in order") {
    TextBlock a, b;
    a.text = "alpha";
    b.text = "beta";
    const Texts t = BlockTexts({a, b});
    REQUIRE(t.size() == 2);
    CHECK(t[0] == "alpha");
    CHECK(t[1] == "beta");
}

TEST_CASE("ScoreLayout: case does not count, in Latin or Cyrillic") {
    const LayoutScore latin = ScoreLayout({"Loading"}, {"LOADING"});
    CHECK(latin.editDistance == 0);
    const LayoutScore cyrillic = ScoreLayout({"Зареждане ЁЖ"}, {"зАРЕЖДАНЕ ёж"});
    CHECK(cyrillic.editDistance == 0);
    CHECK(ScoreLayout({"Loading"}, {"Lodaing"}).editDistance == 2);
}

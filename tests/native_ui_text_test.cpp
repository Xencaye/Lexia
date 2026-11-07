// Word wrapping and the Paragraph widget of the native Settings window (K1.b): {name} bold runs,
// per-line indents (the About page's text goes round an engraving), paragraph gap, the height that
// follows the width. The FakeCanvas measures 0.5 * size per code point: 8 DIPs a letter at 16 px.
#include "native_ui/display.h"
#include "native_ui/text_layout.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

using namespace nui;
using namespace nui::test;

namespace {

std::vector<TextRun> Plain(const char* text) { return {TextRun{text, false}}; }

std::string Joined(const WrappedLine& line) {
    std::string s;
    for (const LinePiece& p : line.pieces) s += p.text + "|";
    return s;
}

}  // namespace

TEST_CASE("ParseTemplate: {name} becomes a bold run, the rest stays plain") {
    const auto runs = ParseTemplate("built with {games} and {comics} in mind",
                                    {{"games", "games"}, {"comics", "comics"}});
    REQUIRE(runs.size() == 5);
    CHECK(runs[0].text == "built with ");
    CHECK_FALSE(runs[0].bold);
    CHECK(runs[1].text == "games");
    CHECK(runs[1].bold);
    CHECK(runs[2].text == " and ");
    CHECK(runs[3].bold);
    CHECK(runs[4].text == " in mind");
}

TEST_CASE("ParseTemplate: an unknown name, an open brace and no braces stay as they are") {
    auto runs = ParseTemplate("a {nope} b { c", {});
    REQUIRE(runs.size() == 1);
    CHECK(runs[0].text == "a {nope} b { c");
    CHECK(ParseTemplate("", {}).empty());
    runs = ParseTemplate("{x}", {{"x", "Y"}});
    REQUIRE(runs.size() == 1);
    CHECK(runs[0].text == "Y");
    CHECK(runs[0].bold);
}

TEST_CASE("WrapRuns: breaks at spaces when the next word does not fit") {
    FakeCanvas c;
    TextStyle st;  // 16: a letter is 8, a space 8
    const auto lines = WrapRuns(c, Plain("aaaa bbbb cccc"), st, 80);
    REQUIRE(lines.size() == 2);
    CHECK(Joined(lines[0]) == "aaaa|bbbb|");
    CHECK(lines[0].pieces[0].x == 0);
    CHECK(lines[0].pieces[1].x == 40);  // 32 + a space of 8
    CHECK(Joined(lines[1]) == "cccc|");
    CHECK(lines[1].pieces[0].x == 0);
}

TEST_CASE("WrapRuns: a word wider than the line stands alone; empty text gives no line") {
    FakeCanvas c;
    TextStyle st;
    const auto lines = WrapRuns(c, Plain("ab abcdefghijklmnop cd"), st, 80);
    REQUIRE(lines.size() == 3);
    CHECK(Joined(lines[1]) == "abcdefghijklmnop|");
    CHECK(WrapRuns(c, Plain(""), st, 80).empty());
    CHECK(WrapRuns(c, Plain("   "), st, 80).empty());
}

TEST_CASE("WrapRuns: punctuation after a bold word is never split from it") {
    FakeCanvas c;
    TextStyle st;
    const std::vector<TextRun> runs{{"see ", false}, {"games", true}, {", ok", false}};
    const auto lines = WrapRuns(c, runs, st, 60);
    REQUIRE(lines.size() == 3);
    CHECK(Joined(lines[0]) == "see|");
    CHECK(Joined(lines[1]) == "games|,|");
    CHECK(lines[1].pieces[1].x == 40);  // right after the word, no space
    CHECK(lines[1].pieces[0].bold);
    CHECK_FALSE(lines[1].pieces[1].bold);
    CHECK(Joined(lines[2]) == "ok|");
}

TEST_CASE("WrapRuns: bold pieces are measured bold") {
    FakeCanvas c;
    c.boldFactor = 1.5f;
    TextStyle st;
    const auto lines = WrapRuns(c, std::vector<TextRun>{{"ab ", false}, {"cd", true}}, st, 500);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].pieces[1].width == doctest::Approx(2 * 8 * 1.5f));
    CHECK(lines[0].pieces[1].x == 16 + 8);
}

TEST_CASE("WrapRuns: each line may have its own left indent; the right edge stays") {
    FakeCanvas c;
    TextStyle st;
    // Four words of 32: with indent 27 only one fits in 90 (27 + 32 = 59, + 8 + 32 = 99 > 90); then
    // indent 23, and indent 15 lets two in (15 + 32 + 8 + 32 = 87).
    const auto lines = WrapRuns(c, Plain("aaaa bbbb cccc dddd"), st, 90, {27, 23, 15});
    REQUIRE(lines.size() == 3);
    CHECK(Joined(lines[0]) == "aaaa|");
    CHECK(lines[0].indent == 27);
    CHECK(lines[0].pieces[0].x == 27);
    CHECK(lines[1].indent == 23);
    CHECK(lines[1].pieces[0].x == 23);
    CHECK(Joined(lines[2]) == "cccc|dddd|");
    CHECK(lines[2].pieces[0].x == 15);
    CHECK(lines[2].pieces[1].x == 15 + 32 + 8);
}

TEST_CASE("WrapRuns: lines past the indent list have no indent") {
    FakeCanvas c;
    TextStyle st;
    const auto lines = WrapRuns(c, Plain("aaaa bbbb cccc dddd eeee"), st, 40, {10});
    REQUIRE(lines.size() == 5);
    CHECK(lines[0].pieces[0].x == 10);
    CHECK(lines[1].pieces[0].x == 0);
    CHECK(lines[4].pieces[0].x == 0);
}

TEST_CASE("WrapRuns: the space width is measured from a pair, so a trimming text engine is fine") {
    FakeCanvas c;
    TextStyle st;
    CHECK(SpaceWidth(c, st) == 8);
    st.size = 13;
    CHECK(SpaceWidth(c, st) == 6.5f);
    CHECK(LineHeight(c, st) == doctest::Approx(1.35f * 13));
}

// --------------------------------------------------------------------------------- Paragraph ---

TEST_CASE("Paragraph: paragraphs are one per newline, a gap apart; the height follows the width") {
    Ui ui;
    auto* p = ui.screen.Root().Add<Paragraph>();
    p->SetText("one two\nthree four five");
    const float lh = 1.35f * 16;
    SizeF s = p->Measure(ui.canvas, 400);
    CHECK(s.h == doctest::Approx(2 * lh + 8));  // two paragraphs of one line, 8 apart
    s = p->Measure(ui.canvas, 40);
    // "one" "two" / "three" / "four" / "five" fit one per line in 40 (three = 40): 2 + 1 + 1 + 1 lines
    CHECK(s.h == doctest::Approx(5 * lh + 8));
    p->SetParagraphGap(20);
    CHECK(p->Measure(ui.canvas, 400).h == doctest::Approx(2 * lh + 20));
    const auto layout = p->LayoutFor(ui.canvas, 400);
    REQUIRE(layout.lines.size() == 2);
    CHECK(layout.lines[0].y == 0);
    CHECK(layout.lines[1].y == doctest::Approx(lh + 20));
    CHECK(p->Measure(ui.canvas, 100).w == 100);
}

TEST_CASE("Paragraph: {name} placeholders draw as bold text") {
    Ui ui;
    auto* p = ui.screen.Root().Add<Paragraph>();
    p->SetBounds({10, 10, 300, 60});
    p->SetText("for {games} and fun", {{"games", "Undertale"}});
    ui.Frame();
    REQUIRE(ui.canvas.FindText("Undertale"));
    CHECK(ui.canvas.FindText("Undertale")->style.weight == FontWeight::Bold);
    CHECK(ui.canvas.FindText("for")->style.weight == FontWeight::Regular);
    CHECK(ui.canvas.FindText("fun")->colour.rgb == ui.pal.ink.rgb);
    CHECK(p->PlainText() == "for Undertale and fun");
    CHECK(p->Access().role == Role::Text);
    CHECK(p->Access().value == "for Undertale and fun");
}

TEST_CASE("Paragraph: the indents count the lines of the whole widget, like the About text round the picture") {
    Ui ui;
    auto* p = ui.screen.Root().Add<Paragraph>();
    p->SetBounds({50, 100, 90, 300});
    p->SetIndents({27, 23, 15, 11, 7, 0});
    p->SetText("aaaa bbbb cccc dddd");
    const auto layout = p->LayoutFor(ui.canvas, 90);
    REQUIRE(layout.lines.size() == 3);
    CHECK(layout.lines[0].line.indent == 27);
    CHECK(layout.lines[1].line.indent == 23);
    CHECK(layout.lines[2].line.indent == 15);
    ui.Frame();
    // Drawn at the widget's left edge plus the line's indent, one line height apart.
    CHECK(ui.canvas.FindText("aaaa")->rect.x == 50 + 27);
    CHECK(ui.canvas.FindText("bbbb")->rect.x == 50 + 23);
    CHECK(ui.canvas.FindText("cccc")->rect.x == 50 + 15);
    CHECK(ui.canvas.FindText("bbbb")->rect.y - ui.canvas.FindText("aaaa")->rect.y ==
          doctest::Approx(1.35f * 16));
}

TEST_CASE("Paragraph: the indent list goes on into the next paragraph") {
    Ui ui;
    auto* p = ui.screen.Root().Add<Paragraph>();
    p->SetIndents({27, 23});
    p->SetText("aaaa\nbbbb");
    const auto layout = p->LayoutFor(ui.canvas, 200);
    REQUIRE(layout.lines.size() == 2);
    CHECK(layout.lines[0].line.indent == 27);
    CHECK(layout.lines[1].line.indent == 23);
}

TEST_CASE("Paragraph: no text is no height") {
    Ui ui;
    auto* p = ui.screen.Root().Add<Paragraph>();
    CHECK(p->Measure(ui.canvas, 300).h == 0);
}

// Unit tests for read_along.h: cutting a chunk into words, mapping spoken words to text words, the
// box of a word on the screen, the underline bar and the track of Readings. Cyrillic is written as
// UTF-8 escapes so the test does not depend on how the editor saved it.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "read_along.h"
#include "stretch.h"

namespace {

// The text a range stands for.
std::string Cut(const std::string& text, TextRange r) { return text.substr(r.start, r.end - r.start); }

std::vector<std::string> Cuts(const std::string& text, const std::vector<TextRange>& ranges) {
    std::vector<std::string> out;
    for (const TextRange& r : ranges) out.push_back(r.Empty() ? std::string("<none>") : Cut(text, r));
    return out;
}

using Words = std::vector<std::string>;

// A chunk of one sentence: `phonemeCounts[i]` phonemes in word i, 10 samples each, a ' ' phoneme
// between words, '^' at the start and '$' at the end (as Piper makes them).
SpeechChunk MakeChunk(const std::vector<int>& phonemeCounts) {
    SpeechChunk c;
    c.sampleRate = 100;
    int at = 0;
    c.phonemes.push_back({U'^', 0, 0});
    for (size_t w = 0; w < phonemeCounts.size(); ++w) {
        if (w > 0) c.phonemes.push_back({U' ', at, 10});
        if (w > 0) at += 10;
        for (int p = 0; p < phonemeCounts[w]; ++p) {
            c.phonemes.push_back({U'a', at, 10});
            at += 10;
        }
    }
    c.phonemes.push_back({U'$', at, 0});
    c.samples.assign(static_cast<size_t>(at), 0.f);
    return c;
}

Rect R(int l, int t, int r, int b) { return Rect{l, t, r, b}; }

TextLine Line(Rect box, const std::string& text) {
    TextLine l;
    l.box = box;
    l.text = text;
    l.conf = 90.f;
    return l;
}

TextBlock BlockOf(std::vector<TextLine> lines, const std::string& text) {
    TextBlock b;
    b.lines = std::move(lines);
    b.text = text;
    b.box = b.lines.front().box;
    return b;
}

}  // namespace

// ------------------------------------------------------------------- the sound ---

TEST_CASE("Read-along: markers and punctuation are not spoken phonemes") {
    CHECK_FALSE(IsSpokenPhoneme(0));
    CHECK_FALSE(IsSpokenPhoneme(U'^'));
    CHECK_FALSE(IsSpokenPhoneme(U'$'));
    CHECK_FALSE(IsSpokenPhoneme(U' '));
    CHECK_FALSE(IsSpokenPhoneme(U','));
    CHECK_FALSE(IsSpokenPhoneme(U'.'));
    CHECK_FALSE(IsSpokenPhoneme(0x2026));
    CHECK(IsSpokenPhoneme(U'a'));
    CHECK(IsSpokenPhoneme(0x0259));  // schwa
}

TEST_CASE("Read-along: a chunk is cut into a piece per word, counting spoken phonemes") {
    const SpeechChunk c = MakeChunk({2, 3, 1});
    const std::vector<WordPiece> p = CutIntoWords(c);
    REQUIRE(p.size() == 3);
    CHECK(p[0].begin == 0);
    CHECK(p[0].end == 20);
    CHECK(p[0].phonemes == 2);
    CHECK(p[1].begin == 20);
    CHECK(p[1].end == 60);  // the silence of the space belongs to the start of the piece
    CHECK(p[1].phonemes == 3);
    CHECK(p[2].begin == 60);
    CHECK(p[2].end == 80);
    CHECK(p[2].phonemes == 1);
}

TEST_CASE("Read-along: a chunk without timings is one piece with no word; an empty one gives none") {
    SpeechChunk c;
    c.sampleRate = 100;
    c.samples.assign(50, 0.f);
    const std::vector<WordPiece> p = CutIntoWords(c);
    REQUIRE(p.size() == 1);
    CHECK(p[0].begin == 0);
    CHECK(p[0].end == 50);
    CHECK(p[0].phonemes == 0);

    CHECK(CutIntoWords(SpeechChunk{}).empty());
}

TEST_CASE("Read-along: a space at the start, a repeated one and a punctuation-only piece cut nothing wrong") {
    SpeechChunk c;
    c.sampleRate = 100;
    c.samples.assign(40, 0.f);
    c.phonemes = {{U' ', 0, 5}, {U'a', 0, 10}, {U' ', 10, 5}, {U' ', 10, 5}, {U'b', 15, 10}, {U' ', 25, 5}, {U',', 30, 10}};
    const std::vector<WordPiece> p = CutIntoWords(c);
    REQUIRE(p.size() == 3);
    CHECK(p[0].end == 10);
    CHECK(p[0].phonemes == 1);
    CHECK(p[1].end == 25);
    CHECK(p[1].phonemes == 1);
    CHECK(p[2].end == 40);
    CHECK(p[2].phonemes == 0);  // only a comma: nothing to show
}

TEST_CASE("Read-along: the stretches that are spoken, with the offsets of their chunks") {
    const std::string latin = "Hello there friends ";
    const std::string cyr = "\xD0\xA2\xD0\xBE\xD0\xB2\xD0\xB0 \xD0\xB5";  // "Това е"
    const std::vector<TextRange> one = SpokenStretchRanges("Hello there world");
    REQUIRE(one.size() == 1);
    CHECK(one[0].start == 0);
    CHECK(one[0].end == 17);

    const std::string text = latin + cyr;
    const std::vector<TextRange> two = SpokenStretchRanges(text);
    REQUIRE(two.size() == 2);
    CHECK(two[0].start == 0);
    CHECK(two[0].end == latin.size());
    CHECK(two[1].start == latin.size());
    CHECK(two[1].end == text.size());

    CHECK(SpokenStretchRanges("").empty());
}

// ----------------------------------------------------------- sound -> text words ---

TEST_CASE("Read-along: spoken words map one to one when the counts agree") {
    const std::string text = "Hello big world.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {3, 2, 4})) == Words{"Hello", "big", "world"});  // no full stop in the range
}

TEST_CASE("Read-along: a sentence is picked by its index; past the end is the last one") {
    const std::string text = "One two. Three four five! Six?";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1})) == Words{"One", "two"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1, 1})) == Words{"Three", "four", "five"});
    CHECK(Cuts(text, SpokenWordRanges(text, 2, {1})) == Words{"Six"});
    CHECK(Cuts(text, SpokenWordRanges(text, 9, {1})) == Words{"Six"});
}

TEST_CASE("Read-along: quotes, brackets and commas stay out of a word; inner marks stay in") {
    const std::string text = "He said \"stop.\" Then (don't, 3.5) left.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1, 1})) == Words{"He", "said", "stop"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1, 1, 1})) == Words{"Then", "don't", "3.5", "left"});
}

TEST_CASE("Read-along: a sentence of marks only is not a sentence; a mark alone is not a word") {
    const std::string text = "Wait ... what - now? Next.";
    // espeak-ng takes "..." as a pause inside a sentence, not as its end (checked against Piper: this
    // text is two chunks). The lone "-" is not a word.
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1, 1})) == Words{"Wait", "what", "now"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1})) == Words{"Next"});
}

// R42: where Piper (espeak-ng, readclause.c) cuts its chunks. The expectations below were checked
// against the real Piper with the shipped Voice: the number of chunks it made is in each comment.
TEST_CASE("Read-along: a full stop before a capital ends a sentence, also after Mr. / Dr. / initials") {
    // 3 chunks: espeak-ng has no list of abbreviations, so "Mr." before a capital is a sentence end.
    const std::string text = "Mr. Smith went home. He slept.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1})) == Words{"Mr"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1, 1})) == Words{"Smith", "went", "home"});
    CHECK(Cuts(text, SpokenWordRanges(text, 2, {1, 1})) == Words{"He", "slept"});

    // 4 chunks.
    const std::string initials = "A.B. went home. So did C. D. Smith.";
    CHECK(Cuts(initials, SpokenWordRanges(initials, 0, {1, 1, 1})) == Words{"A.B", "went", "home"});
    CHECK(Cuts(initials, SpokenWordRanges(initials, 1, {1, 1, 1})) == Words{"So", "did", "C"});
    CHECK(Cuts(initials, SpokenWordRanges(initials, 2, {1})) == Words{"D"});
    CHECK(Cuts(initials, SpokenWordRanges(initials, 3, {1})) == Words{"Smith"});
}

TEST_CASE("Read-along: a full stop before a lower-case word is not a sentence end") {
    // 2 chunks: "Mr." and "e.g." before a lower-case word stay inside the sentence.
    const std::string mr = "He met Mr. smith. Then left.";
    CHECK(Cuts(mr, SpokenWordRanges(mr, 0, {1, 1, 1, 1})) == Words{"He", "met", "Mr", "smith"});
    CHECK(Cuts(mr, SpokenWordRanges(mr, 1, {1, 1})) == Words{"Then", "left"});

    // 2 chunks.
    const std::string eg = "Use tools, e.g. a saw and i.e. a drill. Done.";
    CHECK(Cuts(eg, SpokenWordRanges(eg, 0, {1, 1, 1, 1, 1, 1, 1, 1, 1})) ==
          Words{"Use", "tools", "e.g", "a", "saw", "and", "i.e", "a", "drill"});
    CHECK(Cuts(eg, SpokenWordRanges(eg, 1, {1})) == Words{"Done"});

    // 2 chunks: "Etc." before "and" goes on, "more." before a capital ends.
    const std::string etc = "Etc. and more. And so on.";
    CHECK(Cuts(etc, SpokenWordRanges(etc, 0, {1, 1, 1})) == Words{"Etc", "and", "more"});
    CHECK(Cuts(etc, SpokenWordRanges(etc, 1, {1, 1, 1})) == Words{"And", "so", "on"});

    // 2 chunks: two dots count as one full stop; a lone lower-case letter after one goes on.
    const std::string two = "Hello.  Two spaces. x";
    CHECK(Cuts(two, SpokenWordRanges(two, 0, {1})) == Words{"Hello"});
    CHECK(Cuts(two, SpokenWordRanges(two, 1, {1, 1, 1})) == Words{"Two", "spaces", "x"});
}

TEST_CASE("Read-along: a line break after a full stop ends the sentence even before a lower-case word") {
    // 3 chunks.
    const std::string text = "Line one.\nline two. Line three.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1})) == Words{"Line", "one"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"line", "two"});
    CHECK(Cuts(text, SpokenWordRanges(text, 2, {1, 1})) == Words{"Line", "three"});
}

TEST_CASE("Read-along: ? and ! always end a sentence; an ellipsis does not, unless a ? or ! comes first") {
    // 2 chunks: the ellipsis after "Wait" is a pause.
    const std::string text = "Wait... what? Yes!";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1})) == Words{"Wait", "what"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1})) == Words{"Yes"});

    // 3 chunks: "?..." ends at the "?".
    const std::string question = "Really?... yes. Ok.";
    CHECK(Cuts(question, SpokenWordRanges(question, 0, {1})) == Words{"Really"});
    CHECK(Cuts(question, SpokenWordRanges(question, 1, {1})) == Words{"yes"});
    CHECK(Cuts(question, SpokenWordRanges(question, 2, {1})) == Words{"Ok"});

    // A question mark before a lower-case word still ends (only the full stop is doubtful).
    const std::string lower = "Is it? yes. Ok.";
    CHECK(Cuts(lower, SpokenWordRanges(lower, 1, {1})) == Words{"yes"});
}

TEST_CASE("Read-along: a full stop with a closing quote or bracket after it ends the sentence") {
    // 2 chunks: with the quote between the stop and the space, espeak-ng never looks at the next word.
    const std::string quote = "He said \"stop.\" then left.";
    CHECK(Cuts(quote, SpokenWordRanges(quote, 0, {1, 1, 1})) == Words{"He", "said", "stop"});
    CHECK(Cuts(quote, SpokenWordRanges(quote, 1, {1, 1})) == Words{"then", "left"});

    // 3 chunks: "End.)".
    const std::string bracket = "End.) Next one. Over.";
    CHECK(Cuts(bracket, SpokenWordRanges(bracket, 0, {1})) == Words{"End"});
    CHECK(Cuts(bracket, SpokenWordRanges(bracket, 1, {1, 1})) == Words{"Next", "one"});
}

TEST_CASE("Read-along: a decimal number and a stop inside a word are not sentence ends") {
    // 2 chunks.
    const std::string text = "It costs 3.5 dollars. Fine.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1, 1, 1})) == Words{"It", "costs", "3.5", "dollars"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1})) == Words{"Fine"});
    // 2 chunks.
    const std::string glued = "Mr.Smith went. Ok.";
    CHECK(Cuts(glued, SpokenWordRanges(glued, 0, {1, 1})) == Words{"Mr.Smith", "went"});
}

TEST_CASE("Read-along: a number after a full stop starts a new sentence, as in espeak-ng") {
    // 6 chunks in all: "fig." before "2)" ends a sentence, "(Yes.)" is one of its own.
    const std::string text = "Now (see fig. 2) it ends. (Yes.) Then.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1, 1})) == Words{"Now", "see", "fig"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1, 1})) == Words{"2", "it", "ends"});
    CHECK(Cuts(text, SpokenWordRanges(text, 2, {1})) == Words{"Yes"});
    CHECK(Cuts(text, SpokenWordRanges(text, 3, {1})) == Words{"Then"});
}

TEST_CASE("Read-along: a number said as several words stays on its text word") {
    const std::string text = "I owe 2024 now";
    // I, owe, twenty, twenty, four, now
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {1, 2, 5, 5, 3, 2});
    CHECK(Cuts(text, r) == Words{"I", "owe", "2024", "2024", "2024", "now"});
}

TEST_CASE("Read-along: fewer spoken words than text words skips ahead, never back") {
    const std::string text = "alpha beta gamma delta epsilon";
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {4, 4, 4});
    REQUIRE(r.size() == 3);
    CHECK(Cut(text, r[0]) == "alpha");
    CHECK(Cut(text, r[1]) == "gamma");    // the middle of the middle third (50%) lies in "gamma" (35%..54%)
    CHECK(Cut(text, r[2]) == "epsilon");  // 83% lies in the last word (73%..100%)
}

TEST_CASE("Read-along: ranges are always inside the text and monotonic") {
    const std::string text = "A short text, with 12 words in it; and some more.  Here is another one.";
    for (size_t sentence = 0; sentence < 4; ++sentence) {
        for (size_t spoken = 1; spoken < 12; ++spoken) {
            std::vector<int> counts(spoken, 3);
            const std::vector<TextRange> r = SpokenWordRanges(text, sentence, counts);
            REQUIRE(r.size() == spoken);
            size_t last = 0;
            for (const TextRange& x : r) {
                CHECK(x.start >= last);
                CHECK(x.end <= text.size());
                CHECK(x.start < x.end);
                last = x.start;
            }
        }
    }
}

TEST_CASE("Read-along: no text words, no spoken words, or a cut-off character gives nothing wrong") {
    CHECK(SpokenWordRanges("", 0, {1, 2}).size() == 2);
    CHECK(SpokenWordRanges("", 0, {1, 2})[0].Empty());
    CHECK(SpokenWordRanges("... !!!", 0, {1})[0].Empty());
    CHECK(SpokenWordRanges("word", 0, {}).empty());
    // A lone lead byte at the end of the text (cut-off UTF-8) must not read past it.
    const std::string broken = "ab \xD0";
    for (const TextRange& r : SpokenWordRanges(broken, 0, {1, 1})) CHECK(r.end <= broken.size());
}

TEST_CASE("Read-along: Cyrillic words are found by character, not byte") {
    const std::string text = "\xD0\xA2\xD0\xBE\xD0\xB2\xD0\xB0 \xD0\xB5 \xD0\xB4\xD0\xBE\xD0\xB1\xD1\x8A\xD1\x80.";  // "Това е добър."
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {3, 1, 4});
    REQUIRE(r.size() == 3);
    CHECK(Cut(text, r[0]) == "\xD0\xA2\xD0\xBE\xD0\xB2\xD0\xB0");
    CHECK(Cut(text, r[1]) == "\xD0\xB5");
    CHECK(Cut(text, r[2]) == "\xD0\xB4\xD0\xBE\xD0\xB1\xD1\x8A\xD1\x80");
}

// ------------------------------------------------------------- text -> the screen ---

TEST_CASE("Read-along: a word's box is its share of the Line's characters") {
    // 10 characters over 100 px: 10 px each.
    const TextBlock b = BlockOf({Line(R(100, 50, 200, 70), "abcd efghi")}, "abcd efghi");
    const std::vector<Rect> first = WordBoxes(b, {0, 4});
    REQUIRE(first.size() == 1);
    CHECK(first[0].left == 100);
    CHECK(first[0].right == 140);
    CHECK(first[0].top == 50);
    CHECK(first[0].bottom == 70);
    const std::vector<Rect> second = WordBoxes(b, {5, 10});
    REQUIRE(second.size() == 1);
    CHECK(second[0].left == 150);
    CHECK(second[0].right == 200);
}

TEST_CASE("Read-along: a word of the second Line lies on that Line") {
    const TextBlock b = BlockOf({Line(R(0, 0, 70, 10), "one two"), Line(R(0, 20, 50, 30), "three")}, "one two three");
    const std::vector<Rect> w = WordBoxes(b, {8, 13});
    REQUIRE(w.size() == 1);
    CHECK(w[0].top == 20);
    CHECK(w[0].left == 0);
    CHECK(w[0].right == 50);
}

TEST_CASE("Read-along: a word that wraps over two Lines has two boxes; a hyphen joined word too") {
    // Space join: "a b" / "c d": the range "b c" touches both Lines.
    const TextBlock s = BlockOf({Line(R(0, 0, 30, 10), "a b"), Line(R(0, 20, 30, 30), "c d")}, "a b c d");
    const std::vector<Rect> both = WordBoxes(s, {2, 5});
    REQUIRE(both.size() == 2);
    CHECK(both[0].top == 0);
    CHECK(both[0].left == 20);  // "b" is the third of three characters
    CHECK(both[0].right == 30);
    CHECK(both[1].top == 20);
    CHECK(both[1].left == 0);
    CHECK(both[1].right == 10);

    // Hyphen join (layout.cpp drops the '-' when the next Line starts in lower case):
    // "an inter-" + "national word" = "an international word".
    const TextBlock h = BlockOf({Line(R(0, 0, 90, 10), "an inter-"), Line(R(0, 20, 130, 30), "national word")},
                                "an international word");
    const std::vector<Rect> w = WordBoxes(h, {3, 16});
    REQUIRE(w.size() == 2);
    CHECK(w[0].left == 30);
    CHECK(w[0].right == 90);  // runs to the end of the Line: the hyphen's place is included
    CHECK(w[0].top == 0);
    CHECK(w[1].left == 0);
    CHECK(w[1].right == 80);
    CHECK(w[1].top == 20);
    // The word after the hyphen join is on the second Line, found by its place in the joined text.
    const std::vector<Rect> after = WordBoxes(h, {17, 21});
    REQUIRE(after.size() == 1);
    CHECK(after[0].top == 20);
    CHECK(after[0].left == 90);
    CHECK(after[0].right == 130);
}

TEST_CASE("Read-along: UTF-8 characters count once in a Line") {
    const std::string text = "\xD0\xB4\xD0\xB2\xD0\xB5 \xD1\x82\xD1\x80\xD0\xB8";  // "две три": 7 characters
    const TextBlock b = BlockOf({Line(R(0, 0, 70, 10), text)}, text);
    const std::vector<Rect> w = WordBoxes(b, {7, 13});  // "три"
    REQUIRE(w.size() == 1);
    CHECK(w[0].left == 40);
    CHECK(w[0].right == 70);
}

TEST_CASE("Read-along: unknown ranges and Lines that do not match the text give no box") {
    const TextBlock b = BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two");
    CHECK(WordBoxes(b, {}).empty());
    CHECK(WordBoxes(b, {5, 5}).empty());
    CHECK(WordBoxes(b, {40, 50}).empty());
    CHECK(WordBoxes(b, {4, 100}).size() == 1);  // clamped to the text

    TextBlock changed = b;
    changed.text = "one two three";  // the Lines no longer make up the text
    CHECK(WordBoxes(changed, {0, 3}).empty());
}

TEST_CASE("Read-along: a Block with no Lines is taken as one Line") {
    TextBlock b;
    b.box = R(10, 10, 110, 30);
    b.text = "abcde fghij";
    const std::vector<Rect> w = WordBoxes(b, {6, 11});
    REQUIRE(w.size() == 1);
    CHECK(w[0].left == 10 + 100 * 6 / 11);
    CHECK(w[0].right == 110);
}

TEST_CASE("Read-along: the underline bar sits under the box") {
    const Rect bar = UnderlineBar(R(10, 20, 50, 40), 3);
    CHECK(bar.left == 10);
    CHECK(bar.right == 50);
    CHECK(bar.top == 40);
    CHECK(bar.bottom == 43);
    const Rect lower = UnderlineBar(R(10, 20, 50, 40), 4, 2);
    CHECK(lower.top == 42);
    CHECK(lower.bottom == 46);
    CHECK(UnderlineBar(R(0, 0, 5, 5), 0).bottom == 6);  // at least one pixel
}

// ------------------------------------------------------------------ the Readings ---

TEST_CASE("Read-along: the track gives bars for the Readings it knows and no others") {
    ReadAlongTrack track;
    CHECK(track.Empty());
    track.Add(0, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two")});  // id 0: not kept
    CHECK(track.Empty());
    track.Add(7, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two")});
    track.Add(8, {BlockOf({Line(R(0, 100, 70, 110), "xx yy")}, "xx yy"),
                  BlockOf({Line(R(0, 200, 40, 210), "ab cd")}, "ab cd")});
    CHECK_FALSE(track.Empty());
    CHECK(track.Knows(7));
    CHECK_FALSE(track.Knows(9));

    // G3: the bar touches the line box (gap 0, what the Overlay and the layer use): it starts at the box bottom.
    const std::vector<Rect> bars = track.Bars(HeardWord{7, 0, {4, 7}}, 3, 0);
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].left == 40);
    CHECK(bars[0].right == 70);
    CHECK(bars[0].top == 10);  // the Line's bottom
    CHECK(bars[0].bottom == 13);
    // The default gap is 0 too; a gap moves the bar down by exactly that much.
    CHECK(track.Bars(HeardWord{7, 0, {4, 7}}, 3)[0].top == 10);
    CHECK(track.Bars(HeardWord{7, 0, {4, 7}}, 3, 1)[0].top == 11);

    // The second Block of Reading 8.
    const std::vector<Rect> second = track.Words(HeardWord{8, 1, {3, 5}});
    REQUIRE(second.size() == 1);
    CHECK(second[0].top == 200);

    CHECK(track.Bars(std::nullopt, 3).empty());
    CHECK(track.Bars(HeardWord{9, 0, {0, 3}}, 3).empty());   // another Session's Reading
    CHECK(track.Bars(HeardWord{7, 5, {0, 3}}, 3).empty());   // no such Block
    track.Clear();
    CHECK(track.Empty());
}

TEST_CASE("Read-along: AnyActive is true while one of the track's Readings is still in the queue") {
    ReadAlongTrack track;
    const std::function<bool(uint64_t)> only8 = [](uint64_t id) { return id == 8; };
    CHECK_FALSE(track.AnyActive(only8));  // an empty track has nothing to keep a layer for
    track.Add(7, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two")});
    track.Add(8, {BlockOf({Line(R(0, 100, 70, 110), "xx yy")}, "xx yy")});
    CHECK(track.AnyActive(only8));
    CHECK(track.AnyActive([](uint64_t id) { return id == 7; }));
    CHECK_FALSE(track.AnyActive([](uint64_t) { return false; }));
    // An id the track does not know does not keep it alive (another Session's Reading).
    CHECK_FALSE(track.AnyActive([](uint64_t id) { return id == 9; }));
    CHECK_FALSE(track.AnyActive(nullptr));  // nobody to ask: finished
}

TEST_CASE("Read-along: ClipToArea keeps a bar on the screen and drops one that is off it") {
    const Rect in = ClipToArea(R(10, 20, 50, 23), 100, 100);
    CHECK(in.left == 10);
    CHECK(in.right == 50);
    const Rect edge = ClipToArea(R(-5, 95, 120, 103), 100, 100);
    CHECK(edge.left == 0);
    CHECK(edge.top == 95);
    CHECK(edge.right == 100);
    CHECK(edge.bottom == 100);
    const Rect gone = ClipToArea(R(10, 100, 50, 104), 100, 100);  // below the last row
    CHECK(gone.right <= gone.left);
    CHECK(gone.bottom <= gone.top);
}

TEST_CASE("Read-along: SameRects compares sizes and every edge") {
    const std::vector<Rect> a = {R(1, 2, 3, 4), R(5, 6, 7, 8)};
    CHECK(SameRects(a, a));
    CHECK(SameRects({}, {}));
    CHECK_FALSE(SameRects(a, {R(1, 2, 3, 4)}));
    CHECK_FALSE(SameRects(a, {R(1, 2, 3, 4), R(5, 6, 7, 9)}));
    CHECK_FALSE(SameRects({R(1, 2, 3, 4)}, {R(2, 2, 3, 4)}));
}

// =============================================================== more edges ===
// The cases below pin the edges of the rules above: every punctuation phoneme, the odd timings a chunk may
// carry, the mirror of SpeakStretches, the sentence rules for the marks a real text holds, and the
// degenerate Rects and Lines.

TEST_CASE("Read-along: every punctuation phoneme is silent for the read-along; a Cyrillic letter is spoken") {
    // The list in IsSpokenPhoneme: ASCII punctuation, then the dashes, the ellipsis, the curly quotes and
    // the guillemets that espeak-ng may put in a phoneme string.
    const char32_t punctuation[] = {U',',   U'.',   U';',   U':',   U'!',   U'?',   U'-',
                                    U'"',   U'\'',  U'(',   U')',   0x2013, 0x2014, 0x2026,
                                    0x2018, 0x2019, 0x201C, 0x201D, 0x00AB, 0x00BB};
    for (char32_t p : punctuation) {
        INFO("phoneme U+" << static_cast<unsigned>(p));
        CHECK_FALSE(IsSpokenPhoneme(p));
    }
    CHECK(IsSpokenPhoneme(0x0430));  // Cyrillic small a: a sound, not a mark
}

TEST_CASE("Read-along: CutIntoWords gives nothing for a sample rate that is not positive") {
    SpeechChunk c = MakeChunk({2, 2});
    REQUIRE_FALSE(c.samples.empty());
    c.sampleRate = 0;
    CHECK(CutIntoWords(c).empty());
    c.sampleRate = -22050;
    CHECK(CutIntoWords(c).empty());
}

TEST_CASE("Read-along: CutIntoWords ignores a space that lies at or past the end of the audio, or before its start") {
    SpeechChunk c;
    c.sampleRate = 100;
    c.samples.assign(40, 0.f);
    // A space exactly at the end, one beyond it and one with a negative start: none of them cuts.
    c.phonemes = {{U'a', 0, 10}, {U' ', 40, 5}, {U' ', 90, 5}, {U' ', -3, 5}};
    const std::vector<WordPiece> p = CutIntoWords(c);
    REQUIRE(p.size() == 1);
    CHECK(p[0].begin == 0);
    CHECK(p[0].end == 40);
    CHECK(p[0].phonemes == 1);
}

TEST_CASE("Read-along: a spoken phoneme with a negative start counts in the first piece") {
    SpeechChunk c;
    c.sampleRate = 100;
    c.samples.assign(40, 0.f);
    c.phonemes = {{U'a', -5, 10}, {U' ', 20, 5}, {U'b', 20, 10}};
    const std::vector<WordPiece> p = CutIntoWords(c);
    REQUIRE(p.size() == 2);
    CHECK(p[0].phonemes == 1);  // the 'a' that "started" before the audio
    CHECK(p[1].phonemes == 1);
}

TEST_CASE("Read-along: SpokenStretchRanges mirrors the non-blank stretches of SplitIntoStretches") {
    // Runs of two or more words of one script are stretches of their own: Latin, Cyrillic, Latin.
    const std::string text =
        "one two \xD0\xBD\xD0\xB5 \xD0\xB4\xD0\xB0 five six";  // "one two <ne> <da> five six" in Cyrillic
    const std::vector<TextRange> ranges = SpokenStretchRanges(text);
    REQUIRE(ranges.size() == 3);
    CHECK(ranges[0].start == 0);
    CHECK(ranges[0].end == ranges[1].start);
    CHECK(ranges[1].end == ranges[2].start);
    CHECK(ranges[2].end == text.size());

    // The test computes the offsets itself, from the split the speech uses, so a change in either side shows.
    std::vector<size_t> starts;
    size_t offset = 0;
    for (const Stretch& s : SplitIntoStretches(text)) {
        if (!Trim(s.text).empty()) starts.push_back(offset);
        offset += s.text.size();
    }
    REQUIRE(starts.size() == ranges.size());
    for (size_t i = 0; i < starts.size(); ++i) CHECK(ranges[i].start == starts[i]);
    CHECK(ranges[1].start == std::string("one two ").size());
}

TEST_CASE("Read-along: a text of only spaces has no stretch to speak") {
    CHECK(SpokenStretchRanges("    ").empty());
    CHECK(SpokenStretchRanges(" \t \n").empty());
}

TEST_CASE("Read-along: an ellipsis glued to a word, or four dots, is a pause inside the sentence") {
    const std::string glued = "Wait\xE2\x80\xA6 what now.";  // Wait<U+2026> what now.
    CHECK(Cuts(glued, SpokenWordRanges(glued, 0, {1, 1, 1})) == Words{"Wait", "what", "now"});
    const std::string dots = "Wait.... what now.";
    CHECK(Cuts(dots, SpokenWordRanges(dots, 0, {1, 1, 1})) == Words{"Wait", "what", "now"});
}

TEST_CASE("Read-along: a full stop inside curly quotes ends the sentence") {
    // He said <U+201C>stop.<U+201D> then left.
    const std::string text = "He said \xE2\x80\x9Cstop.\xE2\x80\x9D then left.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1, 1})) == Words{"He", "said", "stop"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"then", "left"});
}

TEST_CASE("Read-along: CR LF after a full stop ends the sentence; a tab separates words") {
    const std::string text = "Line one.\r\nline two.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1, 1})) == Words{"Line", "one"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"line", "two"});

    const std::string tabbed = "alpha\tbeta gamma.";
    CHECK(Cuts(tabbed, SpokenWordRanges(tabbed, 0, {1, 1, 1})) == Words{"alpha", "beta", "gamma"});
}

TEST_CASE("Read-along: a Latin-1 or Cyrillic lower-case word after a full stop does not end the sentence") {
    // "Mr." before a lower-case e-acute (U+00E9): one sentence of four words.
    const std::string latin1 = "See Mr. \xC3\xA9"
                               "mile now.";
    CHECK(Cuts(latin1, SpokenWordRanges(latin1, 0, {1, 1, 1, 1})) ==
          Words{"See", "Mr",
                "\xC3\xA9"
                "mile",
                "now"});

    const std::string da = "\xD0\x94\xD0\xB0";       // capital De, small a
    const std::string daLower = "\xD0\xB4\xD0\xB0";  // all small
    const std::string ne = "\xD0\xBD\xD0\xB5";       // small en, small e
    const std::string neCap = "\xD0\x9D\xD0\xB5";    // capital En, small e
    // "Da ne. da ne." (Cyrillic): the full stop is followed by a lower-case Cyrillic word, so it goes on.
    const std::string lower = da + " " + ne + ". " + daLower + " " + ne + ".";
    CHECK(Cuts(lower, SpokenWordRanges(lower, 0, {1, 1, 1, 1})) == Words{da, ne, daLower, ne});
    // "Da ne. Ne da." (Cyrillic): a capital follows, so the sentence ends there.
    const std::string capital = da + " " + ne + ". " + neCap + " " + daLower + ".";
    CHECK(Cuts(capital, SpokenWordRanges(capital, 0, {1, 1})) == Words{da, ne});
    CHECK(Cuts(capital, SpokenWordRanges(capital, 1, {1, 1})) == Words{neCap, daLower});
}

TEST_CASE("Read-along: a run of marks with no word is not a sentence of its own") {
    const std::string text = "One. !!! Two three.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1})) == Words{"One"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"Two", "three"});
}

TEST_CASE("Read-along: ?! ends a sentence, also inside a bracket") {
    const std::string text = "Wait?! then go.";
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1})) == Words{"Wait"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"then", "go"});

    const std::string bracket = "(Why?) then go.";
    CHECK(Cuts(bracket, SpokenWordRanges(bracket, 0, {1})) == Words{"Why"});
    CHECK(Cuts(bracket, SpokenWordRanges(bracket, 1, {1, 1})) == Words{"then", "go"});
}

TEST_CASE("Read-along: three spoken words on one text word all land on it") {
    const std::string text = "USA";  // said "U S A"
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {1, 1, 1});
    CHECK(Cuts(text, r) == Words{"USA", "USA", "USA"});
}

TEST_CASE("Read-along: phoneme counts of zero or less are taken as one in the proportional mapping") {
    const std::string text = "alpha beta gamma delta";
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {0, -3, 2});  // 3 spoken, 4 text words
    REQUIRE(r.size() == 3);
    size_t last = 0;
    for (const TextRange& x : r) {
        CHECK_FALSE(x.Empty());
        CHECK(x.end <= text.size());
        CHECK(x.start >= last);  // never back
        last = x.start;
    }
}

TEST_CASE("Read-along: a range that is only the space between two Lines has no box") {
    const TextBlock b = BlockOf({Line(R(0, 0, 30, 10), "a b"), Line(R(0, 20, 30, 30), "c d")}, "a b c d");
    CHECK(WordBoxes(b, {3, 4}).empty());
}

TEST_CASE("Read-along: a range over three Lines has three boxes, the middle one the whole Line") {
    const TextBlock b = BlockOf({Line(R(0, 0, 50, 10), "aa bb"), Line(R(0, 20, 50, 30), "cc dd"),
                                 Line(R(0, 40, 50, 50), "ee ff")},
                                "aa bb cc dd ee ff");
    const std::vector<Rect> w = WordBoxes(b, {3, 14});  // "bb cc dd ee"
    REQUIRE(w.size() == 3);
    CHECK(w[0].top == 0);
    CHECK(w[0].left == 30);  // 3 of 5 characters in
    CHECK(w[0].right == 50);
    CHECK(w[1].top == 20);
    CHECK(w[1].left == 0);  // the whole Line
    CHECK(w[1].right == 50);
    CHECK(w[2].top == 40);
    CHECK(w[2].left == 0);
    CHECK(w[2].right == 20);  // 2 of 5 characters
}

TEST_CASE("Read-along: a Line that ends in a hyphen but is joined with a space keeps its hyphen") {
    // layout.cpp keeps the '-' and puts a space when the next Line starts in upper case ("self-" "Esteem").
    const TextBlock b = BlockOf({Line(R(0, 0, 50, 10), "self-"), Line(R(0, 20, 60, 30), "Esteem")}, "self- Esteem");
    const std::vector<Rect> first = WordBoxes(b, {0, 5});
    REQUIRE(first.size() == 1);
    CHECK(first[0].top == 0);
    CHECK(first[0].left == 0);
    CHECK(first[0].right == 50);
    const std::vector<Rect> second = WordBoxes(b, {6, 12});
    REQUIRE(second.size() == 1);
    CHECK(second[0].top == 20);
    CHECK(second[0].left == 0);
    CHECK(second[0].right == 60);
}

TEST_CASE("Read-along: a last Line that ends in a hyphen the text lacks gives no box") {
    // Only a Line that is not the last may lose its '-' to a join, so these Lines do not make up the text.
    const TextBlock b = BlockOf({Line(R(0, 0, 50, 10), "word-")}, "word");
    CHECK(WordBoxes(b, {0, 4}).empty());
}

TEST_CASE("Read-along: a Line of no width gives no box") {
    const TextBlock b = BlockOf({Line(R(10, 0, 10, 10), "abc")}, "abc");
    CHECK(WordBoxes(b, {0, 3}).empty());
}

TEST_CASE("Read-along: one character on a Line narrower than its text is still a pixel wide") {
    const TextBlock b = BlockOf({Line(R(0, 0, 5, 10), "abcdefghij")}, "abcdefghij");  // 10 characters in 5 px
    const std::vector<Rect> w = WordBoxes(b, {2, 3});
    REQUIRE(w.size() == 1);
    CHECK(w[0].right - w[0].left >= 1);
    CHECK(w[0].left >= 0);
    CHECK(w[0].right <= 5);  // and inside the Line
}

TEST_CASE("Read-along: a thickness below one is one pixel") {
    const Rect bar = UnderlineBar(R(0, 0, 5, 5), -4);
    CHECK(bar.bottom - bar.top == 1);
    const Rect gapped = UnderlineBar(R(0, 0, 5, 5), -4, 2);
    CHECK(gapped.top == 7);
    CHECK(gapped.bottom == 8);
}

TEST_CASE("Read-along: ClipToArea gives an empty Rect for one wholly outside, or for an empty area") {
    auto isEmpty = [](const Rect& r) { return r.left == 0 && r.top == 0 && r.right == 0 && r.bottom == 0; };
    CHECK(isEmpty(ClipToArea(R(-20, 10, -5, 20), 100, 100)));  // wholly to the left
    CHECK(isEmpty(ClipToArea(R(10, -20, 50, -5), 100, 100)));  // wholly above
    CHECK(isEmpty(ClipToArea(R(10, 10, 50, 50), 0, 100)));     // an area of no width
    CHECK(isEmpty(ClipToArea(R(10, 10, 50, 50), 100, 0)));     // an area of no height
    const Rect same = ClipToArea(R(0, 0, 100, 80), 100, 80);   // the area itself stays as it is
    CHECK(same.left == 0);
    CHECK(same.top == 0);
    CHECK(same.right == 100);
    CHECK(same.bottom == 80);
}

TEST_CASE("Read-along: SameRects sees a change of one edge, and a change of order") {
    CHECK_FALSE(SameRects({R(1, 2, 3, 4)}, {R(1, 3, 3, 4)}));  // top only
    CHECK_FALSE(SameRects({R(1, 2, 3, 4)}, {R(1, 2, 3, 5)}));  // bottom only
    CHECK_FALSE(SameRects({R(1, 2, 3, 4)}, {R(1, 2, 4, 4)}));  // right only
    CHECK_FALSE(SameRects({R(1, 2, 3, 4), R(5, 6, 7, 8)}, {R(5, 6, 7, 8), R(1, 2, 3, 4)}));  // same Rects, other order
}

TEST_CASE("Read-along: adding a Reading again replaces its Blocks") {
    ReadAlongTrack track;
    track.Add(7, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two"),
                  BlockOf({Line(R(0, 50, 70, 60), "three four")}, "three four")});
    REQUIRE(track.Words(HeardWord{7, 1, {0, 5}}).size() == 1);  // Block 1 exists
    track.Add(7, {BlockOf({Line(R(0, 100, 70, 110), "xx yy")}, "xx yy")});
    const std::vector<Rect> w = track.Words(HeardWord{7, 0, {0, 2}});
    REQUIRE(w.size() == 1);
    CHECK(w[0].top == 100);                               // the new Block 0
    CHECK(track.Words(HeardWord{7, 1, {0, 5}}).empty());  // the old Block 1 is gone
}

TEST_CASE("Read-along: a hyphen-joined word has two bars, each under its own Line") {
    ReadAlongTrack track;
    // "an inter-" + "national word" = "an international word" (see the hyphen test above).
    track.Add(3, {BlockOf({Line(R(0, 0, 90, 10), "an inter-"), Line(R(0, 20, 130, 30), "national word")},
                          "an international word")});
    const std::vector<Rect> bars = track.Bars(HeardWord{3, 0, {3, 16}}, 3, 1);
    REQUIRE(bars.size() == 2);
    CHECK(bars[0].left == 30);
    CHECK(bars[0].right == 90);
    CHECK(bars[0].top == 11);  // under the first Line (bottom 10 + gap 1)
    CHECK(bars[0].bottom == 14);
    CHECK(bars[1].left == 0);
    CHECK(bars[1].right == 80);
    CHECK(bars[1].top == 31);  // under the second Line (bottom 30 + gap 1)
    CHECK(bars[1].bottom == 34);
}

TEST_CASE("Read-along: an empty range has no boxes in the track") {
    ReadAlongTrack track;
    track.Add(7, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two")});
    CHECK(track.Words(HeardWord{7, 0, {3, 3}}).empty());
    CHECK(track.Words(HeardWord{7, 0, TextRange{}}).empty());
    CHECK(track.Bars(HeardWord{7, 0, {3, 3}}, 3, 1).empty());
}

TEST_CASE("Read-along: AnyActive is false after Clear, whatever the queue says") {
    ReadAlongTrack track;
    track.Add(7, {BlockOf({Line(R(0, 0, 70, 10), "one two")}, "one two")});
    const std::function<bool(uint64_t)> always = [](uint64_t) { return true; };
    CHECK(track.AnyActive(always));
    track.Clear();
    CHECK_FALSE(track.AnyActive(always));
}


TEST_CASE("Read-along: more spoken words than text words give every text word a turn") {
    // "In 2024 the old mill" as Piper says it: in, two, thousand, and, twenty, four, the, old, mill. A walk by
    // the middle of each spoken word skipped "old"; the grouping puts one text word on each run of words.
    const std::string text = "In 2024 the old mill";
    const std::vector<TextRange> r = SpokenWordRanges(text, 0, {2, 4, 8, 2, 7, 4, 3, 5, 4});
    REQUIRE(r.size() == 9);
    // Which spoken word starts the next text word is an estimate from the phoneme and letter shares, so only the
    // order and the coverage are exact: every text word is marked, in order, the first one first and the last one last.
    Words distinct;
    for (const std::string& w : Cuts(text, r)) {
        if (distinct.empty() || distinct.back() != w) distinct.push_back(w);
    }
    CHECK(distinct == Words{"In", "2024", "the", "old", "mill"});
}

TEST_CASE("Read-along: with more spoken words than text words no text word is skipped and none goes back") {
    const std::string text = "A short text, with 12 words in it; and some more.";
    const size_t textWords = 11;  // A short text with 12 words in it and some more: one sentence ("it;" does not end one)
    for (size_t spoken = textWords + 1; spoken < 40; ++spoken) {
        // Uneven phoneme counts: one long spoken word (a number), the others short.
        std::vector<int> counts(spoken, 3);
        counts[spoken / 2] = 9;
        const std::vector<TextRange> r = SpokenWordRanges(text, 0, counts);
        REQUIRE(r.size() == spoken);
        // The distinct text words in the order they are marked: each starts where the one before it ended or later,
        // and, counted by their start, they are all the words of the sentence.
        std::vector<TextRange> distinct;
        for (const TextRange& x : r) {
            if (distinct.empty() || x.start != distinct.back().start) distinct.push_back(x);
        }
        CHECK(distinct.size() == textWords);  // no text word was skipped
        for (size_t i = 1; i < distinct.size(); ++i) CHECK(distinct[i].start > distinct[i - 1].start);  // none went back
        CHECK(Cut(text, r.front()) == "A");
        CHECK(Cut(text, r.back()) == "more");
    }
}

TEST_CASE("Read-along: four spoken words on three text words skip none") {
    const std::string text = "alpha beta gamma";
    const Words heard = Cuts(text, SpokenWordRanges(text, 0, {4, 4, 2, 2}));
    REQUIRE(heard.size() == 4);
    CHECK(heard.front() == "alpha");
    CHECK(heard.back() == "gamma");
    // "beta" is heard: the second text word has a spoken word of its own.
    CHECK(std::count(heard.begin(), heard.end(), std::string("beta")) >= 1);
}

// ------------------------------------------------------- the click-through layer's rules ---

namespace {

// Facts for which the layer is wanted; a test turns one off.
ReadAlongLayerFacts AllTrue() {
    ReadAlongLayerFacts f;
    f.styleReadAlong = f.settingReadAlong = f.lexiaOn = f.haveWindow = f.trackHasReadings = f.anyReadingActive = true;
    f.quitting = false;
    return f;
}

}  // namespace

TEST_CASE("Read-along layer: it is wanted only when every condition holds") {
    CHECK(ReadAlongLayerWanted(AllTrue()));
    struct Case {
        const char* what;
        void (*spoil)(ReadAlongLayerFacts&);
    };
    const Case cases[] = {
        {"the Session was run without Read-along", [](ReadAlongLayerFacts& f) { f.styleReadAlong = false; }},
        {"the setting was turned off during the Session", [](ReadAlongLayerFacts& f) { f.settingReadAlong = false; }},
        {"Lexia is Off", [](ReadAlongLayerFacts& f) { f.lexiaOn = false; }},
        {"Lexia is quitting", [](ReadAlongLayerFacts& f) { f.quitting = true; }},
        {"there is no window to post to", [](ReadAlongLayerFacts& f) { f.haveWindow = false; }},
        {"nothing was Picked", [](ReadAlongLayerFacts& f) { f.trackHasReadings = false; }},
        {"every Reading is done", [](ReadAlongLayerFacts& f) { f.anyReadingActive = false; }},
    };
    for (const Case& c : cases) {
        ReadAlongLayerFacts f = AllTrue();
        c.spoil(f);
        CHECK_MESSAGE(!ReadAlongLayerWanted(f), c.what);
    }
    CHECK_FALSE(ReadAlongLayerWanted(ReadAlongLayerFacts{}));  // no fact true: not wanted
}

TEST_CASE("Read-along layer: which events end it") {
    // These end it whatever the flag says.
    for (const ReadAlongLayerEvent e : {ReadAlongLayerEvent::Stop, ReadAlongLayerEvent::Quit, ReadAlongLayerEvent::RunEnded,
                                        ReadAlongLayerEvent::NewSession, ReadAlongLayerEvent::NewLayer}) {
        CHECK(EndsReadAlongLayer(e));
        CHECK(EndsReadAlongLayer(e, true));
        CHECK(EndsReadAlongLayer(e, false));
    }
    // Turning Lexia Off ends it; turning it On does not.
    CHECK(EndsReadAlongLayer(ReadAlongLayerEvent::LexiaSwitched, false));
    CHECK_FALSE(EndsReadAlongLayer(ReadAlongLayerEvent::LexiaSwitched, true));
    // The Read-along setting turned off (the page or settings.json) ends it; turned on does not.
    CHECK(EndsReadAlongLayer(ReadAlongLayerEvent::ReadAlongSwitched, false));
    CHECK_FALSE(EndsReadAlongLayer(ReadAlongLayerEvent::ReadAlongSwitched, true));
}

TEST_CASE("Read-along layer: a posted 'ended' message deletes only a layer that has ended") {
    CHECK(KeepLayerOnEndedMessage(false));  // a newer layer that runs: the stale message leaves it alone
    CHECK_FALSE(KeepLayerOnEndedMessage(true));
}

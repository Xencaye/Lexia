// Unit tests for read_along.h: cutting a chunk into words, mapping spoken words to text words, the
// box of a word on the screen, the underline bar and the track of Readings. Cyrillic is written as
// UTF-8 escapes so the test does not depend on how the editor saved it.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "read_along.h"

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
    const std::string text = "Wait ... what - now?";
    // "..." ends the sentence, then "what - now?" is the second one; the lone "-" is not a word.
    CHECK(Cuts(text, SpokenWordRanges(text, 0, {1})) == Words{"Wait"});
    CHECK(Cuts(text, SpokenWordRanges(text, 1, {1, 1})) == Words{"what", "now"});
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

    const std::vector<Rect> bars = track.Bars(HeardWord{7, 0, {4, 7}}, 3, 1);
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].left == 40);
    CHECK(bars[0].right == 70);
    CHECK(bars[0].top == 11);
    CHECK(bars[0].bottom == 14);

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

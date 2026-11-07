#include "read_along.h"

#include <algorithm>
#include <cctype>

#include "codepoint.h"
#include "stretch.h"

namespace {

// ------------------------------------------------------------------- text helpers ---

bool IsAsciiSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

// Is this code point part of a word (a letter or a digit, of any script)? Punctuation and symbols
// are not, so a range can be trimmed to the word without its quotes, commas and full stops. The
// non-ASCII test is by block, not a full Unicode table: Latin-1 symbols (up to U+00BF, and x and
// the division sign), General Punctuation to Miscellaneous Symbols and Arrows (U+2000..U+2BFF),
// CJK punctuation and U+FFFD are not word characters; everything else is.
bool IsWordChar(char32_t cp) {
    if (cp < 0x80) return std::isalnum(static_cast<int>(cp)) != 0;
    if (IsLatinLetter(cp) || IsCyrillicLetter(cp)) return true;
    if (cp < 0xC0 || cp == 0xD7 || cp == 0xF7) return false;
    if (cp >= 0x2000 && cp <= 0x2BFF) return false;
    if (cp >= 0x3000 && cp <= 0x303F) return false;
    return cp != 0xFFFD;
}

// A closing quote or bracket may follow the full stop that ends a sentence: He said "stop."
bool IsCloser(char32_t cp) {
    switch (cp) {
    case U'"': case U'\'': case U')': case U']': case U'}':
    case 0x2019: case 0x201D: case 0xBB:
        return true;
    default:
        return false;
    }
}

bool IsSentenceEnd(char32_t cp) { return cp == U'.' || cp == U'!' || cp == U'?' || cp == 0x2026; }

// How long a text word is when spoken, in letters: a letter is about one phoneme, a digit about
// three ("2024" is "twenty twenty-four": 15 phonemes for 4 characters). This only has to put the
// shares of a sentence's words in roughly the right proportion for the count-mismatch case.
size_t SpokenWeight(const std::string& text, TextRange r) {
    size_t weight = 0;
    for (size_t i = r.start; i < r.end && i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) == 0x80) continue;  // continues a character
        weight += (c >= '0' && c <= '9') ? 3 : 1;
    }
    return std::max<size_t>(1, weight);
}

// The number of characters (code points) in text[from, to): every byte that does not continue one.
size_t CodePoints(const std::string& text, size_t from, size_t to) {
    size_t n = 0;
    for (size_t i = from; i < to && i < text.size(); ++i) n += (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80;
    return n;
}

// One word-like run of the text between spaces (it may be only punctuation).
struct Token {
    size_t start = 0;
    size_t end = 0;
};

std::vector<Token> Tokens(const std::string& text) {
    std::vector<Token> out;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && IsAsciiSpace(text[i])) ++i;
        const size_t start = i;
        while (i < text.size() && !IsAsciiSpace(text[i])) ++i;
        if (i > start) out.push_back({start, i});
    }
    return out;
}

// The token without leading and trailing punctuation; empty when it has no letter or digit.
TextRange TrimmedWord(const std::string& text, const Token& t) {
    size_t first = std::string::npos, last = 0;
    for (size_t i = t.start; i < t.end;) {
        const size_t at = i;
        const char32_t cp = NextCodePoint(text, i);
        if (!IsWordChar(cp)) continue;
        if (first == std::string::npos) first = at;
        last = i;  // (a sequence cut short ends at the space after the token, never beyond it)
    }
    if (first == std::string::npos) return {};
    return {first, last};
}

// Does this token end a sentence? (A full stop, ! ? or an ellipsis, then maybe closing quotes.)
bool EndsSentence(const std::string& text, const Token& t) {
    std::vector<char32_t> cps;
    for (size_t i = t.start; i < t.end;) cps.push_back(NextCodePoint(text, i));
    while (!cps.empty() && IsCloser(cps.back())) cps.pop_back();
    return !cps.empty() && IsSentenceEnd(cps.back());
}

// The text words of each sentence of `text`. A sentence with no word in it ("...") is left out, as
// Piper makes no chunk to speak for it.
std::vector<std::vector<TextRange>> SentenceWords(const std::string& text) {
    std::vector<std::vector<TextRange>> sentences;
    std::vector<TextRange> current;
    for (const Token& t : Tokens(text)) {
        const TextRange word = TrimmedWord(text, t);
        if (!word.Empty()) current.push_back(word);
        if (EndsSentence(text, t) && !current.empty()) {
            sentences.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) sentences.push_back(std::move(current));
    return sentences;
}

}  // namespace

// ---------------------------------------------------------------------- the sound ---

bool IsSpokenPhoneme(char32_t p) {
    switch (p) {
    case 0: case U'^': case U'$': case U' ':
    case U',': case U'.': case U';': case U':': case U'!': case U'?': case U'-':
    case U'"': case U'\'': case U'(': case U')':
    case 0x2013: case 0x2014: case 0x2026:  // en dash, em dash, ellipsis
    case 0x2018: case 0x2019: case 0x201C: case 0x201D: case 0xAB: case 0xBB:
        return false;
    default:
        return true;
    }
}

std::vector<WordPiece> CutIntoWords(const SpeechChunk& chunk) {
    const size_t n = chunk.samples.size();
    if (n == 0 || chunk.sampleRate <= 0) return {};
    // A word ends where the next ' ' phoneme starts. A ' ' at the very start, a repeated one and one
    // beyond the audio cut nothing.
    std::vector<size_t> cuts{0};
    for (const PhonemeTiming& p : chunk.phonemes) {
        if (p.phoneme != U' ' || p.startSample <= 0) continue;
        const size_t at = static_cast<size_t>(p.startSample);
        if (at > cuts.back() && at < n) cuts.push_back(at);
    }
    cuts.push_back(n);
    std::vector<WordPiece> pieces;
    for (size_t i = 0; i + 1 < cuts.size(); ++i) pieces.push_back({cuts[i], cuts[i + 1], 0});
    // Each phoneme belongs to the piece its start falls in; the phonemes come in order, so one pass.
    size_t k = 0;
    for (const PhonemeTiming& p : chunk.phonemes) {
        const size_t at = p.startSample < 0 ? 0 : static_cast<size_t>(p.startSample);
        while (k + 1 < pieces.size() && at >= pieces[k].end) ++k;
        if (IsSpokenPhoneme(p.phoneme)) ++pieces[k].phonemes;
    }
    return pieces;
}

// Mirrors SpeakStretches (tts/stretch_speech.cpp): the same split, the same skipping of stretches
// that are only spaces, so the offsets are the ones SpeechChunk::textOffset carries.
std::vector<TextRange> SpokenStretchRanges(const std::string& blockText) {
    std::vector<size_t> starts;
    size_t offset = 0;
    for (const Stretch& s : SplitIntoStretches(blockText)) {
        if (!Trim(s.text).empty()) starts.push_back(offset);
        offset += s.text.size();
    }
    std::vector<TextRange> out;
    for (size_t i = 0; i < starts.size(); ++i) {
        out.push_back({starts[i], i + 1 < starts.size() ? starts[i + 1] : blockText.size()});
    }
    return out;
}

// ------------------------------------------------------------- sound -> text words ---

std::vector<TextRange> SpokenWordRanges(const std::string& stretchText, size_t sentenceIndex,
                                        const std::vector<int>& phonemesPerWord) {
    std::vector<TextRange> out(phonemesPerWord.size());
    const std::vector<std::vector<TextRange>> sentences = SentenceWords(stretchText);
    if (out.empty() || sentences.empty()) return out;
    const std::vector<TextRange>& words = sentences[std::min(sentenceIndex, sentences.size() - 1)];

    // The usual case: every spoken word is a text word.
    if (words.size() == out.size()) return words;

    // Else by proportion: a spoken word lands on the text word that holds the middle of its share of
    // the sentence's phonemes, measured against the share of the sentence's letters. Monotonic, so
    // the highlight never goes back; several spoken words may share one text word ("2024" said as
    // "twenty twenty four"), and a text word may be skipped.
    double totalPhonemes = 0;
    for (int p : phonemesPerWord) totalPhonemes += std::max(p, 1);
    std::vector<double> letters;  // the spoken length of each text word, in letters (at least 1)
    double totalLetters = 0;
    for (const TextRange& w : words) {
        letters.push_back(static_cast<double>(SpokenWeight(stretchText, w)));
        totalLetters += letters.back();
    }
    double before = 0;  // phonemes before the spoken word i
    size_t j = 0;       // the text word being tried
    double lettersBefore = 0;  // letters before text word j
    for (size_t i = 0; i < out.size(); ++i) {
        const double p = std::max(phonemesPerWord[i], 1);
        const double middle = (before + p / 2) / totalPhonemes;
        before += p;
        // Move on while the middle lies beyond the end of text word j.
        while (j + 1 < words.size() && (lettersBefore + letters[j]) / totalLetters <= middle) {
            lettersBefore += letters[j];
            ++j;
        }
        out[i] = words[j];
    }
    return out;
}

// ------------------------------------------------------------- text -> the screen ---

namespace {

// Where one Line's text sits in the Block's text.
struct Segment {
    const TextLine* line = nullptr;
    size_t start = 0;       // first byte in block.text
    size_t end = 0;         // one past its last byte in block.text
    bool hyphenCut = false; // its trailing '-' was dropped when the next Line was joined on (layout.cpp)
};

// Finds where each Line's text is in `text`, the way GroupBlocks joined them: a space between two
// Lines, or nothing when the first ended in '-' and the second continues the word (the '-' is
// dropped from the Block's text). This follows the text it is given and does not repeat the
// rule for when a hyphen is joined, so it stays right if that rule changes. False if the Lines do
// not make up the text exactly.
bool LocateLines(const std::vector<TextLine>& lines, const std::string& text, std::vector<Segment>& out) {
    size_t pos = 0;  // where the next Line starts in `text`
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& t = lines[i].text;
        const bool last = i + 1 == lines.size();
        // The Line is in the text whole, or (not the last one) without its trailing '-', which
        // GroupBlocks drops when it joins the next Line straight on.
        size_t length = 0;
        bool cut = false;
        if (text.compare(pos, t.size(), t) == 0) {
            length = t.size();
        } else if (!last && !t.empty() && t.back() == '-' && text.compare(pos, t.size() - 1, t, 0, t.size() - 1) == 0) {
            length = t.size() - 1;
            cut = true;
        } else {
            return false;
        }
        Segment s;
        s.line = &lines[i];
        s.start = pos;
        s.end = pos + length;
        s.hyphenCut = cut;
        out.push_back(s);
        pos += length;
        if (last) break;
        if (cut) continue;  // the next Line goes on at once
        if (pos >= text.size() || text[pos] != ' ') return false;
        ++pos;  // the space GroupBlocks put between the two Lines
    }
    return pos == text.size();
}

}  // namespace

std::vector<Rect> WordBoxes(const TextBlock& block, TextRange range) {
    std::vector<Rect> boxes;
    range.end = std::min(range.end, block.text.size());
    if (range.Empty()) return boxes;

    // A Block built without Lines is one Line.
    std::vector<TextLine> single;
    const std::vector<TextLine>* lines = &block.lines;
    if (block.lines.empty()) {
        TextLine only;
        only.box = block.box;
        only.text = block.text;
        single.push_back(std::move(only));
        lines = &single;
    }
    std::vector<Segment> segments;
    if (!LocateLines(*lines, block.text, segments)) return boxes;

    for (const Segment& s : segments) {
        const size_t a = std::max(range.start, s.start);
        const size_t b = std::min(range.end, s.end);
        if (a >= b) continue;
        const std::string& t = s.line->text;
        const size_t count = std::max<size_t>(1, CodePoints(t, 0, t.size()));
        const size_t from = CodePoints(t, 0, a - s.start);
        // A word that runs to a hyphen at the Line's end includes the hyphen's place.
        const size_t to = (s.hyphenCut && b == s.end) ? count : CodePoints(t, 0, b - s.start);
        const Rect& line = s.line->box;
        const long long width = static_cast<long long>(line.right) - line.left;
        Rect box = line;
        box.left = line.left + static_cast<int>(width * static_cast<long long>(from) / static_cast<long long>(count));
        box.right = line.left + static_cast<int>(width * static_cast<long long>(to) / static_cast<long long>(count));
        if (box.right <= box.left) box.right = std::min(line.right, box.left + 1);
        if (box.right > box.left) boxes.push_back(box);
    }
    return boxes;
}

Rect UnderlineBar(const Rect& wordBox, int thickness, int gap) {
    Rect bar;
    bar.left = wordBox.left;
    bar.right = wordBox.right;
    bar.top = wordBox.bottom + gap;
    bar.bottom = bar.top + std::max(thickness, 1);
    return bar;
}

// ------------------------------------------------------------------ the Readings ---

void ReadAlongTrack::Add(uint64_t readingId, std::vector<TextBlock> blocks) {
    if (readingId == 0) return;
    readings_[readingId] = std::move(blocks);
}

void ReadAlongTrack::Clear() { readings_.clear(); }

std::vector<Rect> ReadAlongTrack::Words(const std::optional<HeardWord>& heard) const {
    if (!heard) return {};
    const auto it = readings_.find(heard->readingId);
    if (it == readings_.end() || heard->block >= it->second.size()) return {};
    return WordBoxes(it->second[heard->block], heard->range);
}

std::vector<Rect> ReadAlongTrack::Bars(const std::optional<HeardWord>& heard, int thickness, int gap) const {
    std::vector<Rect> bars;
    for (const Rect& box : Words(heard)) bars.push_back(UnderlineBar(box, thickness, gap));
    return bars;
}

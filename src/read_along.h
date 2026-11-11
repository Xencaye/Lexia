#pragma once
// Read-along (G1): which word of the text is heard now, and where it is on the screen. Pure and
// Win32-free (UTF-8 in, plain Rects out), so the Overlay (G1) and the click-through layer that
// follows (G2) use the same logic, and the tests drive it with hand-made data.
//
// The chain, from the sound to the screen:
//   Piper chunk (one sentence, with phoneme timings)
//     -> CutIntoWords: the word pieces of its audio, and how many real phonemes each has
//     -> SpokenWordRanges: the byte range in the Block's text that each spoken word stands for
//     -> (the Reading queue keeps these with the audio it plays; HeardWord says which is audible)
//     -> WordBoxes: the box(es) of that range on the screen, one per Line it touches
//     -> UnderlineBar: the bar drawn under each box.
// OCR gives boxes for Lines, not for words, so a word's place inside its Line is an estimate: the
// share of the Line's characters before it (a proportional font makes it a little off).
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common.h"
#include "tts/speech_chunk.h"

// A byte range [start, end) of a Block's text. Empty (end <= start) = unknown.
struct TextRange {
    size_t start = 0;
    size_t end = 0;
    bool Empty() const { return end <= start; }
};

// The word that is audible now: which Reading (ReadingQueue::Add's id), which of its Blocks, and
// where in that Block's text.
struct HeardWord {
    uint64_t readingId = 0;
    size_t block = 0;
    TextRange range;
};

// Does this phoneme stand for a sound of a word? False for Piper's markers ('^' start of
// sentence, '$' end, ' ' between words, 0) and for punctuation phonemes (',' '.' ...), which carry
// no letters of the text.
bool IsSpokenPhoneme(char32_t phoneme);

// A word's piece of a chunk's audio: samples [begin, end), and how many spoken phonemes it holds
// (0 = silence or punctuation only: no word to show).
struct WordPiece {
    size_t begin = 0;
    size_t end = 0;
    int phonemes = 0;
};

// Cuts a chunk's audio into word pieces, as the Reading queue feeds it to the sink: a word ends
// where the next ' ' phoneme starts (the silence of the space belongs to the start of the next
// piece). A chunk with no samples gives none; one without timings is one piece with 0 phonemes.
std::vector<WordPiece> CutIntoWords(const SpeechChunk& chunk);

// The byte ranges, in a Block's text, of the stretches that SpeakStretches speaks (the ones with
// something to say; the same offsets as SpeechChunk::textOffset). Each range runs to the start of
// the next one, and the last to the end of the text.
std::vector<TextRange> SpokenStretchRanges(const std::string& blockText);

// Maps the spoken words of one chunk to the words of the text. `stretchText` is the text of the
// stretch the chunk belongs to; the chunk is its `sentenceIndex`-th sentence (0 based, counting
// only sentences that have a word in them; past the end = the last one); `phonemesPerWord` holds
// the spoken phonemes of each spoken word of the chunk, in order (each > 0).
//
// Sentences end where espeak-ng, and so Piper, ends them: at a word ending in ? or !, or in a full
// stop that is followed by a closing quote or bracket, by a line break, by the end of the text or by
// a word that does not start with a lower-case letter ("Mr. Smith" ends, "Mr. smith" and "e.g. a"
// do not; there is no list of abbreviations). An ellipsis ("..." or U+2026) is a pause, not an end.
// Details and the checks against Piper: EndsSentence in read_along.cpp. A text word is a run of non-space characters that has a letter or digit;
// leading and trailing punctuation is not part of its range. Spoken words are matched to text
// words 1:1 when the counts agree; otherwise by proportion (the share of the sentence's phonemes
// before a spoken word against the share of its letters before a text word; a digit counts as three
// letters), so a number or an
// abbreviation that is said as several words still lands on its own text word.
//
// Result: one range per spoken word, relative to `stretchText` (empty if there is no text word to
// map to). A range never lies outside the text, and always starts and ends on a character.
std::vector<TextRange> SpokenWordRanges(const std::string& stretchText, size_t sentenceIndex,
                                        const std::vector<int>& phonemesPerWord);

// The box on the screen of a byte range of `block.text`: one Rect per Line it touches (a word
// hyphenated at a Line end gives two). Empty if the range is empty or the Lines do not match the
// text (the Block was built some other way): then nothing is shown rather than something wrong.
// A Block with no Lines is taken as one Line with the Block's box.
std::vector<Rect> WordBoxes(const TextBlock& block, TextRange range);

// The bar under a word box: `thickness` px high, `gap` px below the box, as wide as the box.
// Keeping it on the Shot is the caller's job.
Rect UnderlineBar(const Rect& wordBox, int thickness, int gap = 0);

// `r` cut to the rectangle [0, w) x [0, h) (a Shot's or a layer's own coordinates: a word at the edge
// of the screen must not draw outside it). An empty Rect if nothing is left of it.
Rect ClipToArea(const Rect& r, int w, int h);

// Are the two lists the same Rects in the same order? The Overlay and the click-through layer redraw
// only when the bars changed.
bool SameRects(const std::vector<Rect>& a, const std::vector<Rect>& b);

// The Readings that were Picked, with their Blocks as the Pick found them (Shot coordinates), and
// the logic from "which word is heard" to "where are its bars". The Overlay keeps one for the length
// of a Session and hands it back when the Session ends; the click-through layer (G2, "keep the word
// mark") then keeps it until its Readings are done. Blocks are copied: they are small.
class ReadAlongTrack {
public:
    // Remembers the Blocks of a Reading (the Block index of a HeardWord is an index into `blocks`).
    // An id of 0 (the queue ignored the Reading) is not kept.
    void Add(uint64_t readingId, std::vector<TextBlock> blocks);
    void Clear();
    bool Knows(uint64_t readingId) const { return readings_.count(readingId) != 0; }
    bool Empty() const { return readings_.empty(); }

    // Is at least one of the Readings still going on? `isActive` says whether a Reading id is still in
    // the Reading queue (Speaker::HasReading): queued, being spoken or paused. False for an empty track
    // or no function. The click-through layer ends when this turns false.
    bool AnyActive(const std::function<bool(uint64_t)>& isActive) const;

    // The boxes of the heard word (nothing when no word is heard, or the Reading is not one of
    // this track's, or the range does not fit).
    std::vector<Rect> Words(const std::optional<HeardWord>& heard) const;
    // The bars under those boxes.
    std::vector<Rect> Bars(const std::optional<HeardWord>& heard, int thickness, int gap = 0) const;

private:
    std::map<uint64_t, std::vector<TextBlock>> readings_;
};

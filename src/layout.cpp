#include "layout.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iterator>

#include "codepoint.h"

// ----------------------------------------------------------- reading order ---
// Top-to-bottom in rows, left-to-right inside a row. Two boxes belong to the same row
// when they overlap vertically by at least half of the shorter one's height and do not
// share half of the narrower one's width (those are stacked, top one first). Sorting by
// the raw top edge is fragile: the detector's boxes jitter by a pixel or two, which can
// put a piece on the right *before* its neighbour on the left.
template <typename T>  // T needs a Rect member called `box`
void SortReadingOrder(std::vector<T>& v) {
    std::sort(v.begin(), v.end(), [](const T& a, const T& b) {
        if (a.box.top != b.box.top) return a.box.top < b.box.top;
        return a.box.left < b.box.left;
    });

    std::vector<T> out;
    out.reserve(v.size());
    std::vector<char> taken(v.size(), 0);
    for (size_t i = 0; i < v.size(); ++i) {
        if (taken[i]) continue;
        const Rect anchor = v[i].box;  // topmost item not yet placed starts a new row
        std::vector<size_t> row = {i};
        taken[i] = 1;
        for (size_t j = i + 1; j < v.size(); ++j) {
            if (taken[j]) continue;
            if (v[j].box.top >= anchor.bottom) break;  // sorted by top: nothing later overlaps
            const int overlap = std::min<int>(anchor.bottom, v[j].box.bottom) -
                                std::max<int>(anchor.top, v[j].box.top);
            const int minH = std::min<int>(anchor.bottom - anchor.top,
                                           v[j].box.bottom - v[j].box.top);
            if (minH > 0 && overlap * 2 >= minH) {
                // Two centred lines of a bubble can overlap vertically by more than half (tight
                // leading in big lettering) and the wider one may start further left; sorting them
                // by left edge would read the lower line first. Boxes that also share most of
                // their width are stacked, not side by side: the one with the higher top comes
                // first, so this candidate waits for a row of its own.
                bool stacked = false;
                for (size_t m : row) {
                    const Rect& r = v[m].box;
                    const int shared = std::min<int>(r.right, v[j].box.right) -
                                       std::max<int>(r.left, v[j].box.left);
                    const int narrower = std::min<int>(r.right - r.left,
                                                       v[j].box.right - v[j].box.left);
                    if (narrower > 0 && shared * 2 >= narrower) {
                        stacked = true;
                        break;
                    }
                }
                if (stacked) continue;
                row.push_back(j);
                taken[j] = 1;
            }
        }
        std::sort(row.begin(), row.end(), [&](size_t a, size_t b) {
            if (v[a].box.left != v[b].box.left) return v[a].box.left < v[b].box.left;
            return v[a].box.top < v[b].box.top;
        });
        for (size_t idx : row) out.push_back(std::move(v[idx]));
    }
    v = std::move(out);
}

template void SortReadingOrder<TextLine>(std::vector<TextLine>&);
template void SortReadingOrder<TextBlock>(std::vector<TextBlock>&);

// ----------------------------------------------- reading order of Blocks ---
// Recursive XY cut: the Blocks are split in two at the widest empty strip of the page that no
// Block crosses, and each half is ordered the same way (top or left half first). A strip is a
// candidate when it is at least kCutRow line heights high (a table reads row by row) or
// LayoutParams::columnGap line heights wide (a sidebar or a column is read down before moving
// across); a narrower vertical strip is only used when no horizontal one exists. Where nothing
// cuts, the row rule of SortReadingOrder decides. Strips are measured in line heights, so the
// rule does not depend on the font size or the scale of the Shot.
namespace {

const double kCutRow = 0.25;    // a horizontal strip this high between rows of Blocks cuts
const double kCutNarrow = 1.5;  // ...or this wide when there is no horizontal strip

struct Cut {
    double gap = 0;  // in pixels, 0 = none
    size_t at = 0;   // items[at..] go after the cut
};

// The start and end of a box along the axis that a cut crosses: a vertical cut (between columns)
// looks at left / right, a horizontal one (between rows) at top / bottom.
int Lo(const Rect& r, bool vertical) { return vertical ? r.left : r.top; }
int Hi(const Rect& r, bool vertical) { return vertical ? r.right : r.bottom; }

// Sorts by where each box starts along that axis.
void SortAlong(std::vector<TextBlock>& items, bool vertical) {
    std::sort(items.begin(), items.end(), [&](const TextBlock& a, const TextBlock& b) {
        return Lo(a.box, vertical) < Lo(b.box, vertical);
    });
}

// The widest strip at least `minGap` wide that no item crosses. `items` must be sorted along
// the axis (SortAlong) and not empty.
Cut WidestStrip(const std::vector<TextBlock>& items, bool vertical, double minGap) {
    Cut best;
    int reach = Hi(items[0].box, vertical);
    for (size_t i = 1; i < items.size(); ++i) {
        const double gap = Lo(items[i].box, vertical) - reach;
        if (gap >= minGap && gap > best.gap) best = {gap, i};
        reach = std::max(reach, Hi(items[i].box, vertical));
    }
    return best;
}

// Recursive XY cut: find the widest empty strip across the items (a gap between rows, or a wide gap
// between columns), split there, and order each side the same way; the first side is read first.
// When no strip is wide enough, the items are one group, read in rows. lh is the typical line
// height; the gaps are in line heights.
std::vector<TextBlock> XyCut(std::vector<TextBlock> items, double lh, double columnGap) {
    if (items.size() <= 1) return items;
    auto split = [&](size_t at) {
        std::vector<TextBlock> second(std::make_move_iterator(items.begin() + at),
                                      std::make_move_iterator(items.end()));
        items.resize(at);
        std::vector<TextBlock> out = XyCut(std::move(items), lh, columnGap);
        for (TextBlock& b : XyCut(std::move(second), lh, columnGap)) out.push_back(std::move(b));
        return out;
    };
    // The widest of the horizontal strips and the wide vertical ones.
    std::vector<TextBlock> byY = items;
    SortAlong(byY, false);
    SortAlong(items, true);
    const Cut row = WidestStrip(byY, false, kCutRow * lh);
    const Cut column = WidestStrip(items, true, columnGap * lh);
    if (row.gap > 0 && row.gap >= column.gap) {
        items = std::move(byY);
        return split(row.at);
    }
    if (column.gap > 0) return split(column.at);
    const Cut narrow = WidestStrip(items, true, kCutNarrow * lh);  // items are still sorted by x
    if (narrow.gap > 0) return split(narrow.at);
    SortReadingOrder(items);
    return items;
}

}  // namespace

// -------------------------------------------------- paragraph grouping ---
// Groups the detected pieces of text into blocks (paragraphs, headings, single
// names...) using spacing and alignment.

namespace {

// The number of characters (code points) in UTF-8 text: every byte that does not continue a character.
int CodePointCount(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;  // not a UTF-8 continuation byte
    return n;
}

// The upper median of `v`, which must not be empty.
int Median(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// The typical Line height of a Shot: the median over its Lines (not empty).
int MedianLineHeight(const std::vector<TextLine>& lines) {
    std::vector<int> heights;
    for (const TextLine& l : lines) heights.push_back(l.box.bottom - l.box.top);
    return Median(std::move(heights));
}

// Does the text start with a lower-case letter (Latin, Latin-1 or Cyrillic)? Then the sentence
// goes on from the Line above.
bool StartsLowercase(const std::string& s) {
    if (s.empty()) return false;
    const unsigned char a = static_cast<unsigned char>(s[0]);
    if (a >= 'a' && a <= 'z') return true;
    if (s.size() < 2) return false;
    const unsigned char b = static_cast<unsigned char>(s[1]);
    if (a == 0xC3) return b >= 0x9F && b <= 0xBF && b != 0xB7;  // ß-ÿ without ÷
    if (a == 0xD0) return b >= 0xB0 && b <= 0xBF;               // а-п
    if (a == 0xD1) return (b >= 0x80 && b <= 0x8F) || b == 0x91;  // р-я, ё
    return false;
}

// Is this code point a lower-case letter of the scripts Lexia reads (the same set as
// StartsLowercase: a-z, Latin-1 ß-ÿ without ÷, Cyrillic а-я and ѐ-џ)?
bool IsLowercaseCodePoint(char32_t cp) {
    return (cp >= U'a' && cp <= U'z') || (cp >= 0xDF && cp <= 0xFF && cp != 0xF7) ||
           (cp >= 0x430 && cp <= 0x45F);
}

// Does the text hold any lower-case letter? Decodes with NextCodePoint, so malformed OCR bytes
// cannot run past the end of the string; empty text has none.
bool HasLowercase(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        if (IsLowercaseCodePoint(NextCodePoint(s, i))) return true;
    }
    return false;
}

// Does the text end a sentence (. ! ? : ; or an ellipsis, optionally followed by closing quotes
// or brackets and spaces)? Then the next Line starts something new. Works on the bytes from the
// end with a bound check at every step, so a short or malformed text is safe; empty text does not.
bool EndsSentence(const std::string& s) {
    size_t n = s.size();
    auto endsWith = [&](const char* tail) {
        const size_t len = std::char_traits<char>::length(tail);
        return n >= len && s.compare(n - len, len, tail) == 0;
    };
    // Spaces, closing brackets and closing quotes (ASCII, U+2019, U+201D, U+00BB) are skipped.
    while (n > 0) {
        const char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') {
            --n;
        } else if (endsWith("\xE2\x80\x99") || endsWith("\xE2\x80\x9D")) {
            n -= 3;
        } else if (endsWith("\xC2\xBB")) {
            n -= 2;
        } else {
            break;
        }
    }
    if (n == 0) return false;
    const char last = s[n - 1];
    return last == '.' || last == '!' || last == '?' || last == ':' || last == ';' ||
           endsWith("\xE2\x80\xA6");  // U+2026 ellipsis
}

// ---- Row fragments: constants, in line heights unless noted ----
// The detector sometimes cuts one line of pixel-font or widely spaced text into word groups
// that sit side by side. They are put back together when they are one row of similar text.
const double kRowJoin = 1.2;       // the widest gap between two fragments of one row; a grid of
                                   // items ("File 01 | File 02") is ~1.7 apart and must stay apart
const double kRowOverlap = -1.0;   // fragments may overlap this much horizontally (skewed boxes)
const double kRowHeightLo = 0.65;  // height ratio of neighbours: boxes of lower-case-only text are
const double kRowHeightHi = 1.55;  //   shorter than ones with ascenders, so this is generous
const double kRowCentre = 0.35;    // the vertical centres may differ by this much

// Are `next` and the fragment `prev` (the last piece of a row so far, which `rowRight` is the right
// edge of the whole row so far) pieces of one visual row?
bool SameRow(const Rect& prev, int rowRight, const Rect& next) {
    const int hp = prev.bottom - prev.top, hn = next.bottom - next.top;
    if (hp <= 0 || hn <= 0) return false;
    const double lh = (hp + hn) / 2.0;  // the two pieces' mean height: the Shot median would
                                        // need a parameter and these two are what is compared
    const double ratio = static_cast<double>(hn) / hp;
    if (ratio < kRowHeightLo || ratio > kRowHeightHi) return false;
    const double centreDiff = std::abs((prev.top + prev.bottom) - (next.top + next.bottom)) / 2.0;
    if (centreDiff > kRowCentre * lh) return false;
    const double gap = static_cast<double>(next.left) - rowRight;
    return gap >= kRowOverlap * lh && gap <= kRowJoin * lh;
}

// ---- Tight centred stacks: constants, in line heights ----
const double kStackGap = 0.5;     // the widest gap for a centred line to go on into the next
const double kStackCentre = 0.3;  // the centres must match this well
const double kStackEdge = 0.6;    // an edge that matches this well is "aligned", not centred
// B1 limits the cue to all-caps pairs (comic bubbles, captions). B2 (false) allows any case, but
// then two real Fixture cases merge: a centred list of mixed-case statistics (unraveled_01) and
// a value that happens to be centred under another column's value (twd_02). Both get worse, so
// B1 is kept; a mixed-case centred prompt is therefore still one Block per line.
const bool kStackAllCapsOnly = true;

// Does the short Line A, with the Line B below it, form a tight centred stack (a bubble, a caption,
// a centred prompt) rather than two entries of a list? The tests: A does not end a sentence, the
// gap is tight, the centres match, and the two lines are ragged (neither the left nor the right
// edge matches), because equal-width or edge-aligned lines are the shape of a menu. Once the
// group has two members a ragged pair has already shown that it is a centred stack, so an edge
// that happens to match no longer splits it ("DOESN'T GIVE" over "ANY DETAILS"). `gap` >= 0.
bool ContinuesCentredStack(const Rect& A, const std::string& textA, const Rect& B,
                           const std::string& textB, double lh, int gap, size_t groupSize) {
    if (gap > kStackGap * lh) return false;
    if (EndsSentence(textA)) return false;
    if (kStackAllCapsOnly && (HasLowercase(textA) || HasLowercase(textB))) return false;
    const double centreDiff = std::abs((A.left + A.right) - (B.left + B.right)) / 2.0;
    if (centreDiff > kStackCentre * lh) return false;
    const bool edgeAligned = std::abs(A.left - B.left) <= kStackEdge * lh ||
                             std::abs(A.right - B.right) <= kStackEdge * lh;
    return !edgeAligned || groupSize >= 2;
}

}  // namespace

// Puts the pieces of one visual row back together. `lines` is put in reading order first (rows top
// to bottom, left to right), so the pieces of a row are neighbours; each piece that is one row with
// the one before it (SameRow) is added to it. The merged Line has the union box, the texts joined
// by one space, and the lowest confidence of its pieces (the weakest piece is the doubt about the
// whole). Heights, centres and the gap are judged against the last piece, not the union, so a long
// row does not drift. A piece is compared with its neighbour in the sorted order only: a different
// row in between simply keeps the pieces apart.
std::vector<TextLine> MergeRowFragments(std::vector<TextLine> lines) {
    SortReadingOrder(lines);
    std::vector<TextLine> out;
    Rect last = {};  // the last piece added to out.back()
    for (TextLine& l : lines) {
        if (!out.empty() && SameRow(last, out.back().box.right, l.box)) {
            TextLine& m = out.back();
            m.box.left = std::min(m.box.left, l.box.left);
            m.box.top = std::min(m.box.top, l.box.top);
            m.box.right = std::max(m.box.right, l.box.right);
            m.box.bottom = std::max(m.box.bottom, l.box.bottom);
            m.text += ' ';
            m.text += l.text;
            m.conf = std::min(m.conf, l.conf);
        } else {
            out.push_back(l);
        }
        last = l.box;
    }
    return out;
}

// The OCR's Lines -> Blocks in Reading order. First the noise is dropped (low confidence, no letter or
// digit, icon-sized one- or two-character Lines); then Lines that belong together by spacing and
// alignment become one Block; then the Blocks are put in Reading order (XyCut).
std::vector<TextBlock> GroupBlocks(std::vector<TextLine> lines, const LayoutParams& params) {
    lines.erase(std::remove_if(lines.begin(), lines.end(),
                               [](const TextLine& l) {
                                   return l.conf < 40.f || !HasLetterOrDigit(l.text);
                               }),
                lines.end());

    // A 1-2 character Line far smaller than the typical Line is an icon the detector read as a
    // letter. "Far smaller" is relative to the Shot, so it holds at any text size or scale.
    if (!lines.empty()) {
        const int typical = MedianLineHeight(lines);
        lines.erase(std::remove_if(lines.begin(), lines.end(),
                                   [typical](const TextLine& l) {
                                       return CodePointCount(l.text) <= 2 &&
                                              (l.box.bottom - l.box.top) * 2 < typical;
                                   }),
                    lines.end());
    }
    // The typical height is taken from the detector's pieces, before they are merged, so the cut
    // thresholds of XyCut stay what they were tuned with.
    const int typicalHeight = lines.empty() ? 0 : MedianLineHeight(lines);
    lines = MergeRowFragments(std::move(lines));  // also puts the Lines in reading order

    struct Group {
        std::vector<size_t> members;
        Rect box = {};
        std::vector<int> gaps;
    };
    std::vector<Group> groups;

    // Can line B (below) join the group whose last line is A? Returns the gap if so.
    auto canJoin = [&](const Group& g, const Rect& A, const TextLine& lineB, int* gapOut) {
        const Rect& B = lineB.box;
        const int ha = A.bottom - A.top, hb = B.bottom - B.top;
        if (ha <= 0 || hb <= 0) return false;
        const double lh = (ha + hb) / 2.0;
        const double ratio = static_cast<double>(hb) / ha;
        if (ratio < 0.6 || ratio > 1.7) return false;  // different text size

        const int gap = static_cast<int>(B.top - A.bottom);
        if (gap < -0.5 * lh) return false;  // side by side, not stacked

        double maxGap;
        if (g.gaps.empty()) {
            maxGap = params.paragraphGap * lh;
        } else {
            const double typical = Median(g.gaps);
            maxGap = std::max(typical * 1.8, typical + 0.5 * lh);
        }
        if (gap > maxGap) return false;

        const int wa = static_cast<int>(A.right - A.left), wb = static_cast<int>(B.right - B.left);
        const int overlap = static_cast<int>(std::min(A.right, B.right) - std::max(A.left, B.left));
        if (overlap < 0.3 * std::min(wa, wb)) return false;  // different column

        const bool leftAligned = std::abs(static_cast<int>(A.left - B.left)) <= 1.5 * lh;
        const bool rightAligned = std::abs(static_cast<int>(A.right - B.right)) <= 0.6 * lh;
        const bool centered =
            std::abs(static_cast<int>((A.left + A.right) - (B.left + B.right))) / 2.0 <= 0.6 * lh;
        if (!(leftAligned || rightAligned || centered)) return false;

        // A Line that goes on into the next one is a full-width Line of a wrapped paragraph. Short
        // stacked Lines are a menu or a list, one Block per entry, unless the next Line starts in
        // lower case (the sentence goes on: a narrow bubble, a centred subtitle). Overlapping
        // boxes (gap < 0) are tight leading in big lettering, which a list with padding never has.
        // A tight centred stack of lines that does not end a sentence (a bubble, a caption) is the
        // other cue that the text goes on.
        if (gap >= 0 && wa < params.listLine * lh && !StartsLowercase(lineB.text) &&
            !ContinuesCentredStack(A, lines[g.members.back()].text, B, lineB.text, lh, gap,
                                   g.members.size())) {
            return false;
        }

        *gapOut = gap;
        return true;
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const Rect& B = lines[i].box;
        int bestG = -1, bestGap = INT_MAX;
        for (size_t g = 0; g < groups.size(); ++g) {
            const Rect& A = lines[groups[g].members.back()].box;
            int gap = 0;
            if (canJoin(groups[g], A, lines[i], &gap) && gap < bestGap) {
                bestGap = gap;
                bestG = static_cast<int>(g);
            }
        }
        if (bestG >= 0) {
            Group& g = groups[bestG];
            g.gaps.push_back(bestGap);
            g.members.push_back(i);
            g.box.left = std::min(g.box.left, B.left);
            g.box.top = std::min(g.box.top, B.top);
            g.box.right = std::max(g.box.right, B.right);
            g.box.bottom = std::max(g.box.bottom, B.bottom);
        } else {
            Group g;
            g.members.push_back(i);
            g.box = B;
            groups.push_back(std::move(g));
        }
    }

    std::vector<TextBlock> blocks;
    for (const Group& g : groups) {
        TextBlock blk;
        blk.box = g.box;
        for (size_t idx : g.members) {
            blk.lines.push_back(lines[idx]);
            const std::string& t = lines[idx].text;
            if (!blk.text.empty()) {
                if (blk.text.back() == '-' && StartsLowercase(t)) {
                    blk.text.pop_back();  // re-join words hyphenated across lines (Latin, Latin-1, Cyrillic)
                } else {
                    blk.text += ' ';
                }
            }
            blk.text += t;
        }
        blocks.push_back(std::move(blk));
    }
    if (blocks.empty()) return blocks;
    return XyCut(std::move(blocks), typicalHeight, params.columnGap);
}

// All Blocks' text as one line, joined by spaces (for the console).
std::string BlocksToText(const std::vector<TextBlock>& blocks) {
    std::string out;
    for (const TextBlock& b : blocks) {
        if (!out.empty()) out += ' ';
        out += b.text;
    }
    return out;
}

// The Block under point `p` (a Click), with `tol` pixels of slack on each side; when boxes overlap,
// the smallest one wins, as it is the most specific. -1 if none.
int FindBlockAt(const std::vector<TextBlock>& blocks, Point p, int tol) {
    int best = -1;
    long long bestArea = LLONG_MAX;
    for (size_t i = 0; i < blocks.size(); ++i) {
        const Rect& b = blocks[i].box;
        if (p.x >= b.left - tol && p.x <= b.right + tol && p.y >= b.top - tol &&
            p.y <= b.bottom + tol) {
            const long long area = static_cast<long long>(b.right - b.left) * (b.bottom - b.top);
            if (area < bestArea) {
                bestArea = area;
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

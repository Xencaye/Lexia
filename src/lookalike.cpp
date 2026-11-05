#include "lookalike.h"

#include <vector>

#include "codepoint.h"

namespace {

// Letters that look the same in common fonts in the two scripts. Kept small on purpose: a pair is
// here only when a reader cannot tell the two apart (В and B, yes; Д and D, no; к and k differ in
// too many fonts; У and Y are left out for the same reason, only the lowercase у/y is in).
struct Pair {
    char32_t latin, cyrillic;
};
constexpr Pair kPairs[] = {
    {U'A', 0x0410}, {U'B', 0x0412}, {U'C', 0x0421}, {U'E', 0x0415}, {U'H', 0x041D},
    {U'K', 0x041A}, {U'M', 0x041C}, {U'O', 0x041E}, {U'P', 0x0420}, {U'T', 0x0422},
    {U'X', 0x0425},
    {U'a', 0x0430}, {U'c', 0x0441}, {U'e', 0x0435}, {U'o', 0x043E}, {U'p', 0x0440},
    {U'x', 0x0445}, {U'y', 0x0443},
};

// The lookalike pair that `cp` belongs to (in either script), or nullptr.
const Pair* LookalikePairOf(char32_t cp) {
    for (const Pair& p : kPairs) {
        if (cp == p.latin || cp == p.cyrillic) return &p;
    }
    return nullptr;
}

// What a word is made of. Digits and combining marks (a stress mark on a Cyrillic vowel) stay in
// the word; everything else, malformed bytes included, ends it.
bool IsWordChar(char32_t cp) {
    return IsLatinLetter(cp) || IsCyrillicLetter(cp) || (cp >= U'0' && cp <= U'9') ||
           (cp >= 0x0300 && cp <= 0x036F);
}

// One code point of the input with the bytes it came from, so that anything we do not change is
// copied back byte for byte (including malformed bytes, which decode as U+FFFD).
struct Cp {
    char32_t cp;
    size_t pos, len;
};

// The code points of `s`, each with its byte position and length, so unchanged letters can be copied
// as their original bytes.
std::vector<Cp> Decode(const std::string& s) {
    std::vector<Cp> out;
    for (size_t i = 0; i < s.size();) {
        const size_t pos = i;
        const char32_t cp = NextCodePoint(s, i);
        out.push_back({cp, pos, i - pos});
    }
    return out;
}

// Appends one code point as UTF-8 (only what the lookalike table needs: up to 3 bytes).
void AppendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {  // not reached: the table has only ASCII and U+04xx
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

// Word by word. Only a word that mixes Latin and Cyrillic letters is changed: its lookalikes become
// the script that has more script-only letters in the word (a tie, or none: tieBreak, the
// Language's script). Every other character is copied as it was.
std::string FixLookalikes(const std::string& utf8, Script tieBreak) {
    const std::vector<Cp> cps = Decode(utf8);
    std::string out;
    out.reserve(utf8.size());

    for (size_t i = 0; i < cps.size();) {
        if (!IsWordChar(cps[i].cp)) {
            out.append(utf8, cps[i].pos, cps[i].len);
            ++i;
            continue;
        }
        size_t end = i;
        size_t latinOnly = 0, cyrillicOnly = 0;  // letters that exist in one script only: the votes
        bool hasLatin = false, hasCyrillic = false;
        for (; end < cps.size() && IsWordChar(cps[end].cp); ++end) {
            const char32_t cp = cps[end].cp;
            const bool lookalike = LookalikePairOf(cp) != nullptr;
            if (IsLatinLetter(cp)) {
                hasLatin = true;
                if (!lookalike) ++latinOnly;
            } else if (IsCyrillicLetter(cp)) {
                hasCyrillic = true;
                if (!lookalike) ++cyrillicOnly;
            }
        }

        Script target = tieBreak;
        if (latinOnly > cyrillicOnly) target = Script::Latin;
        else if (cyrillicOnly > latinOnly) target = Script::Cyrillic;
        const bool mixed = hasLatin && hasCyrillic;

        for (size_t k = i; k < end; ++k) {
            const Pair* p = mixed ? LookalikePairOf(cps[k].cp) : nullptr;
            if (p) AppendUtf8(out, target == Script::Latin ? p->latin : p->cyrillic);
            else out.append(utf8, cps[k].pos, cps[k].len);
        }
        i = end;
    }
    return out;
}

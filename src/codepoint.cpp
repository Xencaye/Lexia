#include "codepoint.h"

// Reads the code point at s[i] and moves i past it. A UTF-8 lead byte says how many continuation bytes
// follow (110xxxxx: 1, 1110xxxx: 2, 11110xxx: 3); each continuation byte (10xxxxxx) adds 6 bits.
char32_t NextCodePoint(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80) return c;
    // 0xC0, 0xC1 (they could only start an overlong 2-byte form) and 0xF5..0xFF (past U+10FFFF)
    // never occur in UTF-8: one U+FFFD for the one byte, like a stray continuation byte.
    if (c == 0xC0 || c == 0xC1 || c >= 0xF5) return 0xFFFD;
    int extra;
    char32_t cp;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else return 0xFFFD;
    for (int k = 0; k < extra; ++k) {
        if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
    }
    return cp;
}

// A-Z, a-z, Latin-1 letters and Latin Extended-A/B (U+00C0..U+024F, without the signs x U+00D7 and
// / U+00F7), and Latin Extended Additional (U+1E00..U+1EFF, Vietnamese and others).
bool IsLatinLetter(char32_t cp) {
    if ((cp >= U'A' && cp <= U'Z') || (cp >= U'a' && cp <= U'z')) return true;
    if (cp >= 0xC0 && cp <= 0x24F) return cp != 0xD7 && cp != 0xF7;
    return cp >= 0x1E00 && cp <= 0x1EFF;
}

// The Cyrillic block and Cyrillic Supplement (U+0400..U+052F), without U+0482..U+0489 (a sign and
// combining marks, not letters).
bool IsCyrillicLetter(char32_t cp) { return (cp >= 0x400 && cp <= 0x481) || (cp >= 0x48A && cp <= 0x52F); }

std::optional<Script> ScriptOfLetter(char32_t cp) {
    if (IsLatinLetter(cp)) return Script::Latin;
    if (IsCyrillicLetter(cp)) return Script::Cyrillic;
    return std::nullopt;
}

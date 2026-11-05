#pragma once
// Code points and letters: the one UTF-8 decoder and the one idea of "a Latin letter" and "a
// Cyrillic letter" that the text modules (language, lookalike, stretch) share. Pure and Win32-free.
#include <cstddef>
#include <optional>
#include <string>

#include "language.h"

// Decodes the code point that starts at s[i] and advances i past it (i must be < s.size()).
// Malformed bytes: a stray continuation byte or a byte that never occurs in UTF-8 (0xC0, 0xC1,
// 0xF5..0xFF) is one U+FFFD and one byte; a lead byte whose sequence is cut short (by the end of
// the text or by a byte that is not a continuation) is one U+FFFD covering the lead and the
// continuation bytes before the break, and the breaking byte is left for the next call. U+FFFD is
// neither a letter nor a space. Overlong 3- and 4-byte forms, surrogates and 4-byte forms above
// U+10FFFF (lead 0xF4) are not checked: they decode to the value their bits give, so a caller
// that writes the result back (lookalike) may turn such a form into its well-formed letter.
char32_t NextCodePoint(const std::string& s, size_t& i);

// Latin letters: ASCII, Latin-1 and Latin Extended-A/B (without U+00D7 x and U+00F7 /), and Latin
// Extended Additional.
bool IsLatinLetter(char32_t cp);

// Cyrillic letters: U+0400..U+052F without U+0482..U+0489, the signs and combining marks.
bool IsCyrillicLetter(char32_t cp);

// The Script a letter belongs to, or nothing for a code point that is not a letter of a Script that
// a Language uses (digits, punctuation, Greek, ...).
std::optional<Script> ScriptOfLetter(char32_t cp);

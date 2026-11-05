#pragma once
// The Lookalike fix (docs/GLOSSARY.md): in a word that mixes Latin and Cyrillic letters, the
// letters that look the same in both scripts (Latin o, Cyrillic о ...) are converted to the
// script the word mostly is. Pure text logic, no Win32 and no ONNX, so it can be unit-tested.
#include <string>

#include "language.h"

// Fixes every word of `utf8` on its own. A word is a run of letters and digits; whitespace and
// punctuation end it. Only a word that has letters of both scripts is touched, and in it only the
// lookalike letters (a small table in lookalike.cpp, both cases): they are converted to the script
// that has more letters which exist in one script only (lookalike letters do not vote). A tie, or
// a word with no such letters, goes to `tieBreak`. All other letters, digits, punctuation and
// malformed bytes are kept as they are.
std::string FixLookalikes(const std::string& utf8, Script tieBreak);

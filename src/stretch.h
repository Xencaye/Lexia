#pragma once
// Which Voice speaks what: a Block's text is cut into stretches of one script, and each stretch is
// spoken by the Voice of the Language its script matches. Pure and Win32-free (UTF-8 in and out).
//
// Rules, in the order they apply:
//  - A word is a run of characters between whitespace. Its script is the one most of its letters
//    belong to (a tie goes to the script of its first letter). Only scripts that some Language in
//    the table uses count (Latin and Cyrillic today); a word with no such letters ("2024", "--",
//    a Greek word) has no script and joins the stretch around it. It does not count as a word
//    when counting words below.
//  - A run is consecutive words of one script (words with no script do not break it).
//  - A run of two or more words is a stretch of its own. A run of one word is an island: it stays
//    in the stretch before it, or, at the start of the text, in the one after it. So a single
//    foreign word, or two of them with a word of the surrounding script between, never changes
//    the Voice.
//  - If no run has two words, the whole text is one stretch, in the script with the most words
//    (a tie goes to GuessLanguage).
//  - Whitespace and words with no script between two stretches belong to the earlier one, so
//    joining the stretches' texts gives back the original text exactly.
#include <string>
#include <vector>

#include "language.h"

struct Stretch {
    std::string text;
    const Language* language = nullptr;  // always set; points into AllLanguages()
};

// Empty text gives no stretches; text with no letters gives one, in the default Language.
std::vector<Stretch> SplitIntoStretches(const std::string& utf8);

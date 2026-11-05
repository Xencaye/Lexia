#pragma once
// The Fixture score: how far the Blocks lexia_cli produced are from the Blocks a person
// expected, as numbers a script can compare before and after a change to grouping or Reading
// order. Pure functions: no Win32, no ONNX, no OpenCV.
#include <string>
#include <vector>

#include "common.h"

// Parses an expected.txt: Blocks separated by blank lines, in Reading order. Tolerates CRLF,
// a UTF-8 BOM, trailing whitespace and several blank lines in a row. Inside a Block, line
// breaks and runs of spaces collapse to one space, so a Block may be wrapped by hand.
std::vector<std::string> ParseExpectedBlocks(const std::string& utf8);

// The text of each Block, in order (what ScoreLayout wants for the produced side).
std::vector<std::string> BlockTexts(const std::vector<TextBlock>& blocks);

struct LayoutScore {
    // Edit distance between the two joined texts, divided by the longer of the two (both
    // counted in Unicode code points, not bytes). 0 = identical, 1 = nothing in common; an
    // empty result against expected text is 1, and two empty sides are 0.
    double distance = 0.0;
    size_t editDistance = 0;       // the unnormalised distance, in code points
    int expectedBlocks = 0;
    int producedBlocks = 0;
    int blockCountError = 0;       // |producedBlocks - expectedBlocks|
};

// Scores `produced` against `expected` (each entry is one Block's text). A Block is trimmed and
// its white space collapsed first, Blocks with no text are dropped, and Latin and Cyrillic case
// is ignored (case is not heard). The Blocks are joined
// with a blank line ("\n\n", as in expected.txt) before comparing, so a Block split in two or
// two merged into one costs a little in `distance` and shows in `blockCountError`, and two
// Blocks swapped costs a lot.
LayoutScore ScoreLayout(const std::vector<std::string>& expected,
                        const std::vector<std::string>& produced);

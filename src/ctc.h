#pragma once
// The recogniser's output decoding, with no ONNX in it so it can be unit-tested: the Alphabet
// mask over the model's classes and the CTC greedy decode that uses it.
//
// The recogniser's classes are: 0 = blank, 1..dict.size() = the dictionary entries (class k is
// dict[k - 1]), then one more class for the space if the model has it.
#include <string>
#include <vector>

#include "language.h"

// One flag per class: may the recogniser produce it for `lang`? The blank and the space are
// always allowed; a dictionary entry is allowed when every character of it is in the Language's
// Alphabet (InAlphabet). Built once per OcrEngine, since the engine is per Language.
std::vector<char> BuildAlphabetMask(const std::vector<std::string>& dict, bool useSpace,
                                    const Language& lang);

struct CtcResult {
    std::string text;  // UTF-8
    float conf = 0.f;  // mean score of the characters kept, 0..1; 0 when there are none
};

// CTC greedy decoding of `T` time steps of `C` class scores (row-major in `logits`): the best
// allowed class per step (`allowed` has C flags; a masked class can never win, so the next-best
// in-Alphabet class does), repeats merged, blanks dropped.
CtcResult CtcGreedyDecode(const float* logits, int T, int C, const std::vector<std::string>& dict,
                          bool useSpace, const std::vector<char>& allowed);

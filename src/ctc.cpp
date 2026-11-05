// Turning the recogniser's output into text (CTC = connectionist temporal classification).
//
// The recogniser looks at a Line image as T narrow time steps, left to right. For each step it gives
// one score per class: class 0 is the "blank" (no character here), classes 1..N are the dictionary's
// entries in order, and with useSpace the class after them is a space. Greedy decoding takes the best
// class of each step, joins repeats of one class into one character, and drops the blanks:
// "hh-e-ll-ll-o" (- = blank) gives "hello". The blank between the two "ll" runs is what keeps the
// double letter.
#include "ctc.h"

// Which classes may be chosen for this Language (E3, the Alphabet lock): 1 = allowed. The mask has the
// same order as the classes, so index 0 is the blank, which is always allowed.
std::vector<char> BuildAlphabetMask(const std::vector<std::string>& dict, bool useSpace,
                                    const Language& lang) {
    std::vector<char> mask;
    mask.reserve(dict.size() + 2);
    mask.push_back(1);  // class 0: the blank
    for (const std::string& entry : dict) mask.push_back(InAlphabet(lang, entry) ? 1 : 0);
    if (useSpace) mask.push_back(1);  // the last class: the space
    return mask;
}

// logits: T rows of C scores (row-major). A class that `allowed` masks out is skipped, so the best
// allowed class of that step wins instead. A class past the end of the mask counts as allowed.
// conf is the mean score of the characters that were output (0 when there are none).
CtcResult CtcGreedyDecode(const float* logits, int T, int C, const std::vector<std::string>& dict,
                          bool useSpace, const std::vector<char>& allowed) {
    CtcResult res;
    int last = 0;  // the best class of the step before; a repeat of it is the same character
    double confSum = 0;
    int count = 0;
    for (int t = 0; t < T; ++t) {
        const float* row = logits + static_cast<size_t>(t) * C;
        int best = 0;  // the blank is always allowed, so it is the fallback
        float bestV = row[0];
        for (int k = 1; k < C; ++k) {
            if (static_cast<size_t>(k) < allowed.size() && !allowed[static_cast<size_t>(k)]) continue;
            if (row[k] > bestV) {
                bestV = row[k];
                best = k;
            }
        }
        if (best != 0 && best != last) {  // a new character starts here
            const size_t idx = static_cast<size_t>(best - 1);  // class k is dictionary entry k - 1
            if (idx < dict.size()) {
                res.text += dict[idx];
                confSum += bestV;
                ++count;
            } else if (useSpace && idx == dict.size()) {
                res.text += ' ';
                confSum += bestV;
                ++count;
            }
        }
        last = best;
    }
    res.conf = count > 0 ? static_cast<float>(confSum / count) : 0.f;
    return res;
}

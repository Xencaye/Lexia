#include "tts/speech_chunk.h"

// One group in `phonemes` is one phoneme, repeated once per phoneme id it produced, then a 0. It
// becomes one PhonemeTiming whose length is the sum of the alignments of those ids (see the header).
// The timings are back to back from sample 0; the Reading queue cuts words with them (read_along.h).
std::vector<PhonemeTiming> PhonemeTimings(const std::vector<char32_t>& phonemes,
                                          const std::vector<int>& alignments) {
    std::vector<PhonemeTiming> out;
    size_t id = 0;  // next alignment to use
    int start = 0;
    for (size_t i = 0; i < phonemes.size();) {
        if (phonemes[i] == 0) {  // group separator
            ++i;
            continue;
        }
        PhonemeTiming t;
        t.phoneme = phonemes[i];
        t.startSample = start;
        for (; i < phonemes.size() && phonemes[i] != 0; ++i, ++id) {
            if (id < alignments.size()) t.lengthSamples += alignments[id];
        }
        start += t.lengthSamples;
        out.push_back(t);
    }
    return out;
}

// Walks back from the end, counting the code points of the chunk's own ids; the walk stops where all
// `numIds` are counted. Each group is its code points then a 0, so the cut falls just after a 0 (or at the
// start); a 0 that the cut would leave first is dropped.
std::vector<char32_t> OwnPhonemes(const std::vector<char32_t>& phonemes, size_t numIds) {
    if (numIds == 0) return phonemes;
    size_t seen = 0;
    size_t cut = phonemes.size();  // phonemes[cut..] are the own ones
    while (cut > 0 && seen < numIds) {
        --cut;
        if (phonemes[cut] != 0) ++seen;
    }
    if (seen < numIds) return phonemes;  // fewer entries than ids: nothing earlier to drop
    // `cut` is on the first code point of the own phonemes. Anything before it (an earlier sentence) goes.
    return std::vector<char32_t>(phonemes.begin() + static_cast<std::ptrdiff_t>(cut), phonemes.end());
}

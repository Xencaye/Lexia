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

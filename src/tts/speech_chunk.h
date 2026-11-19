#pragma once
// What a Voice produces: audio in chunks, each with the timing of the phonemes that made it.
// Plain data, Win32-free. The Reading queue reads the timings: it cuts words at the ' ' phonemes, so
// Announcements pause at a word boundary (B3) and the Overlay marks the heard word (G1, read_along.h).
#include <cstddef>
#include <cstdint>
#include <vector>

// One phoneme inside a chunk. Offsets are in samples from the start of the chunk's audio.
// The sequence includes Piper's markers: '^' (start of sentence), '$' (end of sentence) and ' '
// (the phoneme between two words, so a word is the run of phonemes between two spaces).
struct PhonemeTiming {
    char32_t phoneme = 0;
    int startSample = 0;
    int lengthSamples = 0;
};

struct SpeechChunk {
    std::vector<float> samples;  // mono, -1..1
    int sampleRate = 0;
    std::vector<PhonemeTiming> phonemes;  // in order, back to back
    bool isLast = false;                  // the last chunk of the text that was synthesised
    // Set when a Block is spoken as several stretches (tts/stretch_speech.h): the byte offset, in the
    // Block's text, of the stretch this chunk belongs to (0 otherwise). Then isLast means the last
    // chunk of the whole Block, and the phoneme timings are still relative to this chunk's own audio.
    size_t textOffset = 0;
};

// How speaking a text ended: all of it, stopped by the caller, or an error.
enum class SynthResult { Done, Cancelled, Failed };

// Turns Piper's per-chunk arrays into PhonemeTimings. `phonemes` holds each phoneme's code
// point once per phoneme id it produced, with a 0 after every group; `alignments` holds the
// sample count of every phoneme id, in order (see piper.h, piper_audio_chunk). A group of N
// repeated code points takes the next N alignments. Alignments missing at the end count as 0.
std::vector<PhonemeTiming> PhonemeTimings(const std::vector<char32_t>& phonemes,
                                          const std::vector<int>& alignments);

// The phonemes that belong to ONE chunk. libpiper (the pinned piper1-gpl, piper.cpp) never clears its list
// of phoneme code points between sentences, so the Nth chunk's `phonemes` holds the groups of every earlier
// sentence first and its own last, while its phoneme ids, its alignments and its audio are its own. Fed to
// PhonemeTimings as they are, the alignments of this sentence would be taken for the earlier sentences'
// phonemes and every word cut after the first sentence would be at the wrong place. `numIds` is the
// chunk's `num_phoneme_ids`: the own phonemes are the last groups whose non-zero entries add up to it
// (each id has one code point; the 0 separators have none). Returns `phonemes` unchanged when it holds no
// more than `numIds` entries, or when `numIds` is 0 (unknown).
std::vector<char32_t> OwnPhonemes(const std::vector<char32_t>& phonemes, size_t numIds);

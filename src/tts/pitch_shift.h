#pragma once
// Changing the pitch of synthesised speech without changing how long it takes (SoundTouch,
// LGPL-2.1). Win32-free.
#include <vector>

// `samples` (mono) with the pitch moved by `semitones` (limited to +-12). The result has exactly
// as many samples as the input, so phoneme timings (speech_chunk.h) stay valid. 0 semitones
// returns the input unchanged. Each call is independent: a chunk is shifted on its own.
std::vector<float> ShiftPitch(const std::vector<float>& samples, int sampleRate, float semitones);

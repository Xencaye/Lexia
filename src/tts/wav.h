#pragma once
// Writing a WAV file: 16-bit PCM, mono. Win32-free.
#include <cstdint>
#include <string>
#include <vector>

// The bytes of a complete .wav file for `samples` (-1..1; louder values are clipped).
std::string EncodeWav16(const std::vector<float>& samples, int sampleRate);

// One sample as 16-bit PCM: scaled, rounded, and clipped to -1..1.
int16_t FloatToPcm16(float sample);

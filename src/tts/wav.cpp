#include "tts/wav.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// A WAV file of 16-bit mono PCM, for `lexia_cli --say ... --wav`. A WAV file is little-endian: the
// low byte of each number comes first.
namespace {

void Put16(std::string& s, uint16_t v) {
    s += static_cast<char>(v & 0xFF);
    s += static_cast<char>(v >> 8);
}

void Put32(std::string& s, uint32_t v) {
    Put16(s, static_cast<uint16_t>(v & 0xFFFF));
    Put16(s, static_cast<uint16_t>(v >> 16));
}

}  // namespace

// -1..1 -> -32767..32767, rounded to the nearest value. A sample outside -1..1 is clipped first, so a
// loud Voice cannot wrap around to the other sign (a loud click).
int16_t FloatToPcm16(float sample) {
    return static_cast<int16_t>(std::lrintf(std::clamp(sample, -1.f, 1.f) * 32767.f));
}

std::string EncodeWav16(const std::vector<float>& samples, int sampleRate) {
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    std::string wav;
    wav.reserve(44 + dataBytes);  // the header is 44 bytes
    wav += "RIFF";
    Put32(wav, 36 + dataBytes);   // the size of the rest of the file: everything after these 8 bytes
    wav += "WAVEfmt ";
    Put32(wav, 16);                                     // size of the fmt chunk
    Put16(wav, 1);                                      // PCM
    Put16(wav, 1);                                      // mono
    Put32(wav, static_cast<uint32_t>(sampleRate));
    Put32(wav, static_cast<uint32_t>(sampleRate) * 2);  // bytes per second
    Put16(wav, 2);                                      // bytes per frame
    Put16(wav, 16);                                     // bits per sample
    wav += "data";
    Put32(wav, dataBytes);
    for (float f : samples) {
        Put16(wav, static_cast<uint16_t>(FloatToPcm16(f)));
    }
    return wav;
}

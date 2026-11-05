#include "tts/pitch_shift.h"

#include <soundtouch/SoundTouch.h>

#include <algorithm>
#include <cmath>

// Pitch, the Voice control: Piper has no pitch setting, so the speech is shifted after synthesis with
// SoundTouch, which changes the pitch and keeps the length (and so the speed and the timings).
std::vector<float> ShiftPitch(const std::vector<float>& samples, int sampleRate, float semitones) {
    if (samples.empty() || sampleRate <= 0 || std::isnan(semitones)) return samples;
    semitones = std::min(std::max(semitones, -12.f), 12.f);  // one octave each way at most
    if (semitones == 0.f) return samples;

    soundtouch::SoundTouch st;
    st.setSampleRate(static_cast<unsigned>(sampleRate));
    st.setChannels(1);
    st.setPitchSemiTones(semitones);
    st.putSamples(samples.data(), static_cast<unsigned>(samples.size()));
    st.flush();  // pushes out what is still inside, so the end of the speech is not cut off

    std::vector<float> out(samples.size());
    size_t got = 0;
    while (got < out.size()) {
        const unsigned n = st.receiveSamples(out.data() + got, static_cast<unsigned>(out.size() - got));
        if (n == 0) break;
        got += n;
    }
    // Whatever is missing at the end is silence: the length must stay exactly the same.
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(got), out.end(), 0.f);
    return out;
}

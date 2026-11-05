#include "tts/voice_controls.h"

#include <algorithm>
#include <cmath>

// The Voice controls (B4): Speed, Pitch and Volume of one Voice. Every value that comes in (a slider,
// settings.json, a Preset file) is clamped to its range, and NaN becomes the default.

// {min, max, default} of each control.
ControlRange RangeOf(VoiceControl control) {
    switch (control) {
        case VoiceControl::Speed: return {0.5f, 2.f, 1.f};   // times the Voice's own pace
        case VoiceControl::Pitch: return {-6.f, 6.f, 0.f};   // semitones
        case VoiceControl::Volume: return {0.f, 2.f, 1.f};   // gain on the samples (2 = twice as loud)
    }
    return {0.f, 0.f, 0.f};
}

float Get(const VoiceControls& c, VoiceControl control) {
    switch (control) {
        case VoiceControl::Speed: return c.speed;
        case VoiceControl::Pitch: return c.pitch;
        case VoiceControl::Volume: return c.volume;
    }
    return 0.f;
}

float ClampControl(VoiceControl control, float value) {
    const ControlRange r = RangeOf(control);
    if (std::isnan(value)) return r.def;
    return std::min(std::max(value, r.min), r.max);
}

void Set(VoiceControls& c, VoiceControl control, float value) {
    value = ClampControl(control, value);
    switch (control) {
        case VoiceControl::Speed: c.speed = value; break;
        case VoiceControl::Pitch: c.pitch = value; break;
        case VoiceControl::Volume: c.volume = value; break;
    }
}

// A copy with every control in its range (Set clamps).
VoiceControls Clamped(VoiceControls c) {
    for (VoiceControl control : {VoiceControl::Speed, VoiceControl::Pitch, VoiceControl::Volume}) {
        Set(c, control, Get(c, control));
    }
    return c;
}

void ResetControl(VoiceControls& c, VoiceControl control) { Set(c, control, RangeOf(control).def); }

void ResetAll(VoiceControls& c) { c = VoiceControls{}; }

// ------------------------------------------------------------ applying them ---

// Piper's length_scale is the length of the speech, so it is 1 / Speed: Speed 2 gives half the length.
float PaceForSpeed(float speed) { return speed > 0.f ? 1.f / speed : 1.f; }

// Multiplies every sample by `gain` and clips to -1..1, so a gain above 1 cannot wrap around.
void ApplyVolume(std::vector<float>& samples, float gain) {
    if (!(gain > 0.f)) gain = 0.f;  // also catches NaN
    for (float& s : samples) s = std::min(std::max(s * gain, -1.f), 1.f);
}

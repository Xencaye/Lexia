#pragma once
// A Voice's three controls (Speed, Pitch, Volume), their ranges and defaults. Plain data and pure
// functions, Win32-free and Piper-free: settings.json is read into these (settings.h), PiperVoice
// applies them (piper_voice.h). The Voice tab edits them; its Reset buttons call ResetControl.
#include <string>
#include <vector>

enum class VoiceControl { Speed, Pitch, Volume };

struct ControlRange {
    float min;
    float max;
    float def;  // the value Reset sets
};

// Speed: 1 is the Voice's own pace, 2 twice as fast.        0.5 .. 2     (default 1)
// Pitch: semitones; the length of the speech stays the same. -6 .. 6      (default 0)
// Volume: 1 leaves the samples as they are.                  0 .. 2       (default 1)
ControlRange RangeOf(VoiceControl control);

struct VoiceControls {
    float speed = 1.f;
    float pitch = 0.f;
    float volume = 1.f;

    bool operator==(const VoiceControls& o) const {
        return speed == o.speed && pitch == o.pitch && volume == o.volume;
    }
    bool operator!=(const VoiceControls& o) const { return !(*this == o); }
};

float Get(const VoiceControls& c, VoiceControl control);
void Set(VoiceControls& c, VoiceControl control, float value);  // clamped into the range

// `value` limited to the control's range; NaN gives the default.
float ClampControl(VoiceControl control, float value);

// All three controls clamped.
VoiceControls Clamped(VoiceControls c);

// Reset: one control, or all three, back to their defaults.
void ResetControl(VoiceControls& c, VoiceControl control);
void ResetAll(VoiceControls& c);

// ------------------------------------------------------------ applying them ---

// Piper's length_scale multiplier for a Speed (1/speed: the speed 2 halves length_scale).
float PaceForSpeed(float speed);

// Multiplies the samples by `gain` (negative counts as 0). A result outside -1..1 is held at the
// limit, never wrapped, so a loud Volume clips rather than distorts the sign.
void ApplyVolume(std::vector<float>& samples, float gain);

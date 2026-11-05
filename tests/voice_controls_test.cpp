// Unit tests for tts/voice_controls.h (the Voice controls) and
// tts/pitch_shift.h. Pure functions, hand-made data.
#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "tts/pitch_shift.h"
#include "tts/voice_controls.h"

namespace {
const float kPi = 3.14159265358979f;

std::vector<float> Sine(int rate, float hz, float seconds) {
    std::vector<float> s(static_cast<size_t>(static_cast<float>(rate) * seconds));
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = 0.5f * std::sin(2.f * kPi * hz * static_cast<float>(i) / static_cast<float>(rate));
    }
    return s;
}

// Frequency estimated from rising zero crossings in the middle half (away from the edges).
float EstimateHz(const std::vector<float>& s, int rate) {
    const size_t from = s.size() / 4, to = s.size() * 3 / 4;
    int crossings = 0;
    for (size_t i = from + 1; i < to; ++i) {
        if (s[i - 1] < 0.f && s[i] >= 0.f) ++crossings;
    }
    return static_cast<float>(crossings) * static_cast<float>(rate) / static_cast<float>(to - from);
}
}  // namespace

// ------------------------------------------------------------ controls ---

TEST_CASE("VoiceControls: the defaults are what Reset gives") {
    const VoiceControls d;
    CHECK(d.speed == 1.f);
    CHECK(d.pitch == 0.f);
    CHECK(d.volume == 1.f);
    for (VoiceControl c : {VoiceControl::Speed, VoiceControl::Pitch, VoiceControl::Volume}) {
        const ControlRange r = RangeOf(c);
        CHECK(r.min < r.max);
        CHECK(r.def >= r.min);
        CHECK(r.def <= r.max);
        CHECK(Get(d, c) == r.def);
    }
}

TEST_CASE("ClampControl: limits to the range, NaN gives the default") {
    CHECK(ClampControl(VoiceControl::Speed, 5.f) == RangeOf(VoiceControl::Speed).max);
    CHECK(ClampControl(VoiceControl::Speed, 0.f) == RangeOf(VoiceControl::Speed).min);
    CHECK(ClampControl(VoiceControl::Speed, 1.25f) == 1.25f);
    CHECK(ClampControl(VoiceControl::Pitch, -40.f) == RangeOf(VoiceControl::Pitch).min);
    CHECK(ClampControl(VoiceControl::Volume, -1.f) == 0.f);
    CHECK(ClampControl(VoiceControl::Speed, std::numeric_limits<float>::quiet_NaN()) == 1.f);
    CHECK(ClampControl(VoiceControl::Pitch, std::numeric_limits<float>::infinity()) ==
          RangeOf(VoiceControl::Pitch).max);
}

TEST_CASE("Clamped and Set keep every control inside its range") {
    VoiceControls wild;
    wild.speed = 10.f;
    wild.pitch = -99.f;
    wild.volume = 50.f;
    const VoiceControls c = Clamped(wild);
    CHECK(c.speed == RangeOf(VoiceControl::Speed).max);
    CHECK(c.pitch == RangeOf(VoiceControl::Pitch).min);
    CHECK(c.volume == RangeOf(VoiceControl::Volume).max);

    VoiceControls v;
    Set(v, VoiceControl::Speed, 100.f);
    CHECK(v.speed == RangeOf(VoiceControl::Speed).max);
}

TEST_CASE("ResetControl puts one control back and leaves the others") {
    VoiceControls c;
    c.speed = 1.5f;
    c.pitch = 3.f;
    c.volume = 0.5f;
    ResetControl(c, VoiceControl::Pitch);
    CHECK(c.pitch == 0.f);
    CHECK(c.speed == 1.5f);
    CHECK(c.volume == 0.5f);
    ResetAll(c);
    CHECK(c == VoiceControls{});
}

// ----------------------------------------------------- applying controls ---

TEST_CASE("PaceForSpeed: twice the speed halves length_scale") {
    CHECK(PaceForSpeed(1.f) == doctest::Approx(1.f));
    CHECK(PaceForSpeed(2.f) == doctest::Approx(0.5f));
    CHECK(PaceForSpeed(0.5f) == doctest::Approx(2.f));
    CHECK(PaceForSpeed(0.f) == doctest::Approx(1.f));  // nonsense counts as the Voice's own pace
}

TEST_CASE("ApplyVolume: scales, holds at the limits, never wraps") {
    std::vector<float> s = {0.25f, -0.25f, 0.8f, -0.8f};
    ApplyVolume(s, 2.f);
    CHECK(s[0] == doctest::Approx(0.5f));
    CHECK(s[1] == doctest::Approx(-0.5f));
    CHECK(s[2] == 1.f);  // 1.6 is held at 1, not wrapped to a negative number
    CHECK(s[3] == -1.f);

    std::vector<float> quiet = {0.5f, -0.5f};
    ApplyVolume(quiet, 0.f);
    CHECK(quiet[0] == 0.f);
    CHECK(quiet[1] == 0.f);

    std::vector<float> neg = {0.5f};
    ApplyVolume(neg, -3.f);  // a negative gain would flip the sign: it counts as 0
    CHECK(neg[0] == 0.f);

    std::vector<float> already = {1.7f, -3.f};  // out-of-range input is held too
    ApplyVolume(already, 1.f);
    CHECK(already[0] == 1.f);
    CHECK(already[1] == -1.f);
}

// -------------------------------------------------------------- pitch ---

TEST_CASE("ShiftPitch: zero semitones returns the input unchanged") {
    const std::vector<float> in = Sine(22050, 220.f, 0.3f);
    CHECK(ShiftPitch(in, 22050, 0.f) == in);
}

TEST_CASE("ShiftPitch: the length stays the same, the pitch moves") {
    const int rate = 22050;
    const std::vector<float> in = Sine(rate, 220.f, 1.0f);
    const float base = EstimateHz(in, rate);
    for (float semitones : {-6.f, -2.f, 3.f, 6.f}) {
        CAPTURE(semitones);
        const std::vector<float> out = ShiftPitch(in, rate, semitones);
        const double tolerance = 0.01 * static_cast<double>(in.size());  // 1 %
        CHECK(std::abs(static_cast<double>(out.size()) - static_cast<double>(in.size())) <= tolerance);
        const float expected = base * std::pow(2.f, semitones / 12.f);
        CHECK(EstimateHz(out, rate) == doctest::Approx(expected).epsilon(0.05));
    }
}

TEST_CASE("ShiftPitch: short and empty input are fine") {
    CHECK(ShiftPitch({}, 22050, 3.f).empty());
    const std::vector<float> few(10, 0.1f);
    CHECK(ShiftPitch(few, 22050, 3.f).size() == few.size());
}

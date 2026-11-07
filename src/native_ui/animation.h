#pragma once
// Transitions of the native Settings window (K1.b). The design's CSS has `transition: ... 0.12s` on the
// icon buttons, the slider thumb and the switch, and the power circle fades in 0.2 s. A Tween is one
// number moving to a target in a set time. There is no clock in here: the caller passes the time in
// seconds (any origin, only differences count), so the tests control it.
//
// Two rules from the brief:
//   - no transition before the first state is drawn (a page that is just built must not "slide in"),
//   - none when reduced motion is on.
// Both reach the Tween as `animate = false` in SetTarget: the value then jumps. Screen decides.
#include "native_ui/palette.h"

namespace nui {

constexpr double kTransitionSeconds = 0.12;  // colours, thumb size, knob position
constexpr double kPowerSeconds = 0.2;        // the power circle (later task)

// Linear mix of two colours in sRGB (alpha too); t is held to 0..1.
Colour Mix(Colour a, Colour b, float t);

// A value between three colours: amount 0 = c0, 1 = c1, 2 = c2 (an icon button: idle, hover, pressed).
Colour Ramp(Colour c0, Colour c1, Colour c2, float amount);

class Tween {
public:
    explicit Tween(float value = 0, double seconds = kTransitionSeconds)
        : from_(value), to_(value), start_(0), seconds_(seconds) {}

    // Moves towards `target`, starting from the value shown at `now`. animate == false (or a zero
    // duration) jumps. Setting the target it already has changes nothing, so the transition in progress
    // is not restarted by every redraw.
    void SetTarget(float target, double now, bool animate);

    float Value(double now) const;           // eased: smoothstep, like the CSS `ease` closely enough
    float Target() const { return to_; }
    bool Running(double now) const { return now < EndTime(); }
    double EndTime() const { return start_ + seconds_; }
    void SetSeconds(double seconds) { seconds_ = seconds; }

private:
    float from_;
    float to_;
    double start_;
    double seconds_;
};

}  // namespace nui

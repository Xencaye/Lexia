#include "native_ui/animation.h"

#include <algorithm>
#include <cmath>

namespace nui {

namespace {

float Clamp01(float t) { return std::min(1.0f, std::max(0.0f, t)); }

// One 8-bit channel of an 0xRRGGBB colour, mixed. Rounded, so a mix of two equal channels is exact.
uint32_t MixChannel(uint32_t a, uint32_t b, float t) {
    const float v = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return static_cast<uint32_t>(std::lround(v)) & 0xFF;
}

}  // namespace

Colour Mix(Colour a, Colour b, float t) {
    t = Clamp01(t);
    Colour out;
    out.rgb = (MixChannel((a.rgb >> 16) & 0xFF, (b.rgb >> 16) & 0xFF, t) << 16) |
              (MixChannel((a.rgb >> 8) & 0xFF, (b.rgb >> 8) & 0xFF, t) << 8) |
              MixChannel(a.rgb & 0xFF, b.rgb & 0xFF, t);
    out.alpha = a.alpha + (b.alpha - a.alpha) * t;
    return out;
}

Colour Ramp(Colour c0, Colour c1, Colour c2, float amount) {
    if (amount <= 1.0f) return Mix(c0, c1, amount);
    return Mix(c1, c2, amount - 1.0f);
}

void Tween::SetTarget(float target, double now, bool animate) {
    if (target == to_) return;  // same target: keep the transition that is running
    if (!animate || seconds_ <= 0) {
        from_ = to_ = target;
        start_ = now - seconds_;  // finished: Running() is false
        return;
    }
    from_ = Value(now);  // start from what is on screen, so a reversal does not jump
    to_ = target;
    start_ = now;
}

float Tween::Value(double now) const {
    if (seconds_ <= 0 || now >= start_ + seconds_) return to_;
    if (now <= start_) return from_;
    const float t = static_cast<float>((now - start_) / seconds_);
    const float eased = t * t * (3.0f - 2.0f * t);  // smoothstep
    return from_ + (to_ - from_) * eased;
}

}  // namespace nui

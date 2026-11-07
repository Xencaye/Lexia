#include "native_ui/slider.h"

#include <algorithm>
#include <cmath>

namespace nui {

namespace {

constexpr float kBoxH = 28, kTrackH = 2, kThumbW = 6, kThumbH = 14, kThumbGrownW = 12, kThumbGrownH = 25;
constexpr float kCompactW = 6, kCompactH = 12, kCompactGrownW = 9, kCompactGrownH = 20;  // the Master volume

// Values are kept to 6 decimals so 0.1 + 0.2 style sums do not leak into the messages.
double Round6(double v) { return std::round(v * 1e6) / 1e6; }

}  // namespace

bool SliderUsable(const SliderRange& r) {
    const bool finite = std::isfinite(r.min) && std::isfinite(r.max) && std::isfinite(r.value) &&
                        std::isfinite(r.step);
    if (!finite || !(r.min < r.max) || !(r.step > 0)) return false;
    return !r.log || r.min > 0;  // a log scale needs a positive minimum
}

double SliderPos(const SliderRange& r, double v) {
    if (r.log && r.min > 0) return (std::log(v) - std::log(r.min)) / (std::log(r.max) - std::log(r.min));
    return (v - r.min) / (r.max - r.min);
}

double SliderValueAt(const SliderRange& r, double pos) {
    if (r.log && r.min > 0) return r.min * std::pow(r.max / r.min, pos);
    return r.min + pos * (r.max - r.min);
}

double SliderSnap(const SliderRange& r, double v) {
    const double steps = std::round((v - r.min) / r.step);
    return std::min(r.max, std::max(r.min, Round6(r.min + steps * r.step)));
}

Slider::Slider() {
    SetRole(Role::Slider);
    SetSize(0, kBoxH);
}

void Slider::SetRange(const SliderRange& range) {
    range_ = range;
    usable_ = SliderUsable(range);
    SetFocusable(usable_);
    hasLastAsked_ = false;  // a new state: the same value may be asked for again
    SyncVisual();
    Invalidate();
}

void Slider::Clear() {
    usable_ = false;
    dragging_ = false;
    SetFocusable(false);
    SyncVisual();
    Invalidate();
}

float Slider::VisualTarget() const {
    // Grown on hover, while dragging and while it has the focus by keyboard (:focus-visible).
    return (Active() && (hovered_ || dragging_ || RingVisible())) ? 1.0f : 0.0f;
}

SizeF Slider::Measure(Canvas&, float maxWidth) { return {maxWidth, kBoxH}; }

RectF Slider::ThumbRect() const {
    const RectF box = Abs();
    const double pos = usable_ ? std::min(1.0, std::max(0.0, SliderPos(range_, range_.value))) : 0.5;
    const float g = VisualAmount();
    const float w0 = compact_ ? kCompactW : kThumbW, h0 = compact_ ? kCompactH : kThumbH;
    const float w1 = compact_ ? kCompactGrownW : kThumbGrownW, h1 = compact_ ? kCompactGrownH : kThumbGrownH;
    const float w = w0 + (w1 - w0) * g;
    const float h = h0 + (h1 - h0) * g;
    const float cx = box.x + static_cast<float>(pos) * box.w;
    return {cx - w / 2, box.y + box.h / 2 - h / 2, w, h};
}

void Slider::Paint(Canvas& canvas, const Palette& p) {
    const RectF box = Abs();
    const RectF thumb = ThumbRect();
    {
        OpacityScope fade(canvas, Active() ? 1.0f : 0.5f);
        canvas.FillRect({box.x, box.y + box.h / 2 - kTrackH / 2, box.w, kTrackH}, p.line);
        canvas.FillRect(thumb, p.accent);
    }
    if (RingVisible()) DrawFocusRing(canvas, p, thumb, 0);
}

void Slider::Ask(double value) {
    if (!usable_) return;
    value = SliderSnap(range_, value);  // never outside the range of the state
    if (value == range_.value) return;  // nothing changes: no message
    if (hasLastAsked_ && value == lastAsked_) return;  // already asked, the state has not come yet
    hasLastAsked_ = true;
    lastAsked_ = value;
    if (onValue_) onValue_(value);
}

void Slider::AskFromPointer(float x) {
    const RectF box = Abs();
    if (box.w <= 0) return;
    const double pos = std::min(1.0, std::max(0.0, static_cast<double>((x - box.x) / box.w)));
    Ask(SliderValueAt(range_, pos));
}

bool Slider::OnMouse(const MouseEvent& ev) {
    if (ev.button != MouseButton::Left && ev.kind != MouseKind::Move) return false;
    switch (ev.kind) {
        case MouseKind::Down:
            if (!Active()) return true;  // eaten, nothing to do
            dragging_ = true;  // the Screen captures the pointer, so the drag goes on outside the box
            SyncVisual();
            AskFromPointer(ev.pos.x);
            return true;
        case MouseKind::Move:
            if (dragging_) AskFromPointer(ev.pos.x);
            return dragging_;
        case MouseKind::Up:
            if (!dragging_) return false;
            dragging_ = false;
            SyncVisual();
            return true;
        default:
            return false;
    }
}

bool Slider::OnKey(const KeyEvent& ev) {
    if (!Active()) return false;
    switch (ev.key) {
        case Key::Right:
        case Key::Up: Ask(range_.value + range_.step); return true;
        case Key::Left:
        case Key::Down: Ask(range_.value - range_.step); return true;
        case Key::PageUp: Ask(range_.value + 10 * range_.step); return true;
        case Key::PageDown: Ask(range_.value - 10 * range_.step); return true;
        case Key::Home: Ask(range_.min); return true;
        case Key::End: Ask(range_.max); return true;
        default: return false;
    }
}

CursorHint Slider::Cursor() const { return Active() ? CursorHint::Hand : CursorHint::Arrow; }

AccessInfo Slider::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Slider;
    a.enabled = Active();
    if (usable_) {
        a.hasRange = true;
        a.min = range_.min;
        a.max = range_.max;
        a.current = range_.value;
        a.step = range_.step;
    }
    return a;
}

}  // namespace nui

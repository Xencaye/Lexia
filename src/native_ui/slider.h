#pragma once
// Slider: the Voice controls and the Master volume (K1.b; ui/css/components.css `.slider` and the maths
// of ui/js/controls.js, which this ports). A box 28 tall; a 2 DIP `line` track; a 6 x 14 accent thumb that
// grows to 12 x 25 on hover, drag and keyboard focus (0.12 s); a focus ring round the grown thumb;
// disabled (no state, or SetEnabled(false)): opacity 0.5.
//
// The slider never holds a value of its own: SetRange() shows what the host's `state` says. A press or
// drag on the box asks for the value under the pointer, the arrows for one step more or less, PageUp /
// PageDown for ten, Home / End for the ends; each asks by calling onValue(v) with v on the step grid
// and inside the range, and only when it is a new value (not the shown one, not the last one asked).
// The page turns onValue into a PageMessage. Speed uses the log2 mode: the position is on a log scale,
// so 1.0 sits in the middle of 0.5 .. 2.
#include "native_ui/widget.h"

#include <functional>

namespace nui {

struct SliderRange {
    double min = 0, max = 1, value = 0, step = 0.1;
    bool log = false;
};

// Pure maths, tested on their own.
bool SliderUsable(const SliderRange& r);                 // finite, min < max, step > 0 (and min > 0 for log)
double SliderPos(const SliderRange& r, double v);        // 0..1 for a value (not clamped)
double SliderValueAt(const SliderRange& r, double pos);  // the value at a position 0..1 (not snapped)
double SliderSnap(const SliderRange& r, double v);       // onto the step grid from min, held in the range

class Slider : public Widget {
public:
    Slider();
    // The state of the control. Without it (Clear) the slider is disabled and not focusable.
    void SetRange(const SliderRange& range);
    void Clear();
    bool HasRange() const { return usable_; }
    const SliderRange& Range() const { return range_; }
    void SetOnValue(std::function<void(double)> fn) { onValue_ = std::move(fn); }
    bool Dragging() const { return dragging_; }
    // The thumb as drawn now (window DIPs), for the tests: centre x from the value, size from the
    // growth transition.
    RectF ThumbRect() const;

    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    bool OnMouse(const MouseEvent& ev) override;
    bool OnKey(const KeyEvent& ev) override;
    CursorHint Cursor() const override;
    AccessInfo Access() const override;

protected:
    float VisualTarget() const override;

private:
    void Ask(double value);        // snap, then onValue if it is a new value
    void AskFromPointer(float x);  // the value under the pointer
    bool Active() const { return usable_ && enabled_; }

    SliderRange range_;
    bool usable_ = false;
    bool dragging_ = false;
    bool hasLastAsked_ = false;
    double lastAsked_ = 0;
    std::function<void(double)> onValue_;
};

}  // namespace nui

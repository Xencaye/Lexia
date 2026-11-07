#pragma once
// ScrollArea: a vertical scrolling viewport (K1.b; the Voice library list, a long Help page). Its
// children are laid out in content coordinates (y from 0 at the top of the content); the page tells the
// area how tall the content is (SetContentHeight) and the area shifts the children by minus its offset
// (ChildOffset) and clips them to its bounds. The offset survives content updates: a new content height
// only clamps it, so a list that refreshes does not jump to the top.
//
// Input: the wheel (when the area can scroll), PageUp / PageDown, Up / Down, Home / End (keys that the
// focused child did not use reach the area through the parent chain), a thin scrollbar on the right
// (drag the thumb, or press on the track), and ScrollIntoView(widget), which Tab uses so a focused
// control is never hidden.
#include "native_ui/widget.h"

namespace nui {

constexpr float kScrollbarZone = 10;   // the pointer zone on the right edge
constexpr float kScrollbarWidth = 4;   // the drawn thumb
constexpr float kScrollKeyStep = 40;   // an arrow key, in DIPs

class ScrollArea : public Widget {
public:
    ScrollArea() { SetRole(Role::Pane); }

    void SetContentHeight(float h);
    float ContentHeight() const { return contentH_; }
    float Offset() const { return offset_; }
    float MaxOffset() const;
    bool CanScroll() const { return MaxOffset() > 0; }
    void ScrollTo(float offset);
    void ScrollBy(float delta) { ScrollTo(offset_ + delta); }
    // Scrolls the least that shows `w` (a descendant) completely, with 4 DIPs to spare for its focus ring.
    void ScrollIntoView(const Widget* w);
    // The scrollbar thumb in window DIPs (empty when there is nothing to scroll), for the tests.
    RectF ThumbRect() const;

    PointF ChildOffset() const override { return {0, -offset_}; }
    void PaintChildren(Canvas& canvas, const Palette& palette) override;
    Widget* HitTest(PointF p) override;
    bool OnMouse(const MouseEvent& ev) override;
    bool OnKey(const KeyEvent& ev) override;
    void OnDescendantFocused(Widget* w) override;

private:
    bool InScrollbarZone(PointF p) const;
    void DragTo(float y);

    float contentH_ = 0;
    float offset_ = 0;
    bool dragging_ = false;
    float grab_ = 0;  // where on the thumb the drag took hold
};

}  // namespace nui

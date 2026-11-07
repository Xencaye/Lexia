#include "native_ui/scroll_area.h"

#include "native_ui/animation.h"

#include <algorithm>

namespace nui {

namespace {
constexpr float kMinThumb = 24;
constexpr float kFocusMargin = 4;
}  // namespace

float ScrollArea::MaxOffset() const { return std::max(0.0f, contentH_ - Bounds().h); }

void ScrollArea::SetContentHeight(float h) {
    contentH_ = std::max(0.0f, h);
    offset_ = std::min(offset_, MaxOffset());  // keep the place; only a shorter content moves it
    Invalidate();
}

void ScrollArea::ScrollTo(float offset) {
    const float clamped = std::min(MaxOffset(), std::max(0.0f, offset));
    if (clamped == offset_) return;
    offset_ = clamped;
    Invalidate();
}

void ScrollArea::ScrollIntoView(const Widget* w) {
    if (!w || !CanScroll()) return;
    const RectF view = Abs();
    const RectF r = w->Abs();  // already shifted by the current offset
    const float top = r.y - view.y;  // relative to the viewport's top
    const float bottom = top + r.h;
    if (top < kFocusMargin || r.h + 2 * kFocusMargin > view.h) {
        ScrollBy(top - kFocusMargin);
    } else if (bottom > view.h - kFocusMargin) {
        ScrollBy(bottom - (view.h - kFocusMargin));
    }
}

void ScrollArea::OnDescendantFocused(Widget* w) {
    ScrollIntoView(w);
    Widget::OnDescendantFocused(w);  // an area inside an area
}

RectF ScrollArea::ThumbRect() const {
    if (!CanScroll()) return {};
    const RectF view = Abs();
    const float thumbH = std::max(kMinThumb, view.h * view.h / contentH_);
    const float y = view.y + (view.h - thumbH) * (offset_ / MaxOffset());
    return {view.Right() - 2 - kScrollbarWidth, y, kScrollbarWidth, thumbH};
}

bool ScrollArea::InScrollbarZone(PointF p) const {
    return CanScroll() && p.x >= Abs().Right() - kScrollbarZone;
}

void ScrollArea::PaintChildren(Canvas& canvas, const Palette& p) {
    canvas.PushClip(Abs());
    Widget::PaintChildren(canvas, p);
    canvas.PopClip();
    if (CanScroll()) {
        // Quiet until the pointer is over the area or the thumb is being dragged.
        const Colour idle = Mix(p.line, p.accent, 0.5f);
        canvas.FillRect(ThumbRect(), (hovered_ || dragging_) ? p.accent : idle, kScrollbarWidth / 2);
    }
}

Widget* ScrollArea::HitTest(PointF p) {
    if (!Visible() || !Abs().Contains(p)) return nullptr;  // children outside the viewport are clipped
    if (InScrollbarZone(p)) return this;
    for (auto it = Children().rbegin(); it != Children().rend(); ++it) {
        if (Widget* hit = (*it)->HitTest(p)) return hit;
    }
    return this;
}

void ScrollArea::DragTo(float y) {
    const RectF view = Abs();
    const float thumbH = ThumbRect().h;
    const float travel = view.h - thumbH;
    if (travel <= 0) return;
    ScrollTo(((y - grab_ - view.y) / travel) * MaxOffset());
}

bool ScrollArea::OnMouse(const MouseEvent& ev) {
    switch (ev.kind) {
        case MouseKind::Wheel:
            if (!CanScroll()) return false;  // the wheel goes on to an area around this one
            ScrollBy(ev.wheelDelta);
            return true;
        case MouseKind::Down: {
            if (ev.button != MouseButton::Left || !InScrollbarZone(ev.pos)) return false;
            const RectF thumb = ThumbRect();
            // On the thumb: hold it where it was touched. On the track: the thumb's middle goes there.
            grab_ = (ev.pos.y >= thumb.y && ev.pos.y < thumb.Bottom()) ? ev.pos.y - thumb.y : thumb.h / 2;
            dragging_ = true;
            DragTo(ev.pos.y);
            return true;
        }
        case MouseKind::Move:
            if (dragging_) DragTo(ev.pos.y);
            return dragging_;
        case MouseKind::Up:
            if (!dragging_) return false;
            dragging_ = false;
            Invalidate();
            return true;
        default:
            return false;
    }
}

bool ScrollArea::OnKey(const KeyEvent& ev) {
    if (!CanScroll()) return false;
    const float page = Bounds().h * 0.9f;
    switch (ev.key) {
        case Key::Up: ScrollBy(-kScrollKeyStep); return true;
        case Key::Down: ScrollBy(kScrollKeyStep); return true;
        case Key::PageUp: ScrollBy(-page); return true;
        case Key::PageDown: ScrollBy(page); return true;
        case Key::Home: ScrollTo(0); return true;
        case Key::End: ScrollTo(MaxOffset()); return true;
        default: return false;
    }
}

}  // namespace nui

#include "native_ui/widget.h"

#include "native_ui/screen.h"

#include <algorithm>
#include <atomic>

namespace nui {

namespace {

// Ids are handed out from one counter for the whole process; widgets are made on the UI thread only,
// but the counter is atomic anyway so a test that builds trees in parallel stays correct.
std::atomic<int> g_nextId{1};

}  // namespace

Colour Resolve(const Palette& p, ColourRole role) {
    switch (role) {
        case ColourRole::Ink: return p.ink;
        case ColourRole::Accent: return p.accent;
        case ColourRole::Danger: return p.danger;
        case ColourRole::Line: return p.line;
        case ColourRole::OnAccent: return p.onAccent;
        case ColourRole::Paper: return p.paper;
        case ColourRole::Card: return p.card;
    }
    return p.ink;
}

Widget::Widget() : id_(g_nextId.fetch_add(1)) {}

Widget::~Widget() = default;

Widget* Widget::AddChild(std::unique_ptr<Widget> child) {
    Widget* raw = child.get();
    raw->parent_ = this;
    children_.push_back(std::move(child));
    Invalidate();
    return raw;
}

void Widget::RemoveChild(Widget* child) {
    auto it = std::find_if(children_.begin(), children_.end(),
                           [child](const std::unique_ptr<Widget>& c) { return c.get() == child; });
    if (it == children_.end()) return;
    // The Screen must drop its raw pointers (hover, focus, capture, layers) before the widget dies.
    if (Screen* s = GetScreen()) s->Forget(child);
    children_.erase(it);
    Invalidate();
}

void Widget::ClearChildren() {
    if (Screen* s = GetScreen()) {
        for (auto& c : children_) s->Forget(c.get());
    }
    children_.clear();
    Invalidate();
}

Screen* Widget::GetScreen() const {
    const Widget* w = this;
    while (w->parent_) w = w->parent_;
    return w->ownerScreen_;
}

void Widget::SetBounds(const RectF& b) {
    bounds_ = b;
    Invalidate();
}

RectF Widget::Abs() const {
    if (!parent_) return bounds_;
    const RectF p = parent_->Abs();
    const PointF off = parent_->ChildOffset();
    return {p.x + off.x + bounds_.x, p.y + off.y + bounds_.y, bounds_.w, bounds_.h};
}

SizeF Widget::Measure(Canvas&, float) { return {bounds_.w, bounds_.h}; }

void Widget::SetVisible(bool v) {
    if (v == visible_) return;
    visible_ = v;
    // A hidden widget must not keep the hover, the focus or a drag.
    if (!v) {
        if (Screen* s = GetScreen()) s->Forget(this);
    }
    Invalidate();
}

void Widget::SetEnabled(bool e) {
    if (e == enabled_) return;
    enabled_ = e;
    StateChanged();
}

void Widget::Focus() {
    if (Screen* s = GetScreen()) s->SetFocus(this);
}

AccessInfo Widget::BaseAccess() const {
    AccessInfo a;
    a.role = role_;
    a.name = name_;
    a.value = valueText_;
    a.help = help_.empty() ? tooltip_ : help_;
    a.liveRegion = liveRegion_;
    a.enabled = enabled_;
    a.focusable = FocusableNow();
    a.focused = focused_;
    return a;
}

AccessInfo Widget::Access() const { return BaseAccess(); }

void Widget::Paint(Canvas&, const Palette&) {}

void Widget::PaintChildren(Canvas& canvas, const Palette& palette) {
    for (auto& c : children_) c->PaintTree(canvas, palette);
}

void Widget::PaintTree(Canvas& canvas, const Palette& palette) {
    if (!visible_) return;
    Paint(canvas, palette);
    PaintChildren(canvas, palette);
}

Widget* Widget::HitTest(PointF p) {
    if (!visible_ || !Abs().Contains(p)) return nullptr;
    // Later children are painted on top, so they are asked first.
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (Widget* hit = (*it)->HitTest(p)) return hit;
    }
    return this;
}

bool Widget::OnMouse(const MouseEvent&) { return false; }
bool Widget::OnKey(const KeyEvent&) { return false; }

void Widget::PrepareLayout(Canvas& canvas) {
    for (auto& c : children_) {
        if (c->visible_) c->PrepareLayout(canvas);
    }
}

void Widget::OnDescendantFocused(Widget* w) {
    if (parent_) parent_->OnDescendantFocused(w);
}

void Widget::OnPopupClosed() {}

float Widget::VisualTarget() const { return (enabled_ && hovered_) ? 1.0f : 0.0f; }

void Widget::StateChanged() {
    SyncVisual();
    Invalidate();
}

void Widget::SyncVisual() { MoveTween(visual_, VisualTarget()); }

float Widget::VisualAmount() const { return TweenValue(visual_); }

void Widget::Invalidate() {
    if (Screen* s = GetScreen()) s->Invalidate();
}

double Widget::Now() const {
    const Screen* s = GetScreen();
    return s ? s->Now() : 0.0;
}

bool Widget::Animate() const {
    const Screen* s = GetScreen();
    return s && s->AnimationsOn();
}

void Widget::MoveTween(Tween& t, float target) {
    Screen* s = GetScreen();
    if (!s) {
        t.SetTarget(target, 0.0, false);  // not in a tree yet: the starting look is the target
        return;
    }
    t.SetTarget(target, s->Now(), s->AnimationsOn());
    if (t.Running(s->Now())) s->NoteAnimationUntil(t.EndTime());
    s->Invalidate();
}

float Widget::TweenValue(const Tween& t) const { return t.Value(Now()); }

bool Widget::RingVisible() const {
    const Screen* s = GetScreen();
    return focused_ && s && s->FocusVisible();
}

void Widget::SetHovered(bool h) {
    if (h == hovered_) return;
    hovered_ = h;
    StateChanged();
}

void Widget::SetFocused(bool f) {
    if (f == focused_) return;
    focused_ = f;
    StateChanged();
}

void DrawFocusRing(Canvas& canvas, const Palette& palette, const RectF& around, float radius) {
    // `outline: 2px solid accent; outline-offset: 2px`: the 2 DIP line sits 2 DIPs outside the widget,
    // so its centre line is 3 DIPs out.
    const RectF r{around.x - 3, around.y - 3, around.w + 6, around.h + 6};
    canvas.StrokeRect(r, palette.accent, 2.0f, radius > 0 ? radius + 3 : 0);
}

}  // namespace nui

#include "native_ui/screen.h"

#include "native_ui/sheet.h"

#include <algorithm>

namespace nui {

Screen::Screen() : root_(std::make_unique<Widget>()), overlay_(std::make_unique<Widget>()) {
    // The two roots are how every widget finds its Screen (Widget::GetScreen walks up to a root).
    root_->ownerScreen_ = this;
    overlay_->ownerScreen_ = this;
    SetSize(size_);
}

Screen::~Screen() = default;

void Screen::SetSize(SizeF size) {
    size_ = size;
    root_->SetBounds({0, 0, size.w, size.h});
    overlay_->SetBounds({0, 0, size.w, size.h});
    Invalidate();
}

// ------------------------------------------------------------------------------ time ---

void Screen::Tick(double now) {
    now_ = now;
    // A transition that ended between two ticks still needs one more repaint to show its end value, so
    // "was animating at the last tick" also counts.
    const bool was = animating_;
    animating_ = now_ < animUntil_;
    if (was || animating_) dirty_ = true;
}

void Screen::NoteAnimationUntil(double endTime) {
    animUntil_ = std::max(animUntil_, endTime);
}

bool Screen::NeedsTick() const {
    if (animating_ || now_ < animUntil_) return true;
    // A tooltip countdown: the hovered widget has a tooltip that is not shown yet.
    if (hover_ && !tooltipHidden_) {
        for (const Widget* w = hover_; w; w = w->Parent()) {
            if (!w->Tooltip().empty()) return now_ < hoverSince_ + tooltipDelay_;
        }
    }
    return false;
}

void Screen::SetReducedMotion(bool reduced) {
    reduced_ = reduced;
    Invalidate();
}

// ---------------------------------------------------------------------------- drawing ---

void Screen::Layout(Canvas& canvas) {
    root_->PrepareLayout(canvas);
    overlay_->PrepareLayout(canvas);
}

void Screen::Paint(Canvas& canvas, const Palette& palette) {
    Layout(canvas);
    root_->PaintTree(canvas, palette);
    for (const Layer& layer : layers_) {
        // A sheet dims everything under it, including the sheets below it.
        if (layer.modal) canvas.FillRect({0, 0, size_.w, size_.h}, palette.scrim);
        layer.widget->PaintTree(canvas, palette);
    }
    dirty_ = false;
    drawn_ = true;  // from now on state changes animate
}

// ------------------------------------------------------------------------------ mouse ---

bool Screen::Inside(const Widget* w, const Widget* tree) {
    for (; w; w = w->Parent()) {
        if (w == tree) return true;
    }
    return false;
}

Widget* Screen::HitTarget(PointF p, bool& outsideModal) const {
    outsideModal = false;
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (Widget* hit = it->widget->HitTest(p)) return hit;
        if (it->modal) {
            outsideModal = true;  // the sheet blocks everything under it
            return nullptr;
        }
    }
    return root_->HitTest(p);
}

void Screen::SetHover(Widget* w) {
    if (w == hover_) return;
    Widget* old = hover_;
    hover_ = w;
    hoverSince_ = now_;
    tooltipHidden_ = false;
    if (old) old->SetHovered(false);
    if (w) w->SetHovered(true);
    Invalidate();
}

bool Screen::DispatchWheel(Widget* target, const MouseEvent& ev) {
    // The wheel goes up from the widget under the pointer until a ScrollArea (or such) uses it.
    for (Widget* w = target; w; w = w->Parent()) {
        if (w->OnMouse(ev)) return true;
    }
    return false;
}

bool Screen::HandleMouse(const MouseEvent& ev) {
    ++dispatchDepth_;
    bool used = false;

    if (ev.kind == MouseKind::Leave) {
        SetHover(nullptr);
        used = true;
    } else {
        bool blocked = false;
        Widget* target = HitTarget(ev.pos, blocked);
        SetHover(target);

        // A press outside an open popup closes it and is not used for anything else.
        const bool isDown = ev.kind == MouseKind::Down;
        bool popupSwallow = false;
        if (isDown && !layers_.empty() && !layers_.back().modal &&
            !(target && Inside(target, layers_.back().widget))) {
            ClosePopupNow();
            popupSwallow = true;
        }

        if (popupSwallow) {
            used = true;
        } else if (ev.kind == MouseKind::Wheel) {
            used = DispatchWheel(target, ev);
        } else if (ev.kind == MouseKind::Move) {
            if (capture_) used = capture_->OnMouse(ev);
            else if (target) used = target->OnMouse(ev);
        } else if (isDown) {
            if (blocked || !target) {
                used = true;  // a press on the scrim: nothing happens
            } else if (ev.button == MouseButton::Left) {
                SetFocusVisible(false);  // the mouse is in use: no focus ring
                tooltipHidden_ = true;   // a press hides the tooltip
                // The nearest widget up the chain that can take the focus gets it; a press on bare
                // paper clears the focus (like a click outside a field in a browser). A press in an
                // open popup leaves the focus on the Select that owns it.
                const bool inPopup = !layers_.empty() && !layers_.back().modal &&
                                     Inside(target, layers_.back().widget);
                if (!inPopup) {
                    Widget* f = target;
                    while (f && !f->FocusableNow()) f = f->Parent();
                    if (!(f && Inside(f, ScopeRoot()))) f = nullptr;
                    SetFocus(f);
                }
                capture_ = target;  // the whole press, drag and release goes to this widget
                used = target->OnMouse(ev);
            } else {
                used = target->OnMouse(ev);  // a right press: no focus, no capture
            }
        } else if (ev.kind == MouseKind::Up) {
            Widget* receiver = capture_ ? capture_ : target;
            if (ev.button == MouseButton::Left) capture_ = nullptr;
            if (receiver) used = receiver->OnMouse(ev);
            else used = blocked;
        }
    }

    if (--dispatchDepth_ == 0) RunDeferred();
    return used;
}

CursorHint Screen::Cursor() const {
    if (capture_) return capture_->Cursor();
    return hover_ ? hover_->Cursor() : CursorHint::Arrow;
}

std::optional<TooltipRequest> Screen::Tooltip() const {
    if (!hover_ || tooltipHidden_ || now_ < hoverSince_ + tooltipDelay_) return std::nullopt;
    for (const Widget* w = hover_; w; w = w->Parent()) {
        if (!w->Tooltip().empty()) return TooltipRequest{w->Tooltip(), w->Abs()};
    }
    return std::nullopt;
}

// ------------------------------------------------------------------------------ focus ---

void Screen::SetFocus(Widget* w) {
    if (w == focused_) return;
    Widget* old = focused_;
    focused_ = w;
    if (old) old->SetFocused(false);
    if (w) {
        w->SetFocused(true);
        // A ScrollArea above scrolls the widget into view (Tab into a clipped control).
        if (w->Parent()) w->Parent()->OnDescendantFocused(w);
    }
    Invalidate();
}

void Screen::SetFocusVisible(bool v) {
    if (v == focusVisible_) return;
    focusVisible_ = v;
    // The ring and the slider thumb's growth depend on it: tell the focused widget.
    if (focused_) focused_->StateChanged();
    Invalidate();
}

Widget* Screen::ScopeRoot() const {
    return layers_.empty() ? root_.get() : layers_.back().widget;
}

void Screen::CollectFocus(Widget* w, std::vector<Widget*>& out) const {
    if (!w->Visible()) return;
    if (w->FocusableNow()) out.push_back(w);
    for (const auto& c : w->Children()) CollectFocus(c.get(), out);
}

std::vector<Widget*> Screen::FocusOrder() const {
    std::vector<Widget*> out;
    CollectFocus(ScopeRoot(), out);
    return out;
}

void Screen::MoveFocus(bool backwards) {
    const std::vector<Widget*> order = FocusOrder();
    if (order.empty()) return;
    const auto it = std::find(order.begin(), order.end(), focused_);
    size_t next;
    if (it == order.end()) {
        next = backwards ? order.size() - 1 : 0;  // the focus is elsewhere: start at an end
    } else {
        const size_t i = static_cast<size_t>(it - order.begin());
        next = backwards ? (i + order.size() - 1) % order.size() : (i + 1) % order.size();
    }
    SetFocus(order[next]);
}

// ------------------------------------------------------------------------------- keys ---

bool Screen::HandleKey(const KeyEvent& ev) {
    ++dispatchDepth_;
    SetFocusVisible(true);  // the keyboard is in use: the ring shows
    bool used = false;

    // An open popup (a Select's list) gets the keys first.
    if (!layers_.empty() && !layers_.back().modal) {
        Widget* popup = layers_.back().widget;
        if (popup->OnKey(ev)) {
            used = true;
        } else if (ev.key == Key::Tab) {
            ClosePopupNow();  // Tab closes the list and then moves on as usual
        } else {
            if (ev.key == Key::Escape) ClosePopupNow();
            used = true;  // the list is open: other keys do nothing
        }
    }

    if (!used) {
        Widget* f = focused_;
        if (f && f->CapturesAllKeys()) {
            used = f->OnKey(ev);  // a key chip waiting for its key takes even Tab and Esc
        } else if (ev.key == Key::Tab && !ev.ctrl && !ev.alt) {
            MoveFocus(ev.shift);
            used = true;
        } else {
            // Keys go up from the focused widget until one uses them (a ScrollArea around a slider).
            for (Widget* w = f; w && !used; w = w->Parent()) used = w->OnKey(ev);
            if (!used && ev.key == Key::Escape && TopSheet()) {
                CloseTopSheet();
                used = true;
            }
        }
    }

    if (--dispatchDepth_ == 0) RunDeferred();
    return used;
}

// ----------------------------------------------------------------------------- layers ---

Sheet* Screen::OpenSheet(std::unique_ptr<Sheet> sheet) {
    Sheet* raw = sheet.get();
    Layer layer;
    layer.widget = overlay_->AddChild(std::move(sheet));
    layer.modal = true;
    layer.opener = focused_;
    ClosePopupNow();  // a list open under a sheet would stay open over nothing
    layers_.push_back(layer);
    SetHover(nullptr);
    // The focus moves into the sheet: its chosen widget, else the first one that can take it.
    Widget* first = raw->InitialFocus();
    if (!first) {
        const std::vector<Widget*> order = FocusOrder();
        if (!order.empty()) first = order.front();
    }
    SetFocus(first);
    Invalidate();
    return raw;
}

Sheet* Screen::TopSheet() const {
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (it->modal) return static_cast<Sheet*>(it->widget);
    }
    return nullptr;
}

size_t Screen::SheetCount() const {
    return static_cast<size_t>(std::count_if(layers_.begin(), layers_.end(),
                                             [](const Layer& l) { return l.modal; }));
}

void Screen::CloseTopSheet() {
    if (Sheet* s = TopSheet()) CloseSheet(s);
}

void Screen::CloseSheet(Sheet* sheet) {
    Defer([this, sheet] {
        const auto it = std::find_if(layers_.begin(), layers_.end(),
                                     [sheet](const Layer& l) { return l.widget == sheet; });
        if (it == layers_.end()) return;  // already closed
        const size_t index = static_cast<size_t>(it - layers_.begin());
        Widget* opener = it->opener;
        const bool focusWasInside = focused_ && Inside(focused_, sheet);
        sheet->NotifyClosed();  // before it is destroyed
        // A popup that belongs to the sheet's widgets goes with it.
        for (size_t i = layers_.size(); i-- > index + 1;) {
            if (!layers_[i].modal && layers_[i].owner && Inside(layers_[i].owner, sheet)) RemoveLayer(i);
        }
        RemoveLayer(index);
        // The focus goes back to what had it when the sheet opened (if that still exists).
        if (focusWasInside || !focused_) SetFocus(opener);
        Invalidate();
    });
}

Widget* Screen::OpenPopup(std::unique_ptr<Widget> popup, Widget* owner) {
    ClosePopupNow();
    Layer layer;
    layer.widget = overlay_->AddChild(std::move(popup));
    layer.modal = false;
    layer.owner = owner;
    layers_.push_back(layer);
    Invalidate();
    return layer.widget;
}

Widget* Screen::Popup() const {
    return (!layers_.empty() && !layers_.back().modal) ? layers_.back().widget : nullptr;
}

void Screen::ClosePopup() {
    Defer([this] { ClosePopupNow(); });
}

void Screen::ClosePopupNow() {
    if (layers_.empty() || layers_.back().modal) return;
    Widget* owner = layers_.back().owner;
    RemoveLayer(layers_.size() - 1);
    if (owner) owner->OnPopupClosed();
}

void Screen::RemoveLayer(size_t index) {
    Widget* w = layers_[index].widget;
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(index));
    // Destroys the widget; RemoveChild makes the Screen forget hover / focus / capture inside it.
    overlay_->RemoveChild(w);
}

// ---------------------------------------------------------------------------- plumbing ---

void Screen::Defer(std::function<void()> fn) {
    if (dispatchDepth_ > 0) deferred_.push_back(std::move(fn));
    else fn();
}

void Screen::RunDeferred() {
    // A deferred call may defer more: go on until the list is empty.
    while (!deferred_.empty()) {
        std::vector<std::function<void()>> now;
        now.swap(deferred_);
        for (auto& fn : now) fn();
    }
}

void Screen::Forget(Widget* subtree) {
    if (hover_ && Inside(hover_, subtree)) SetHover(nullptr);
    if (capture_ && Inside(capture_, subtree)) capture_ = nullptr;
    if (focused_ && Inside(focused_, subtree)) {
        Widget* old = focused_;
        focused_ = nullptr;
        old->SetFocused(false);
    }
    bool closePopup = false;
    for (Layer& l : layers_) {
        if (l.opener && Inside(l.opener, subtree)) l.opener = nullptr;
        if (l.owner && Inside(l.owner, subtree)) {
            // The Select that owns an open list is going: the list goes too (after the current event;
            // not inside this loop, which it would change).
            l.owner = nullptr;
            if (!l.modal) closePopup = true;
        }
    }
    if (closePopup) ClosePopup();
}

}  // namespace nui

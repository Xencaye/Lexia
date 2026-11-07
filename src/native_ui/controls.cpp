#include "native_ui/controls.h"

#include "native_ui/text_layout.h"

#include <algorithm>

namespace nui {

namespace {

constexpr float kIconButtonSize = 28, kIconButtonIcon = 22, kIconButtonRadius = 4;
constexpr float kButtonMinHeight = 34, kButtonSmallHeight = 28, kButtonPad = 18, kButtonSmallPad = 12,
                kButtonRadius = 4;
constexpr float kSwitchW = 22, kSwitchH = 12, kSwitchBorder = 1.5f, kSwitchGap = 8, kSwitchRowH = 28,
                kKnobTravel = 10;
constexpr float kChipMinW = 70, kChipH = 24, kChipPad = 8, kChipRadius = 2;

// The rectangle shrunk by `by` on every side (a stroke is centred on its path, so a border that must
// lie inside a box is drawn on the box shrunk by half its width).
RectF Inset(const RectF& r, float by) { return {r.x + by, r.y + by, r.w - 2 * by, r.h - 2 * by}; }

Colour Transparent(Colour c) { return {c.rgb, 0.0f}; }

}  // namespace

// ----------------------------------------------------------------------------- Clickable ---

Clickable::Clickable() { SetFocusable(true); }

bool Clickable::OnMouse(const MouseEvent& ev) {
    if (ev.button != MouseButton::Left && ev.kind != MouseKind::Move) return false;
    switch (ev.kind) {
        case MouseKind::Down:
            // Pressed even when disabled: the click is eaten, nothing happens.
            down_ = true;
            inside_ = true;
            SyncVisual();
            return true;
        case MouseKind::Move:
            if (down_) {
                // Dragging off the widget lifts the pressed look; coming back puts it again.
                inside_ = Abs().Contains(ev.pos);
                SyncVisual();
            }
            return down_;
        case MouseKind::Up: {
            if (!down_) return false;
            const bool over = Abs().Contains(ev.pos);
            down_ = false;
            inside_ = false;
            SyncVisual();
            if (over && enabled_) Activate();  // press and release on the widget = a click
            return true;
        }
        default:
            return false;
    }
}

bool Clickable::OnKey(const KeyEvent& ev) {
    if ((ev.key == Key::Enter || ev.key == Key::Space) && ev.NoModifiers() && enabled_) {
        if (!ev.repeat) Activate();  // holding the key must not click again and again
        return true;
    }
    return false;
}

float Clickable::VisualTarget() const {
    if (!enabled_) return 0;
    if (PressedLook()) return 2;
    return hovered_ ? 1.0f : 0.0f;
}

void Clickable::Activate() {
    if (enabled_ && onActivate_) onActivate_();
}

bool Clickable::AccInvoke() {
    if (!enabled_) return false;  // a disabled control does what it does with a click: nothing, and says so
    Activate();
    return true;
}

// ----------------------------------------------------------------------------- IconButton ---

IconButton::IconButton(std::string icon) : icon_(std::move(icon)) {
    SetRole(Role::Button);
    SetSize(kIconButtonSize, kIconButtonSize);
    SetFocusWhenDisabled(true);  // a disabled one explains itself by its tooltip, so it can be focused
}

void IconButton::SetPressed(bool pressed) {
    pressed_ = pressed;
    toggle_ = true;  // once the page has said pressed / not pressed, it is a toggle button
    SyncVisual();
    Invalidate();
}

float IconButton::VisualTarget() const {
    if (pressed_) return 2;  // aria-pressed wins over the disabled look, as in the CSS
    return Clickable::VisualTarget();
}

void IconButton::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const float a = VisualAmount();
    // Idle has no tile; the tile fades in from the hover colour so the corners do not flash.
    const Colour tile = Ramp(Transparent(p.line), p.line, p.accent, a);
    const Colour glyph = Ramp(idleAccent_ ? p.accent : p.line, p.accent, p.onAccent, a);
    if (tile.alpha > 0.0f) canvas.FillRect(r, tile, kIconButtonRadius);
    const float off = (kIconButtonSize - kIconButtonIcon) / 2;
    canvas.Icon({r.x + off, r.y + off, kIconButtonIcon, kIconButtonIcon}, icon_, glyph);
    if (RingVisible()) DrawFocusRing(canvas, p, r, kIconButtonRadius);
}

AccessInfo IconButton::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Button;
    if (toggle_) a.toggle = pressed_ ? Toggle::On : Toggle::Off;
    return a;
}

// -------------------------------------------------------------------------------- TextLink ---

TextLink::TextLink(std::string text, std::string icon) : text_(std::move(text)), icon_(std::move(icon)) {
    SetRole(Role::Link);
    SetName(text_);
}

SizeF TextLink::Measure(Canvas& canvas, float) {
    TextStyle regular;
    regular.size = FontSize();
    TextStyle bold = regular;
    bold.weight = FontWeight::Bold;
    // The bold width is always reserved, so the current page's word does not push its neighbours.
    const float word = reserveBold_ ? std::max(canvas.MeasureText(text_, regular).w, canvas.MeasureText(text_, bold).w)
                                    : canvas.MeasureText(text_, regular).w;
    const float lineH = canvas.MeasureText(text_, regular).h;
    const float iconW = icon_.empty() ? 0.0f : iconSize_ + gap_;
    return {iconW + word, std::max(lineH, icon_.empty() ? 0.0f : iconSize_)};
}

void TextLink::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    {
        OpacityScope fade(canvas, enabled_ ? 1.0f : 0.55f);
        float x = r.x;
        if (!icon_.empty()) {
            canvas.Icon({x, r.y + (r.h - iconSize_) / 2, iconSize_, iconSize_}, icon_, p.accent);
            x += iconSize_ + gap_;
        }
        TextStyle st;
        st.size = FontSize();
        st.weight = current_ ? FontWeight::Bold : FontWeight::Regular;
        st.underline = small_ || (hovered_ && enabled_);
        // The word box is as wide as the bold word; a regular word sits in the middle of it (a <button>'s
        // text is centred, and the old page did the same), so a menu of links keeps its look when the current
        // one turns bold.
        if (reserveBold_ && !current_) {
            TextStyle bold = st;
            bold.weight = FontWeight::Bold;
            const float room = canvas.MeasureText(text_, bold).w - canvas.MeasureText(text_, st).w;
            if (room > 0) x += room / 2;
        }
        canvas.Text({x, r.y, std::max(0.0f, r.Right() - x), r.h}, text_, st, p.ink);
    }
    if (RingVisible()) DrawFocusRing(canvas, p, r, 2);
}

AccessInfo TextLink::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Link;
    a.selected = current_;  // the page the window shows now
    a.selectable = nav_;
    return a;
}

// ---------------------------------------------------------------------------------- Button ---

Button::Button(std::string text) : text_(std::move(text)) {
    SetRole(Role::Button);
    SetName(text_);
    SetSize(0, kButtonMinHeight);
}

TextStyle Button::Style() const {
    TextStyle st;
    st.size = small_ ? 14.0f : 16.0f;
    st.weight = FontWeight::Bold;
    st.align = TextAlign::Centre;
    return st;
}

SizeF Button::Measure(Canvas& canvas, float) {
    const float pad = small_ ? kButtonSmallPad : kButtonPad;
    const float minH = small_ ? kButtonSmallHeight : kButtonMinHeight;
    const SizeF t = canvas.MeasureText(text_, Style());
    return {t.w + 2 * pad, std::max(minH, t.h)};
}

void Button::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    // A disabled button is the quiet one, a little fainter.
    const bool quietLook = quiet_ || !enabled_;
    {
        OpacityScope fade(canvas, enabled_ ? 1.0f : 0.7f);
        canvas.FillRect(r, quietLook ? p.line : p.accent, kButtonRadius);
        canvas.Text(r, text_, Style(), quietLook ? p.accent : p.onAccent);
    }
    if (RingVisible()) DrawFocusRing(canvas, p, r, kButtonRadius);
}

AccessInfo Button::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Button;
    return a;
}

// -------------------------------------------------------------------------------- SwitchRow ---

SwitchRow::SwitchRow(std::string label) : label_(std::move(label)) {
    SetRole(Role::Switch);
    SetName(label_);
    SetSize(0, kSwitchRowH);
    SetFocusWhenDisabled(true);  // aria-disabled in the old page: the row explains itself
}

void SwitchRow::SetChecked(bool checked) {
    checked_ = checked;
    MoveTween(on_, checked ? 1.0f : 0.0f);
    Invalidate();
}

SizeF SwitchRow::Measure(Canvas& canvas, float) {
    TextStyle st;
    return {canvas.MeasureText(label_, st).w + kSwitchGap + kSwitchW, kSwitchRowH};
}

void SwitchRow::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const float t = TweenValue(on_);
    TextStyle st;
    st.underline = hovered_ && enabled_;  // a disabled row has no hover look
    st.align = TextAlign::Right;
    const float labelW = canvas.MeasureText(label_, st).w;
    const RectF pill{r.Right() - kSwitchW, r.y + (r.h - kSwitchH) / 2, kSwitchW, kSwitchH};
    {
        OpacityScope fade(canvas, enabled_ ? 1.0f : 0.5f);
        canvas.Text({r.x, r.y, pill.x - kSwitchGap - r.x, r.h}, label_, st, p.ink);

        // The pill: accent fill fades in with `on`; the 1.5 border always shows.
        canvas.FillRect(pill, Mix(Transparent(p.accent), p.accent, t), kSwitchH / 2);
        canvas.StrokeRect(Inset(pill, kSwitchBorder / 2), p.accent, kSwitchBorder,
                          kSwitchH / 2 - kSwitchBorder / 2);
        // The knob is a ring as tall as the pill, drawn over the pill's own edge. Off it sits at the
        // pill's left end, on it 10 DIPs to the right (CSS left -1.5 / 8.5 inside the padding box).
        const RectF knob{pill.x + kKnobTravel * t, pill.y, kSwitchH, kSwitchH};
        canvas.FillEllipse(knob, Mix(Transparent(p.onAccent), p.onAccent, t));
        canvas.StrokeEllipse(Inset(knob, kSwitchBorder / 2), p.accent, kSwitchBorder);
    }
    if (RingVisible()) {
        const float left = pill.x - kSwitchGap - labelW;
        DrawFocusRing(canvas, p, {left, r.y, r.Right() - left, r.h}, 2);
    }
}

AccessInfo SwitchRow::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Switch;
    a.toggle = checked_ ? Toggle::On : Toggle::Off;
    return a;
}

// ---------------------------------------------------------------------------------- KeyChip ---

KeyChip::KeyChip(std::string text) : text_(std::move(text)) {
    SetRole(Role::KeyChip);
    SetSize(kChipMinW, kChipH);
}

void KeyChip::SetState(State s) {
    state_ = s;
    Invalidate();
}

// The hover and focus flags come here through Widget::StateChanged. Compare with the last seen values so
// each callback fires once per change, not on every redraw-driven call.
void KeyChip::StateChanged() {
    Clickable::StateChanged();
    const bool reached = (hovered_ && !wasHovered_) || (focused_ && !wasFocused_);
    const bool blurred = wasFocused_ && !focused_;
    wasHovered_ = hovered_;
    wasFocused_ = focused_;
    if (reached && onReach_) onReach_();
    // The callback runs last: it may change this chip's state (the capture ends).
    if (blurred && state_ == State::Capturing && onBlur_) onBlur_();
}

bool KeyChip::OnKey(const KeyEvent& ev) {
    if (state_ == State::Capturing) {
        if (onKey_) onKey_(ev);
        return true;  // everything is the page's to read, Tab and Esc too
    }
    return Clickable::OnKey(ev);
}

SizeF KeyChip::Measure(Canvas& canvas, float) {
    TextStyle st;
    st.size = 17;
    st.weight = FontWeight::Bold;
    return {std::max(kChipMinW, canvas.MeasureText(text_, st).w + 2 * kChipPad), kChipH};
}

void KeyChip::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    TextStyle st;
    st.size = 17;
    st.weight = FontWeight::Bold;
    st.align = TextAlign::Centre;
    switch (state_) {
        case State::Set:
            canvas.FillRect(r, p.accent, kChipRadius);
            canvas.Text(r, text_, st, p.onAccent);
            break;
        case State::None:
            canvas.FillRect(r, p.line, kChipRadius);
            canvas.Text(r, text_, st, p.accent);
            break;
        case State::Capturing:
            // `outline: 2px solid accent; outline-offset: -2px`: the outline lies inside the chip.
            canvas.FillRect(r, p.card, kChipRadius);
            canvas.StrokeRect(Inset(r, 1), p.accent, 2.0f, kChipRadius);
            canvas.Text(r, text_, st, p.accent);
            break;
    }
    if (RingVisible()) DrawFocusRing(canvas, p, r, kChipRadius);
}

AccessInfo KeyChip::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::KeyChip;
    a.value = text_;
    return a;
}

}  // namespace nui

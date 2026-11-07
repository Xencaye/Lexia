#pragma once
// The clickable controls of the native Settings window (K1.b; look from old ui/css/components.css, numbers
// in DIPs): IconButton, TextLink (also the NavLink), Button, SwitchRow and KeyChip. They share Clickable,
// which turns a left press and release on the widget (or Enter / Space) into one call of onActivate.
//
// None of them changes what it shows by itself. A click only calls onActivate; the page code builds a
// PageMessage (Screen::Emit) and later calls SetPressed / SetChecked / SetState from the next `state`.
#include "native_ui/widget.h"

#include <functional>
#include <string>

namespace nui {

class Clickable : public Widget {
public:
    Clickable();
    void SetOnActivate(std::function<void()> fn) { onActivate_ = std::move(fn); }
    bool OnMouse(const MouseEvent& ev) override;
    bool OnKey(const KeyEvent& ev) override;
    CursorHint Cursor() const override { return enabled_ ? CursorHint::Hand : CursorHint::Arrow; }
    // UI Automation Invoke / Toggle (K.5): the same as a click. False when the widget is disabled.
    bool AccInvoke() override;

protected:
    // Is the press in progress and the pointer still over the widget? (the CSS :active)
    bool PressedLook() const { return enabled_ && down_ && inside_; }
    float VisualTarget() const override;
    virtual void Activate();

private:
    std::function<void()> onActivate_;
    bool down_ = false;
    bool inside_ = false;
};

// ------------------------------------------------------------------------------ IconButton ---
// 28 x 28, radius 4, icon 22. Idle: the icon in `line`. Hover: `line` fill, icon in accent. Pressed (the
// mouse is down, or SetPressed(true) = aria-pressed): accent fill, icon on-accent. Disabled: looks
// idle, does not react, but keeps the focus and the tooltip (it explains why it is off).
class IconButton : public Clickable {
public:
    explicit IconButton(std::string icon = std::string());
    void SetIcon(std::string icon) { icon_ = std::move(icon); Invalidate(); }
    // The pressed look as a state (a toggle button like the Control bar's pause): shown as a toggle to
    // accessibility. SetPressed is how the page shows the host's state.
    void SetPressed(bool pressed);
    bool Pressed() const { return pressed_; }
    // The idle glyph in accent instead of `line` (the About page's itch and GitHub buttons: `color:
    // var(--accent)` on an .icon-btn). Hover and press look as ever.
    void SetIdleAccent(bool accent) { idleAccent_ = accent; Invalidate(); }
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

protected:
    float VisualTarget() const override;

private:
    std::string icon_;
    bool pressed_ = false;
    bool toggle_ = false;
    bool idleAccent_ = false;
};

// -------------------------------------------------------------------------------- WindowButton ---
// K10: the window's own minimise and close buttons (the Settings window has no Windows title bar). An icon
// of 24 in a 28 x 28 hit area, with no tile: only the colour of the icon changes. Idle: the `tile` token (a
// quiet warm grey in both themes). Hover and pressed: `accent` (#6E504A in light). Keyboard focus: the normal
// ring. It is not an IconButton because an IconButton's look is a tile fill and a 22 icon; this one has neither.
class WindowButton : public Clickable {
public:
    explicit WindowButton(std::string icon = std::string());
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;
    // The side of the hit area and of the icon inside it (DIPs): layout.h places the icon box (kIcon), the hit
    // area is 2 larger on each side (kHit).
    static constexpr float kHit = 28, kIcon = 24;

private:
    std::string icon_;
};

// -------------------------------------------------------------------------------- TextLink ---
// An icon in accent and a word in ink, 7 apart. Hover underlines the word. The current page's word is
// bold, and the width of the bold word is reserved, so a menu of links never shifts. Small: 13 px,
// always underlined. Disabled: opacity 0.55. Also the bottom-left NavLinks (SetCurrent).
class TextLink : public Clickable {
public:
    explicit TextLink(std::string text = std::string(), std::string icon = std::string());
    void SetText(std::string text) { text_ = std::move(text); SetName(text_); Invalidate(); }
    void SetIcon(std::string icon) { icon_ = std::move(icon); Invalidate(); }
    void SetCurrent(bool current) { current_ = current; Invalidate(); }
    bool Current() const { return current_; }
    // A link of the frame that picks a page: accessibility shows it as selectable (the current page is the
    // selected one). Other links (Report, About) are plain links.
    void SetNav(bool nav) { nav_ = nav; }
    bool AccSelect() override { return AccInvoke(); }
    void SetSmall(bool isSmall) { small_ = isSmall; Invalidate(); }  // not `small`: windows.h defines it as a macro
    void SetIconSize(float s) { iconSize_ = s; Invalidate(); }
    // The size of the word (the frame's links are 24 / 16 / 18 px, the About page's inline link 10 px);
    // 0 = the default of the link (16, or 13 when small). The gap between icon and word: default 7.
    void SetFontSize(float size) { fontSize_ = size; Invalidate(); }
    void SetGap(float gap) { gap_ = gap; Invalidate(); }
    // The width of the bold word is reserved by default, so a row of NavLinks does not shift when the current
    // one turns bold. Links that are never bold (the Report page's, the inline one) turn that off.
    void SetReserveBold(bool reserve) { reserveBold_ = reserve; Invalidate(); }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    float FontSize() const { return fontSize_ > 0 ? fontSize_ : (small_ ? 13.0f : 16.0f); }
    std::string text_, icon_;
    bool current_ = false;
    bool nav_ = false;
    bool small_ = false;
    float iconSize_ = 22;
    float fontSize_ = 0;
    float gap_ = 7;  // the design's `gap: 7px` between the icon and the word
    bool reserveBold_ = true;
};
using NavLink = TextLink;

// ---------------------------------------------------------------------------------- Button ---
// Min height 34, padding 0 18, radius 4, 16 bold; accent fill with on-accent text. Quiet: `line` fill
// with accent text. Small: 28 tall, padding 12, 14 px. Disabled: `line` fill, accent text, opacity 0.7;
// a disabled button is not focusable (like <button disabled>).
class Button : public Clickable {
public:
    explicit Button(std::string text = std::string());
    void SetText(std::string text) { text_ = std::move(text); SetName(text_); Invalidate(); }
    const std::string& Text() const { return text_; }
    void SetQuiet(bool quiet) { quiet_ = quiet; Invalidate(); }
    void SetSmall(bool isSmall) { small_ = isSmall; Invalidate(); }  // not `small`: windows.h defines it as a macro
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    TextStyle Style() const;
    std::string text_;
    bool quiet_ = false;
    bool small_ = false;
};

// -------------------------------------------------------------------------------- SwitchRow ---
// A label, right-aligned, and a pill 22 x 12 (1.5 accent border, radius 6) with a knob ring 12 (1.5
// accent border) at x -1.5 (off) or 8.5 (on). On: accent pill fill, on-accent knob fill. The label is
// underlined on hover. Disabled (a switch that cannot be used now): opacity 0.5, no hover, but it keeps
// the focus. A click calls onActivate (page: send setX {on: !checked}); it never flips by itself.
class SwitchRow : public Clickable {
public:
    explicit SwitchRow(std::string label = std::string());
    void SetLabel(std::string label) { label_ = std::move(label); SetName(label_); Invalidate(); }
    void SetChecked(bool checked);
    bool Checked() const { return checked_; }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;
    // 0 (off) .. 1 (on) as drawn now, for the tests.
    float OnAmount() const { return TweenValue(on_); }
    // K9: the Settings page shows a card for the switch that is selected. `SetOnReach` is called when the pointer
    // comes onto the row or the focus comes to it (the page selects it, like a KeyRow); a selected row is drawn
    // in bold, as the selected Key is.
    void SetOnReach(std::function<void()> fn) { onReach_ = std::move(fn); }
    void SetSelected(bool selected);
    bool Selected() const { return selected_; }

protected:
    void StateChanged() override;

private:
    std::string label_;
    bool checked_ = false;
    Tween on_;
    bool selected_ = false;
    bool wasHovered_ = false, wasFocused_ = false;  // to see which flag just changed
    std::function<void()> onReach_;
};

// ---------------------------------------------------------------------------------- KeyChip ---
// A key on an accent tag: min width 70, 24 tall, padding 0 8, radius 2, 17 bold. Set: accent fill,
// on-accent text. None: `line` fill, accent text. Capturing (waiting for the next key): card fill,
// accent text, 2 DIP accent outline inside. While it is capturing and has the focus it takes every key
// (Tab and Esc too) and hands each to onKey; the page decides (set the key, cancel).
class KeyChip : public Clickable {
public:
    enum class State { None, Set, Capturing };
    explicit KeyChip(std::string text = std::string());
    void SetText(std::string text) { text_ = std::move(text); Invalidate(); }
    void SetState(State s);
    State GetState() const { return state_; }
    void SetOnKey(std::function<void(const KeyEvent&)> fn) { onKey_ = std::move(fn); }
    // Called when the pointer comes onto the chip or the focus comes to it (the page selects the row).
    void SetOnReach(std::function<void()> fn) { onReach_ = std::move(fn); }
    // Called when the chip loses the focus while it is Capturing (a press elsewhere, Tab, the window
    // going inactive, the page being replaced): the capture is over. The state is still Capturing then.
    void SetOnBlur(std::function<void()> fn) { onBlur_ = std::move(fn); }
    bool OnKey(const KeyEvent& ev) override;
    bool CapturesAllKeys() const override { return state_ == State::Capturing; }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

protected:
    void StateChanged() override;

private:
    std::string text_;
    State state_ = State::None;
    std::function<void(const KeyEvent&)> onKey_;
    std::function<void()> onReach_, onBlur_;
    bool wasHovered_ = false, wasFocused_ = false;  // to see which flag just changed
};

}  // namespace nui

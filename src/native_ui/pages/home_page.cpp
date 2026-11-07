#include "native_ui/pages/home_page.h"

#include <algorithm>
#include <cmath>

#include "native_ui/pages/layout.h"
#include "native_ui/screen.h"
#include "strings/string_ids.h"

namespace nui {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kButtonRadius = 4;

}  // namespace

// ------------------------------------------------------------------------------ PowerButton ---

PowerButton::PowerButton() {
    SetRole(Role::Button);
    SetBounds(layout::kPower);
}

void PowerButton::SetOn(bool on) {
    on_ = on;
    MoveTween(onTween_, on ? 1.0f : 0.0f);  // 0.2 s fade, but not before the first draw (Screen rule)
    Invalidate();
}

Widget* PowerButton::HitTest(PointF p) {
    if (!Visible() || !Abs().Contains(p)) return nullptr;
    // The button of the old page was a circle (border-radius 50%): a press in a corner of its box is the bee's.
    const RectF r = Abs();
    const float dx = p.x - (r.x + r.w / 2), dy = p.y - (r.y + r.h / 2);
    const float rad = r.w / 2;
    return dx * dx + dy * dy <= rad * rad ? this : nullptr;
}

void PowerButton::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const float on = OnAmount();
    // Hover only: the CSS has no pressed look for the power circle, and a disabled one has no hover.
    const float hover = std::min(1.0f, VisualAmount());
    const Colour rest = Mix(p.paper, p.accent, on);
    const Colour hovered = Mix(p.powerOffHover, p.powerOnHover, on);
    canvas.FillEllipse(r, Mix(rest, hovered, hover));
    const Colour glyph = Mix(p.accent, p.onAccent, on);

    // The old SVG (viewBox 64): a stem M32 7 v23 and an arc from (18.6, 13.2) the long way round to
    // (45.4, 13.2) with radius 24. The SVG's own centre for that arc is a little below (32, 32): half the
    // chord is 13.4, so the centre is sqrt(24^2 - 13.4^2) below the chord.
    const float s = layout::kPowerGlyphSize / layout::kPowerGlyphViewBox;
    const float ox = r.x + (r.w - layout::kPowerGlyphSize) / 2, oy = r.y + (r.h - layout::kPowerGlyphSize) / 2;
    const float halfChord = 13.4f, chordY = 13.2f, radius = 24.0f;
    const float centreY = chordY + std::sqrt(radius * radius - halfChord * halfChord);
    // The start (top right) is at angle atan2(-(centreY - chordY), halfChord); the arc runs clockwise on the
    // screen from there, through the bottom, to the same height on the left.
    const float startDeg = std::atan2(-(centreY - chordY), halfChord) * 180.0f / kPi;
    const float endDeg = 180.0f - startDeg;  // the mirror point, top left (360 - 123.9 = 236.1)
    const float sweepDeg = endDeg - startDeg;
    const float w = layout::kPowerGlyphStroke * s;
    canvas.Arc({ox + 32 * s, oy + centreY * s}, radius * s, startDeg, sweepDeg, glyph, w);
    canvas.Line({ox + 32 * s, oy + 7 * s}, {ox + 32 * s, oy + 30 * s}, glyph, w);

    if (RingVisible()) DrawFocusRing(canvas, p, r, r.w / 2);
}

AccessInfo PowerButton::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Button;
    a.toggle = on_ ? Toggle::On : Toggle::Off;  // aria-pressed
    return a;
}

// ----------------------------------------------------------------------------- EnableButton ---

EnableButton::EnableButton() {
    SetRole(Role::Button);
    SetBounds(layout::kEnable);
}

void EnableButton::SetOn(bool on) {
    on_ = on;
    MoveTween(onTween_, on ? 1.0f : 0.0f);
    Invalidate();
}

void EnableButton::SetLabel(std::string text) {
    label_ = std::move(text);
    SetName(label_);
    Invalidate();
}

void EnableButton::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const float on = OnAmount();
    canvas.FillRect(r, Mix(p.accent, p.line, on), kButtonRadius);
    TextStyle st;
    st.size = layout::kEnableFont;
    st.weight = FontWeight::Bold;
    st.align = TextAlign::Centre;
    canvas.Text(r, label_, st, Mix(p.onAccent, p.accent, on));
    if (RingVisible()) DrawFocusRing(canvas, p, r, kButtonRadius);
}

AccessInfo EnableButton::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Button;
    return a;
}

// --------------------------------------------------------------------------------- HomePage ---

HomePage::HomePage(const PageEnv& env) : PageView(Page::Home, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavHome));

    bee_ = Add<PictureWidget>("bee", PictureTint::BeeOff);
    bee_->SetBounds(layout::kBee);
    bee_->SetOpacity(layout::kBeeOpacity);

    power_ = Add<PowerButton>();
    power_->SetName(T(strings::id::kWindowHomePower));
    power_->SetEnabled(false);  // until the first state: there is nothing to toggle
    power_->SetOnActivate([this] { Toggle(); });

    enable_ = Add<EnableButton>();
    enable_->SetEnabled(false);
    enable_->SetOnActivate([this] { Toggle(); });
}

void HomePage::Toggle() {
    if (!haveState_) return;
    PageMessage m = MakeMessage(PageMessageType::SetOn, "setOn");
    m.on = !on_;  // the page keeps no copy of a setting: the opposite of what the last state says
    Emit(m);
}

void HomePage::SetState(const UiState& state) {
    haveState_ = true;
    on_ = state.on;
    power_->SetEnabled(true);
    power_->SetOn(on_);
    enable_->SetEnabled(true);
    enable_->SetOn(on_);
    enable_->SetLabel(T(on_ ? strings::id::kWindowHomeDisable : strings::id::kWindowHomeEnable));
    bee_->SetTint(on_ ? PictureTint::BeeOn : PictureTint::BeeOff);
}

}  // namespace nui

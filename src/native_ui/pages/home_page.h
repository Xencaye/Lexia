#pragma once
// The Home page (K.2): the bee engraving, the power circle in it and the enable / disable button under it.
// Both turn Lexia On or Off by sending setOn with the opposite of what the state says; both are disabled
// until the first state comes. Ported from ui/css/pages.css `.bee` `.power` `.enable` and ui/js/app.js.
#include "native_ui/controls.h"
#include "native_ui/pages/page.h"

namespace nui {

// The power circle: 124 across, paper fill with the accent glyph when Off, accent fill with the on-accent
// glyph when On, a hover fill for each (palette powerOffHover / powerOnHover); 0.2 s between the looks.
// The glyph is the old SVG drawn with an arc and a line (viewBox 64, stroke 4.4, scaled to 104).
class PowerButton : public Clickable {
public:
    PowerButton();
    void SetOn(bool on);
    bool On() const { return on_; }
    float OnAmount() const { return TweenValue(onTween_); }
    void Paint(Canvas& canvas, const Palette& palette) override;
    Widget* HitTest(PointF p) override;  // the circle, not its bounding box
    AccessInfo Access() const override;

private:
    bool on_ = false;
    Tween onTween_{0, kPowerSeconds};
};

// The button under the circle: accent fill and on-accent word while Off ("enable"); `line` fill and accent
// word while On ("disable"). Same size in both looks, 22 px bold.
class EnableButton : public Clickable {
public:
    EnableButton();
    void SetOn(bool on);
    void SetLabel(std::string text);  // the word; set with the state
    const std::string& Label() const { return label_; }
    float OnAmount() const { return TweenValue(onTween_); }
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    bool on_ = false;
    std::string label_;
    Tween onTween_{0, kPowerSeconds};
};

class HomePage : public PageView {
public:
    explicit HomePage(const PageEnv& env);
    void SetState(const UiState& state) override;

    PowerButton* Power() const { return power_; }
    EnableButton* Enable() const { return enable_; }
    PictureWidget* Bee() const { return bee_; }

private:
    void Toggle();  // sends setOn {on: !state.on}

    PictureWidget* bee_ = nullptr;
    PowerButton* power_ = nullptr;
    EnableButton* enable_ = nullptr;
    bool haveState_ = false;
    bool on_ = false;
};

}  // namespace nui

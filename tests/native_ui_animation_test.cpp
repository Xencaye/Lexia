// Transitions of the native Settings window (K1.b): the Tween maths, and the rules of the Screen that
// switch them off (before the first draw, reduced motion).
#include "native_ui/controls.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

using namespace nui;
using namespace nui::test;

TEST_CASE("Tween: jumps without animation, eases with it") {
    Tween t(0);
    t.SetTarget(1, 10.0, false);
    CHECK(t.Value(10.0) == 1.0f);
    CHECK_FALSE(t.Running(10.0));

    Tween u(0);
    u.SetTarget(1, 10.0, true);
    CHECK(u.Value(10.0) == 0.0f);                                // starts where it was
    CHECK(u.Value(10.0 + kTransitionSeconds / 2) == doctest::Approx(0.5f));  // smoothstep: middle = middle
    CHECK(u.Value(10.0 + kTransitionSeconds) == 1.0f);
    CHECK(u.Value(99.0) == 1.0f);
    CHECK(u.Running(10.05));
    CHECK_FALSE(u.Running(10.0 + kTransitionSeconds));
    CHECK(u.EndTime() == doctest::Approx(10.0 + kTransitionSeconds));
}

TEST_CASE("Tween: the same target keeps the running transition, a reversal starts from the shown value") {
    Tween t(0);
    t.SetTarget(1, 0.0, true);
    const float mid = t.Value(0.06);
    t.SetTarget(1, 0.06, true);  // same target: nothing restarts
    CHECK(t.Value(0.06) == doctest::Approx(mid));
    CHECK(t.EndTime() == doctest::Approx(kTransitionSeconds));

    t.SetTarget(0, 0.06, true);  // back: from the value on screen, not from 1
    CHECK(t.Value(0.06) == doctest::Approx(mid));
    CHECK(t.Value(0.06 + kTransitionSeconds) == 0.0f);
}

TEST_CASE("Tween: the power circle takes 0.2 s") {
    Tween t(0, kPowerSeconds);
    t.SetTarget(1, 0.0, true);
    CHECK(t.Running(0.15));
    CHECK_FALSE(t.Running(0.2));
}

TEST_CASE("Mix and Ramp interpolate colours in sRGB") {
    const Colour a{0x000000, 0.0f}, b{0xFF8040, 1.0f};
    const Colour m = Mix(a, b, 0.5f);
    CHECK(m.rgb == 0x804020u);
    CHECK(m.alpha == doctest::Approx(0.5f));
    CHECK(Mix(a, b, 5.0f).rgb == b.rgb);  // held to 0..1
    const Colour c{0x0000FF, 1.0f};
    CHECK(Ramp(a, b, c, 0.0f).rgb == a.rgb);
    CHECK(Ramp(a, b, c, 1.0f).rgb == b.rgb);
    CHECK(Ramp(a, b, c, 2.0f).rgb == c.rgb);
}

TEST_CASE("no transition before the first state is drawn") {
    Ui ui;
    auto* b = ui.screen.Root().Add<IconButton>("home-05");
    b->SetPosition(10, 10);
    ui.screen.Tick(0);
    ui.Move(20, 20);  // hover before anything was painted
    CHECK(b->VisualAmount() == 1.0f);          // already at the end
    CHECK_FALSE(ui.screen.NeedsTick());
}

TEST_CASE("after the first draw a hover fades in 0.12 s") {
    Ui ui;
    auto* b = ui.screen.Root().Add<IconButton>("home-05");
    b->SetPosition(10, 10);
    ui.Frame();  // the first draw
    CHECK(ui.screen.AnimationsOn());
    CHECK(b->VisualAmount() == 0.0f);

    ui.Move(20, 20);
    CHECK(b->VisualAmount() == 0.0f);                      // starts now
    CHECK(ui.screen.NeedsTick());
    ui.screen.Tick(0.06);
    CHECK(b->VisualAmount() == doctest::Approx(0.5f));
    CHECK(ui.screen.NeedsRepaint());
    ui.screen.Tick(0.2);                                   // past the end
    CHECK(b->VisualAmount() == 1.0f);
    CHECK(ui.screen.NeedsRepaint());                       // one more repaint to show the end value
    ui.now = 0.3;
    ui.Frame();
    CHECK_FALSE(ui.screen.NeedsRepaint());                 // drawn, nothing moves any more
    CHECK_FALSE(ui.screen.NeedsTick());
}

TEST_CASE("reduced motion jumps") {
    Ui ui;
    auto* b = ui.screen.Root().Add<IconButton>("home-05");
    b->SetPosition(10, 10);
    ui.Frame();
    ui.screen.SetReducedMotion(true);
    CHECK_FALSE(ui.screen.AnimationsOn());
    ui.Move(20, 20);
    CHECK(b->VisualAmount() == 1.0f);
    CHECK_FALSE(ui.screen.NeedsTick());
}

TEST_CASE("SkipAnimations starts the not-yet-drawn phase again (a page switch)") {
    Ui ui;
    ui.Frame();
    CHECK(ui.screen.AnimationsOn());
    ui.screen.SkipAnimations();
    CHECK_FALSE(ui.screen.AnimationsOn());
    ui.Frame();
    CHECK(ui.screen.AnimationsOn());
}

TEST_CASE("the switch knob and the slider thumb use the same 0.12 s") {
    Ui ui;
    auto* sw = ui.screen.Root().Add<SwitchRow>("Run at startup");
    sw->SetBounds({100, 100, 300, 28});
    ui.Frame();
    sw->SetChecked(true);
    CHECK(sw->OnAmount() == 0.0f);
    ui.screen.Tick(0.06);
    CHECK(sw->OnAmount() == doctest::Approx(0.5f));
    ui.screen.Tick(0.12);
    CHECK(sw->OnAmount() == 1.0f);
}

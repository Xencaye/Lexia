// Transitions of the native Settings window (K1.b): the Tween maths, and the rules of the Screen that
// switch them off (before the first draw, reduced motion).
#include "native_ui/controls.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

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

// ---------------------------------------------------------------------------------- wiggle ---

TEST_CASE("Wiggle: still for the first period, then 0.6 s of shake every 5 s") {
    CHECK(kWigglePeriodSeconds == 5.0);
    CHECK(kWiggleSeconds == doctest::Approx(0.6));
    // Nothing before the first period, however the time is counted.
    for (double t : {0.0, 0.5, 2.5, 4.99}) {
        CHECK_FALSE(WiggleMoving(t));
        CHECK(WiggleAngle(t) == 0.0f);
    }
    // The first wiggle: 5.0 .. 5.6. It starts and ends at rest, and in between it swings both ways.
    CHECK(WiggleMoving(5.0));
    CHECK(WiggleAngle(5.0) == doctest::Approx(0.0f));
    CHECK(WiggleAngle(5.0 + 0.075) == doctest::Approx(7.0f));  // a quarter of a swing in: (1 - 1/8) * 8 degrees
    float lo = 0, hi = 0;
    for (double t = 5.0; t < 5.6; t += 0.005) {
        lo = std::min(lo, WiggleAngle(t));
        hi = std::max(hi, WiggleAngle(t));
        CHECK(std::abs(WiggleAngle(t)) <= kWiggleDegrees);
    }
    CHECK(lo < -3.0f);
    CHECK(hi > 3.0f);
    // The shake dies away: the last swing is smaller than the first.
    CHECK(std::abs(WiggleAngle(5.0 + 0.6 * 0.875)) < std::abs(WiggleAngle(5.0 + 0.6 * 0.125)));
    CHECK_FALSE(WiggleMoving(5.61));  // not 5.6: 5.6 mod 5 is a hair under 0.6 in floating point
    CHECK(WiggleAngle(5.9) == 0.0f);
    CHECK(WiggleAngle(9.99) == 0.0f);
    // And again at 10 and at every multiple of 5 after it.
    CHECK(WiggleMoving(10.1));
    CHECK(WiggleAngle(10.075) == doctest::Approx(7.0f));
    CHECK(WiggleMoving(5.0 * 40 + 0.3));
}

TEST_CASE("Wiggle: the end of the one that moves, and the start of the next") {
    CHECK(WiggleNextStart(0.0) == 5.0);  // never before the first period
    CHECK(WiggleNextStart(4.99) == 5.0);
    CHECK(WiggleNextStart(5.0) == 10.0);  // later than `elapsed`, always
    CHECK(WiggleNextStart(5.3) == 10.0);
    CHECK(WiggleNextStart(12.0) == 15.0);
    CHECK(WiggleEnd(5.3) == doctest::Approx(5.6));
    CHECK(WiggleEnd(10.0) == doctest::Approx(10.6));
}

TEST_CASE("Screen: a wake-up is asked for at a future time, needs no tick, and is reached by a Tick") {
    Ui ui;
    ui.Frame();
    CHECK_FALSE(ui.screen.NextWakeTime().has_value());
    ui.screen.NoteWakeAt(5.0);
    ui.screen.NoteWakeAt(8.0);  // the earliest wins
    ui.screen.NoteWakeAt(0.0);  // not ahead of the clock: ignored
    REQUIRE(ui.screen.NextWakeTime().has_value());
    CHECK(*ui.screen.NextWakeTime() == 5.0);
    CHECK_FALSE(ui.screen.NeedsTick());  // a wake-up is not a reason for a frame timer

    ui.screen.Tick(4.0);
    CHECK_FALSE(ui.screen.NeedsRepaint());  // not reached yet (Frame cleared the flag)
    CHECK(ui.screen.NextWakeTime().has_value());
    ui.screen.Tick(5.0);
    CHECK(ui.screen.NeedsRepaint());  // reached: the widget gets a paint to move or to note its next one
    CHECK_FALSE(ui.screen.NextWakeTime().has_value());

    // A paint forgets the wake-ups of the paint before (a widget that is not drawn any more asks for none).
    ui.screen.NoteWakeAt(9.0);
    ui.Frame();
    CHECK_FALSE(ui.screen.NextWakeTime().has_value());
}

// Slider and Select of the native Settings window (K1.b): the slider maths (linear and log2), snap and
// clamp, keys, drag, look; the select's popup list, its keyboard and mouse.
#include "native_ui/select.h"
#include "native_ui/slider.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace nui;
using namespace nui::test;

// ------------------------------------------------------------------------------ the maths ---

TEST_CASE("slider maths: linear position, value, snap, clamp") {
    const SliderRange r{0, 100, 50, 5, false};
    CHECK(SliderPos(r, 50) == doctest::Approx(0.5));
    CHECK(SliderPos(r, 0) == 0.0);
    CHECK(SliderPos(r, 100) == 1.0);
    CHECK(SliderValueAt(r, 0.25) == doctest::Approx(25));
    CHECK(SliderSnap(r, 23) == 25);
    CHECK(SliderSnap(r, 22) == 20);
    CHECK(SliderSnap(r, -40) == 0);    // clamped
    CHECK(SliderSnap(r, 1000) == 100);
}

TEST_CASE("slider maths: the step grid starts at min, not at zero") {
    const SliderRange r{0.5, 2.0, 1.0, 0.25, false};
    CHECK(SliderSnap(r, 1.1) == 1.0);
    CHECK(SliderSnap(r, 1.2) == 1.25);
}

TEST_CASE("slider maths: log2 mode puts 1.0 in the middle of 0.5 .. 2") {
    const SliderRange r{0.5, 2.0, 1.0, 0.05, true};
    CHECK(SliderPos(r, 0.5) == doctest::Approx(0));
    CHECK(SliderPos(r, 1.0) == doctest::Approx(0.5));
    CHECK(SliderPos(r, 2.0) == doctest::Approx(1));
    CHECK(SliderPos(r, std::sqrt(2.0)) == doctest::Approx(0.75));
    CHECK(SliderValueAt(r, 0.5) == doctest::Approx(1.0));
    CHECK(SliderValueAt(r, 0.0) == doctest::Approx(0.5));
    CHECK(SliderValueAt(r, 1.0) == doctest::Approx(2.0));
    // The middle of the box snaps to exactly 1 on the 0.05 grid.
    CHECK(SliderSnap(r, SliderValueAt(r, 0.5)) == 1.0);
    CHECK(SliderSnap(r, 1.234) == 1.25);
    // The same range on a linear scale has 1.0 not in the middle.
    SliderRange lin = r;
    lin.log = false;
    CHECK(SliderPos(lin, 1.0) == doctest::Approx(1.0 / 3.0));
}

TEST_CASE("slider maths: what counts as a usable range") {
    CHECK(SliderUsable({0, 1, 0.5, 0.1, false}));
    CHECK_FALSE(SliderUsable({1, 1, 1, 0.1, false}));             // min == max
    CHECK_FALSE(SliderUsable({0, 1, 0.5, 0, false}));             // no step
    CHECK_FALSE(SliderUsable({0, 1, std::nan(""), 0.1, false}));  // not a number
    CHECK_FALSE(SliderUsable({0, 1, 0.5, 0.1, true}));            // a log scale needs min > 0
    CHECK(SliderUsable({0.5, 2, 1, 0.1, true}));
}

// --------------------------------------------------------------------------------- Slider ---

namespace {

struct SliderFixture {
    Ui ui;
    Slider* s = nullptr;
    std::vector<double> asked;
    explicit SliderFixture(SliderRange r = {0, 10, 5, 1, false}) {
        s = ui.screen.Root().Add<Slider>();
        s->SetBounds({100, 100, 200, 28});
        s->SetRange(r);
        s->SetOnValue([this](double v) { asked.push_back(v); });
    }
};

}  // namespace

TEST_CASE("Slider: Right and Up add a step, Left and Down take one") {
    SliderFixture f({0, 100, 50, 1, false});
    f.s->Focus();
    CHECK(f.ui.Key(Key::Right));
    CHECK(f.asked.back() == 51);
    f.s->SetRange({0, 100, 51, 1, false});
    f.ui.Key(Key::Up);
    CHECK(f.asked.back() == 52);
    f.s->SetRange({0, 100, 52, 1, false});
    f.ui.Key(Key::Left);
    CHECK(f.asked.back() == 51);
    f.s->SetRange({0, 100, 51, 1, false});
    f.ui.Key(Key::Down);
    CHECK(f.asked.back() == 50);
    CHECK(f.asked.size() == 4);
}

TEST_CASE("Slider: a value is asked once until the state changes") {
    SliderFixture f;
    f.s->Focus();
    f.ui.Key(Key::Right);
    f.ui.Key(Key::Right);  // still 5 in the state: asks 6 again, and that is the same as the last ask
    REQUIRE(f.asked.size() == 1);
    CHECK(f.asked[0] == 6);
    f.s->SetRange({0, 10, 6, 1, false});  // the state came back
    f.ui.Key(Key::Right);
    REQUIRE(f.asked.size() == 2);
    CHECK(f.asked[1] == 7);
}

TEST_CASE("Slider: keys, with the ends held in the range") {
    SliderFixture f;
    f.s->Focus();
    f.ui.Key(Key::Left);
    CHECK(f.asked.back() == 4);
    f.ui.Key(Key::PageUp);  // 15 -> held at 10
    CHECK(f.asked.back() == 10);
    f.ui.Key(Key::PageDown);  // -5 -> held at 0
    CHECK(f.asked.back() == 0);
    f.ui.Key(Key::Home);
    f.ui.Key(Key::End);
    CHECK(f.asked.back() == 10);
    CHECK(f.asked.size() == 4);  // Home asks 0 again: the same as the last ask, so no message
    f.s->SetRange({0, 10, 10, 1, false});
    f.ui.Key(Key::End);  // already at the end
    CHECK(f.asked.size() == 4);
    f.ui.Key(Key::Other);
    CHECK(f.asked.size() == 4);
}

TEST_CASE("Slider: log2 mode keys still step in value space") {
    SliderFixture f({0.5, 2.0, 1.0, 0.05, true});
    f.s->Focus();
    f.ui.Key(Key::Right);
    CHECK(f.asked.back() == doctest::Approx(1.05));
    f.s->SetRange({0.5, 2.0, 1.0, 0.05, true});
    f.ui.Key(Key::PageDown);
    CHECK(f.asked.back() == doctest::Approx(0.5));  // 1.0 - 0.5 = 0.5
}

TEST_CASE("Slider: a press sets the value under the pointer, on the log scale too") {
    SliderFixture f({0.5, 2.0, 1.0, 0.05, true});
    f.ui.Down(100 + 150, 110);  // 75 % of the box
    REQUIRE(f.asked.size() == 1);
    CHECK(f.asked[0] == doctest::Approx(1.4));  // 0.5 * 4^0.75 = 1.414 -> grid 1.4
    f.ui.Up(250, 110);
    f.s->SetRange({0.5, 2.0, 1.4, 0.05, true});
    f.ui.Down(100, 110);  // the left end
    CHECK(f.asked.back() == doctest::Approx(0.5));
}

TEST_CASE("Slider: pressing at the value that is shown asks for nothing") {
    SliderFixture f;
    f.ui.Down(200, 110);  // the middle: 5
    CHECK(f.asked.empty());
}

TEST_CASE("Slider: the thumb is 6 x 14, grows to 12 x 25 on hover and focus, and is centred on the value") {
    SliderFixture f;
    f.ui.Frame();
    RectF t = f.s->ThumbRect();
    CHECK(t.w == 6);
    CHECK(t.h == 14);
    CHECK(t.x + t.w / 2 == doctest::Approx(200));  // value 5 of 0..10
    CHECK(t.y + t.h / 2 == doctest::Approx(114));

    f.ui.Move(150, 110);
    f.ui.Advance(0.5);
    t = f.s->ThumbRect();
    CHECK(t.w == doctest::Approx(12));
    CHECK(t.h == doctest::Approx(25));
    // The painted thumb and track.
    const auto fills = f.ui.canvas.Of(Call::FillRect);
    REQUIRE(fills.size() == 2);
    CHECK(fills[0]->colour.rgb == f.ui.pal.line.rgb);
    CHECK(fills[0]->rect.h == 2);
    CHECK(fills[1]->colour.rgb == f.ui.pal.accent.rgb);

    f.ui.Leave();
    f.ui.Advance(0.5);
    CHECK(f.s->ThumbRect().w == doctest::Approx(6));

    // By keyboard: the focus grows it and rings it.
    f.ui.Key(Key::Tab);
    f.ui.Advance(0.5);
    CHECK(f.s->ThumbRect().w == doctest::Approx(12));
    const auto rings = f.ui.canvas.Of(Call::StrokeRect);
    REQUIRE(rings.size() == 1);
    CHECK(rings[0]->number == 2);
    CHECK(rings[0]->rect.w == doctest::Approx(12 + 6));
}

TEST_CASE("Slider: while dragging the thumb stays grown; the thumb follows only the state") {
    SliderFixture f;
    f.ui.Frame();
    f.ui.Down(250, 110);
    f.ui.Advance(0.5);
    CHECK(f.s->ThumbRect().w == doctest::Approx(12));
    // The value in the state is still 5: the thumb does not move to the pointer by itself.
    CHECK(f.s->ThumbRect().x + 6 == doctest::Approx(200));
    f.s->SetRange({0, 10, 8, 1, false});
    CHECK(f.s->ThumbRect().x + 6 == doctest::Approx(100 + 160));
}

TEST_CASE("Slider: without a state it is disabled, faded and not focusable") {
    SliderFixture f;
    f.s->Clear();
    f.ui.Frame();
    CHECK(f.ui.canvas.HasOpacity(0.5f));
    f.ui.Key(Key::Tab);
    CHECK(f.ui.screen.Focused() == nullptr);
    f.ui.Down(150, 110);
    CHECK(f.asked.empty());
    CHECK_FALSE(f.s->Access().enabled);
    CHECK_FALSE(f.s->Access().hasRange);
    CHECK(f.ui.screen.Cursor() == CursorHint::Arrow);
}

TEST_CASE("Slider: accessibility data") {
    SliderFixture f({0.5, 2.0, 1.25, 0.05, true});
    f.s->SetName("Speed");
    f.s->SetValueText("1.25x");
    const AccessInfo a = f.s->Access();
    CHECK(a.role == Role::Slider);
    CHECK(a.name == "Speed");
    CHECK(a.value == "1.25x");
    CHECK(a.hasRange);
    CHECK(a.min == 0.5);
    CHECK(a.max == 2.0);
    CHECK(a.current == 1.25);
    CHECK(a.step == 0.05);
    CHECK(a.focusable);
}

// ---------------------------------------------------------------------------------- Select ---

namespace {

struct SelectFixture {
    Ui ui;
    Select* sel = nullptr;
    std::vector<int> chosen;
    explicit SelectFixture(float y = 100) {
        sel = ui.screen.Root().Add<Select>();
        sel->SetBounds({100, y, 200, 28});
        sel->SetItems({"Anna", "Brian", "Clara"});
        sel->SetSelected(1);
        sel->SetOnChoose([this](int i) { chosen.push_back(i); });
    }
    Widget* Popup() { return ui.screen.Popup(); }
    // The centre of row i of the open list.
    PointF Row(int i) { return Centre(*Popup()->Children()[static_cast<size_t>(i)]); }
};

}  // namespace

TEST_CASE("Select: closed it shows the choice and a chevron on a card with a line border") {
    SelectFixture f;
    f.ui.Frame();
    const auto fills = f.ui.canvas.Of(Call::FillRect);
    REQUIRE(fills.size() == 1);
    CHECK(fills[0]->colour.rgb == f.ui.pal.card.rgb);
    CHECK(fills[0]->radius == 1);
    const auto borders = f.ui.canvas.Of(Call::StrokeRect);
    REQUIRE(borders.size() == 1);
    CHECK(borders[0]->colour.rgb == f.ui.pal.line.rgb);
    CHECK(borders[0]->number == 1);
    const Call* text = f.ui.canvas.FindText("Brian");
    REQUIRE(text);
    CHECK(text->colour.rgb == f.ui.pal.ink.rgb);
    CHECK(text->rect.x == 100 + 9);
    const auto chevron = f.ui.canvas.Of(Call::Polyline);
    REQUIRE(chevron.size() == 1);
    CHECK(chevron[0]->colour.rgb == f.ui.pal.ink.rgb);
    CHECK(chevron[0]->points.size() == 3);
    // 16 wide, 9 from the right edge.
    CHECK(chevron[0]->points[2].x < 300 - 9);
    CHECK(chevron[0]->points[0].x > 300 - 9 - 16);
    CHECK(f.sel->Access().role == Role::ComboBox);
    CHECK(f.sel->Access().value == "Brian");
    CHECK(f.sel->Access().expanded == Tri::No);
}

TEST_CASE("Select: a press opens the list under it; the rows are list items") {
    SelectFixture f;
    f.ui.Frame();
    f.ui.Down(150, 110);
    REQUIRE(f.Popup() != nullptr);
    CHECK(f.sel->IsOpen());
    CHECK(f.sel->Access().expanded == Tri::Yes);
    const RectF r = f.Popup()->Abs();
    CHECK(r.x == 100);
    CHECK(r.y == 128);
    CHECK(r.w == 200);
    CHECK(r.h == 3 * 28 + 2);
    REQUIRE(f.Popup()->Children().size() == 3);
    const AccessInfo row1 = f.Popup()->Children()[1]->Access();
    CHECK(row1.role == Role::ListItem);
    CHECK(row1.name == "Brian");
    CHECK(row1.selected);
    CHECK_FALSE(f.Popup()->Children()[0]->Access().selected);
    CHECK(f.Popup()->Access().role == Role::List);
    f.ui.Frame();  // draws with the popup on top
    CHECK(f.ui.canvas.FindText("Clara"));
}

TEST_CASE("Select: opens above when there is no room below") {
    SelectFixture f(560);
    f.ui.Down(150, 570);
    REQUIRE(f.Popup() != nullptr);
    CHECK(f.Popup()->Abs().y == 560 - (3 * 28 + 2));
}

TEST_CASE("Select: Down and Enter choose the next item; the select shows the old one until the state") {
    SelectFixture f;
    f.sel->Focus();
    CHECK(f.ui.Key(Key::Down));  // closed: opens the list
    REQUIRE(f.Popup());
    CHECK(f.chosen.empty());
    CHECK(f.ui.Key(Key::Down));  // highlight starts at the choice (1), moves to 2
    CHECK(f.ui.Key(Key::Down));  // the end holds
    CHECK(f.ui.Key(Key::Enter));
    REQUIRE(f.chosen.size() == 1);
    CHECK(f.chosen[0] == 2);
    CHECK(f.Popup() == nullptr);                 // closed
    CHECK_FALSE(f.sel->IsOpen());
    CHECK(f.sel->Selected() == 1);               // not changed by the click
    CHECK(f.ui.screen.Focused() == f.sel);       // the focus stayed on the select
    f.sel->SetSelected(2);                       // the state came back
    CHECK(f.sel->Access().value == "Clara");
}

TEST_CASE("Select: Up, Home, End and Space inside the list") {
    SelectFixture f;
    f.sel->Focus();
    f.ui.Key(Key::Space);  // opens
    REQUIRE(f.Popup());
    f.ui.Key(Key::End);
    f.ui.Key(Key::Home);
    f.ui.Key(Key::Down);
    f.ui.Key(Key::Up);
    f.ui.Key(Key::Up);  // the top holds
    f.ui.Key(Key::Space);
    REQUIRE(f.chosen.size() == 1);
    CHECK(f.chosen[0] == 0);
}

TEST_CASE("Select: Esc closes without choosing; Tab closes and moves on") {
    SelectFixture f;
    auto* other = f.ui.screen.Root().Add<Select>();
    other->SetBounds({100, 300, 200, 28});
    other->SetItems({"x"});
    f.sel->Focus();
    f.ui.Key(Key::Enter);
    REQUIRE(f.Popup());
    f.ui.Key(Key::Escape);
    CHECK(f.Popup() == nullptr);
    CHECK(f.chosen.empty());
    CHECK(f.ui.screen.Focused() == f.sel);

    f.ui.Key(Key::Enter);
    REQUIRE(f.Popup());
    f.ui.Key(Key::Tab);
    CHECK(f.Popup() == nullptr);
    CHECK(f.ui.screen.Focused() == other);
    CHECK(f.chosen.empty());
}

TEST_CASE("Select: the pointer moves the highlight and a release on a row chooses it") {
    SelectFixture f;
    f.ui.Down(150, 110);
    f.ui.Up(150, 110);  // the press that opened it is over
    REQUIRE(f.Popup());
    const PointF r0 = f.Row(0);
    f.ui.Move(r0.x, r0.y);
    f.ui.Frame();
    // The highlighted row is filled with `line`.
    bool highlighted = false;
    for (const Call* c : f.ui.canvas.Of(Call::FillRect))
        if (c->colour.rgb == f.ui.pal.line.rgb && c->rect.h == 28) highlighted = true;
    CHECK(highlighted);
    f.ui.Down(r0.x, r0.y);
    f.ui.Up(r0.x, r0.y);
    REQUIRE(f.chosen.size() == 1);
    CHECK(f.chosen[0] == 0);
    CHECK(f.Popup() == nullptr);
    CHECK(f.ui.screen.Focused() == f.sel);
}

TEST_CASE("Select: a press outside closes the list and is used up; a press on the select closes it too") {
    SelectFixture f;
    f.ui.Down(150, 110);
    f.ui.Up(150, 110);
    REQUIRE(f.Popup());
    CHECK(f.ui.Down(600, 500));
    CHECK(f.Popup() == nullptr);
    CHECK(f.chosen.empty());

    f.ui.Up(600, 500);
    f.ui.Down(150, 110);
    f.ui.Up(150, 110);
    REQUIRE(f.Popup());
    f.ui.Down(150, 110);  // on the select again: closes, does not reopen
    f.ui.Up(150, 110);
    CHECK(f.Popup() == nullptr);
}

TEST_CASE("Select: disabled is faded and does not open") {
    SelectFixture f;
    f.sel->SetEnabled(false);
    f.ui.Frame();
    CHECK(f.ui.canvas.HasOpacity(0.7f));
    f.ui.Down(150, 110);
    CHECK(f.Popup() == nullptr);
    f.ui.Key(Key::Tab);
    CHECK(f.ui.screen.Focused() == nullptr);  // not focusable
}

TEST_CASE("Select: removing it with its list open closes the list") {
    SelectFixture f;
    f.ui.Down(150, 110);
    REQUIRE(f.Popup());
    f.ui.screen.Root().RemoveChild(f.sel);
    CHECK(f.Popup() == nullptr);
}

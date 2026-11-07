// The Screen of the native Settings window (K1.b): hover, capture, focus order and the focus ring, the
// tooltip request, the cursor hint, the sheet stack, the ScrollArea, widget removal.
#include "native_ui/controls.h"
#include "native_ui/display.h"
#include "native_ui/scroll_area.h"
#include "native_ui/sheet.h"
#include "native_ui/slider.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

using namespace nui;
using namespace nui::test;

namespace {

Button* AddButton(Widget& parent, const char* text, float x, float y) {
    auto* b = parent.Add<Button>(text);
    b->SetBounds({x, y, 100, 34});
    return b;
}

}  // namespace

// ------------------------------------------------------------------------------ mouse ---

TEST_CASE("hover follows the pointer and ends on Leave") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    auto* b = AddButton(ui.screen.Root(), "B", 10, 100);
    ui.Move(20, 20);
    CHECK(a->Hovered());
    CHECK_FALSE(b->Hovered());
    ui.Move(20, 110);
    CHECK_FALSE(a->Hovered());
    CHECK(b->Hovered());
    ui.Leave();
    CHECK_FALSE(b->Hovered());
}

TEST_CASE("the cursor is a hand over a control that reacts, an arrow elsewhere") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    ui.Move(20, 20);
    CHECK(ui.screen.Cursor() == CursorHint::Hand);
    ui.Move(500, 500);
    CHECK(ui.screen.Cursor() == CursorHint::Arrow);
    a->SetEnabled(false);
    ui.Move(20, 20);
    CHECK(ui.screen.Cursor() == CursorHint::Arrow);
}

TEST_CASE("a drag keeps going to the widget that was pressed, outside its bounds") {
    Ui ui;
    std::vector<double> values;
    auto* s = ui.screen.Root().Add<Slider>();
    s->SetBounds({100, 100, 200, 28});
    s->SetRange({0, 10, 5, 1, false});
    s->SetOnValue([&](double v) { values.push_back(v); });

    ui.Down(150, 110);  // a quarter of the way: 2.5 -> snaps to 3
    REQUIRE(values.size() == 1);
    CHECK(values[0] == 3);
    CHECK(s->Dragging());
    ui.Move(900, 500);  // far outside the slider: still the drag
    REQUIRE(values.size() == 2);
    CHECK(values[1] == 10);
    ui.Up(900, 500);
    CHECK_FALSE(s->Dragging());
    ui.Move(120, 110);
    CHECK(values.size() == 2);  // no drag any more
}

TEST_CASE("the wheel goes up to a ScrollArea; an unused wheel is not used") {
    Ui ui;
    auto* area = ui.screen.Root().Add<ScrollArea>();
    area->SetBounds({0, 0, 300, 200});
    AddButton(*area, "In", 10, 10);
    area->SetContentHeight(150);  // fits: nothing to scroll
    CHECK_FALSE(ui.Wheel(20, 20, 48));
    area->SetContentHeight(600);
    CHECK(ui.Wheel(20, 20, 48));
    CHECK(area->Offset() == 48);
}

// ------------------------------------------------------------------------------- tooltip ---

TEST_CASE("the tooltip comes after the hover delay and goes with a press or another widget") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    auto* b = AddButton(ui.screen.Root(), "B", 10, 100);
    a->SetTooltip("About A");
    ui.screen.Tick(1.0);
    ui.Move(20, 20);
    CHECK(ui.screen.NeedsTick());                      // counting down
    CHECK_FALSE(ui.screen.Tooltip().has_value());
    ui.screen.Tick(1.3);
    CHECK_FALSE(ui.screen.Tooltip().has_value());
    ui.screen.Tick(1.6);
    REQUIRE(ui.screen.Tooltip().has_value());
    CHECK(ui.screen.Tooltip()->text == "About A");
    CHECK(ui.screen.Tooltip()->anchor.x == 10);
    CHECK_FALSE(ui.screen.NeedsTick());

    ui.Down(20, 20);                                   // a press hides it
    CHECK_FALSE(ui.screen.Tooltip().has_value());
    ui.Up(20, 20);

    ui.Move(20, 110);                                  // B has none
    ui.screen.Tick(5.0);
    CHECK_FALSE(ui.screen.Tooltip().has_value());
    (void)b;
}

// ---------------------------------------------------------------------------------- focus ---

TEST_CASE("Tab and Shift+Tab walk the focusable widgets in tree order and wrap") {
    Ui ui;
    Widget& root = ui.screen.Root();
    auto* first = AddButton(root, "First", 10, 10);
    auto* chip = root.Add<KeyChip>("F8");
    chip->SetBounds({10, 60, 70, 24});
    auto* hidden = AddButton(root, "Hidden", 10, 100);
    hidden->SetVisible(false);
    auto* off = AddButton(root, "Off", 10, 140);
    off->SetEnabled(false);
    auto* slider = root.Add<Slider>();
    slider->SetBounds({10, 180, 200, 28});
    slider->SetRange({0, 1, 0.5, 0.1, false});
    auto* noState = root.Add<Slider>();  // no state: not focusable
    noState->SetBounds({10, 220, 200, 28});

    CHECK(ui.screen.FocusOrder() == std::vector<Widget*>{first, chip, slider});

    CHECK(ui.Key(Key::Tab));
    CHECK(ui.screen.Focused() == first);
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == chip);
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == slider);
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == first);  // wraps
    ui.Key(Key::Tab, true);
    CHECK(ui.screen.Focused() == slider);  // Shift+Tab wraps back
    ui.Key(Key::Tab, true);
    CHECK(ui.screen.Focused() == chip);
}

TEST_CASE("Shift+Tab with nothing focused starts at the last widget") {
    Ui ui;
    AddButton(ui.screen.Root(), "A", 10, 10);
    auto* b = AddButton(ui.screen.Root(), "B", 10, 60);
    ui.Key(Key::Tab, true);
    CHECK(ui.screen.Focused() == b);
}

TEST_CASE("the focus ring shows only after the keyboard was used") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    auto* b = AddButton(ui.screen.Root(), "B", 10, 100);

    ui.Click(20, 20);  // a click focuses A, but no ring
    CHECK(ui.screen.Focused() == a);
    CHECK_FALSE(ui.screen.FocusVisible());
    ui.Frame();
    CHECK(ui.canvas.Count(Call::StrokeRect) == 0);

    ui.Key(Key::Tab);  // the keyboard: the ring shows on B
    CHECK(ui.screen.Focused() == b);
    CHECK(ui.screen.FocusVisible());
    ui.Frame();
    const auto rings = ui.canvas.Of(Call::StrokeRect);
    REQUIRE(rings.size() == 1);
    CHECK(rings[0]->number == 2);                        // 2 DIP line
    CHECK(rings[0]->colour.rgb == ui.pal.accent.rgb);
    CHECK(rings[0]->rect.x == 10 - 3);                   // 2 DIP offset + half the line
    CHECK(rings[0]->rect.w == 100 + 6);

    ui.Click(20, 20);  // the mouse again: gone
    CHECK_FALSE(ui.screen.FocusVisible());
    ui.Frame();
    CHECK(ui.canvas.Count(Call::StrokeRect) == 0);
}

TEST_CASE("a press on bare paper clears the focus") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    a->Focus();
    CHECK(ui.screen.Focused() == a);
    ui.Click(500, 500);
    CHECK(ui.screen.Focused() == nullptr);
}

TEST_CASE("removing a widget makes the Screen forget its hover, focus and capture") {
    Ui ui;
    auto* page = ui.screen.Root().Add<Widget>();
    page->SetBounds({0, 0, 800, 600});
    auto* a = AddButton(*page, "A", 10, 10);
    ui.Move(20, 20);
    ui.Down(20, 20);
    CHECK(ui.screen.Focused() == a);
    ui.screen.Root().RemoveChild(page);  // a page switch
    CHECK(ui.screen.Focused() == nullptr);
    ui.Up(20, 20);                       // must not touch the dead widget
    ui.Move(30, 30);
    CHECK(ui.screen.Cursor() == CursorHint::Arrow);
}

TEST_CASE("Defer runs after the event; outside an event it runs at once") {
    Ui ui;
    int order = 0, deferredAt = 0, afterAt = 0;
    auto* b = AddButton(ui.screen.Root(), "A", 10, 10);
    b->SetOnActivate([&] {
        ui.screen.Defer([&] { deferredAt = ++order; });
        afterAt = ++order;
    });
    ui.Click(20, 20);
    CHECK(afterAt == 1);
    CHECK(deferredAt == 2);
    int now = 0;
    ui.screen.Defer([&] { now = 1; });
    CHECK(now == 1);
}

TEST_CASE("Emit sends a PageMessage to the sink") {
    Ui ui;
    std::vector<PageMessageType> got;
    ui.screen.SetMessageSink([&](const PageMessage& m) { got.push_back(m.type); });
    auto* sw = ui.screen.Root().Add<SwitchRow>("Left-handed");
    sw->SetBounds({100, 100, 300, 28});
    sw->SetChecked(false);
    // The page's code: a click builds the message from the state it shows.
    sw->SetOnActivate([&] {
        PageMessage m;
        m.type = PageMessageType::SetLeftHanded;
        m.on = !sw->Checked();
        ui.screen.Emit(m);
    });
    ui.Click(200, 110);
    REQUIRE(got.size() == 1);
    CHECK(got[0] == PageMessageType::SetLeftHanded);
    CHECK_FALSE(sw->Checked());
}

TEST_CASE("the repaint flag: Paint clears it, a change sets it") {
    Ui ui;
    auto* a = AddButton(ui.screen.Root(), "A", 10, 10);
    ui.Frame();
    CHECK_FALSE(ui.screen.NeedsRepaint());
    a->SetText("B");
    CHECK(ui.screen.NeedsRepaint());
    ui.Frame();
    CHECK_FALSE(ui.screen.NeedsRepaint());
    ui.Move(20, 20);
    CHECK(ui.screen.NeedsRepaint());
}

// ----------------------------------------------------------------------------------- sheets ---

namespace {

struct SheetFixture {
    Ui ui;
    Button* pageButton = nullptr;
    int pageClicks = 0;
    int closed = 0;
    Sheet* sheet = nullptr;
    Button* cancel = nullptr;
    Button* ok = nullptr;

    SheetFixture() {
        pageButton = AddButton(ui.screen.Root(), "Page", 10, 10);
        pageButton->SetOnActivate([this] { ++pageClicks; });
        pageButton->Focus();
    }
    Sheet* Open(const char* title) {
        auto s = std::make_unique<Sheet>();
        s->SetTitle(title);
        s->AddBodyOf<Label>("Are you sure?");
        cancel = s->AddAction(std::make_unique<Button>("Cancel"));
        ok = s->AddAction(std::make_unique<Button>("OK"));
        s->SetOnClose([this] { ++closed; });
        ok->SetOnActivate([this] {
            if (sheet) sheet->Close();
        });
        sheet = ui.screen.OpenSheet(std::move(s));
        ui.Frame();
        return sheet;
    }
};

}  // namespace

TEST_CASE("a sheet: centred card, scrim behind, focus moves in") {
    SheetFixture f;
    Sheet* s = f.Open("Delete voice");
    CHECK(f.ui.screen.SheetCount() == 1);
    CHECK(s->Bounds().w == 420);
    CHECK(s->Bounds().x == 190);
    // border 1 + padding 24 + title 26.4 + 14 + one line 21.6 + 22 + a 34 high button + padding 24 + border 1
    CHECK(s->Bounds().h == doctest::Approx(25 + 26.4f + 14 + 21.6f + 22 + 34 + 25));

    // The scrim covers the whole window before the card is drawn.
    const auto fills = f.ui.canvas.Of(Call::FillRect);
    bool scrim = false;
    for (const Call* c : fills) {
        if (c->rect.w == 800 && c->rect.h == 600 && c->colour.rgb == f.ui.pal.scrim.rgb &&
            c->colour.alpha == f.ui.pal.scrim.alpha)
            scrim = true;
    }
    CHECK(scrim);
    CHECK(f.ui.canvas.FindText("Delete voice")->style.size == 22);
    CHECK(f.ui.canvas.FindText("Delete voice")->style.weight == FontWeight::Bold);
    CHECK(s->Access().role == Role::Dialog);
    CHECK(s->Access().name == "Delete voice");

    CHECK(f.ui.screen.Focused() == f.cancel);  // the first focusable in the sheet
    // The title is 14 above the body, the actions 22 below it, 12 apart.
    CHECK(f.ok->Bounds().x == f.cancel->Bounds().Right() + 12);
    CHECK(f.cancel->Bounds().x == 25);  // inside the border and the padding
}

TEST_CASE("a sheet is modal: the page gets no clicks, no Tab, no hover") {
    SheetFixture f;
    f.Open("Delete voice");
    f.ui.Click(20, 20);  // on the page button, under the scrim
    CHECK(f.pageClicks == 0);
    CHECK_FALSE(f.pageButton->Hovered());
    f.ui.Key(Key::Tab);
    CHECK(f.ui.screen.Focused() == f.ok);
    f.ui.Key(Key::Tab);
    CHECK(f.ui.screen.Focused() == f.cancel);  // stays inside the sheet
    f.ui.Key(Key::Tab, true);
    CHECK(f.ui.screen.Focused() == f.ok);
}

TEST_CASE("Esc closes the top sheet and the focus goes back to the opener") {
    SheetFixture f;
    f.Open("Delete voice");
    CHECK(f.ui.Key(Key::Escape));
    CHECK(f.ui.screen.SheetCount() == 0);
    CHECK(f.closed == 1);
    CHECK(f.ui.screen.Focused() == f.pageButton);
    // The page works again.
    f.ui.Click(20, 20);
    CHECK(f.pageClicks == 1);
}

TEST_CASE("a button inside the sheet can close it, even from its own click handler") {
    SheetFixture f;
    f.Open("Delete voice");
    const PointF p = Centre(*f.ok);
    f.ui.Click(p.x, p.y);  // OK -> sheet->Close()
    CHECK(f.ui.screen.SheetCount() == 0);
    CHECK(f.closed == 1);
    CHECK(f.ui.screen.Focused() == f.pageButton);
    f.ui.Frame();  // draws again with nothing dangling
}

TEST_CASE("sheets stack: Esc closes only the top one, the scrim is drawn for each") {
    SheetFixture f;
    f.Open("First");
    auto second = std::make_unique<Sheet>(300.0f);
    second->SetTitle("Second");
    auto* inner = second->AddAction(std::make_unique<Button>("Inner"));
    f.ui.screen.OpenSheet(std::move(second));
    f.ui.Frame();
    CHECK(f.ui.screen.SheetCount() == 2);
    CHECK(f.ui.screen.Focused() == inner);
    int scrims = 0;
    for (const Call* c : f.ui.canvas.Of(Call::FillRect))
        if (c->rect.w == 800 && c->rect.h == 600) ++scrims;
    CHECK(scrims == 2);

    f.ui.Key(Key::Escape);
    CHECK(f.ui.screen.SheetCount() == 1);
    CHECK(f.ui.screen.Focused() == f.cancel);  // back in the first sheet
    f.ui.Key(Key::Escape);
    CHECK(f.ui.screen.SheetCount() == 0);
    CHECK(f.ui.screen.Focused() == f.pageButton);
}

TEST_CASE("a press on the scrim does nothing") {
    SheetFixture f;
    f.Open("Delete voice");
    CHECK(f.ui.Click(700, 550));
    CHECK(f.ui.screen.SheetCount() == 1);
}

TEST_CASE("hidden actions take no room; a full row wraps") {
    SheetFixture f;
    auto s = std::make_unique<Sheet>(200.0f);  // inside width 148
    auto* a = s->AddAction(std::make_unique<Button>("Aaaaaaa"));  // 56 + 36 = 92
    auto* b = s->AddAction(std::make_unique<Button>("Bbbbbbb"));  // 92: does not fit beside a
    auto* c = s->AddAction(std::make_unique<Button>("Cc"));       // 16 + 36 = 52
    c->SetVisible(false);
    f.ui.screen.OpenSheet(std::move(s));
    f.ui.Frame();
    CHECK(a->Bounds().y == doctest::Approx(b->Bounds().y - 34 - 12));
    CHECK(b->Bounds().x == 25);
    // Hiding the first lets the second take its place.
    a->SetVisible(false);
    f.ui.Frame();
    CHECK(b->Bounds().x == 25);
}

// --------------------------------------------------------------------------------- ScrollArea ---

namespace {

struct ScrollFixture {
    Ui ui;
    ScrollArea* area = nullptr;
    Button* top = nullptr;
    Button* mid = nullptr;
    Button* low = nullptr;
    ScrollFixture() {
        area = ui.screen.Root().Add<ScrollArea>();
        area->SetBounds({0, 0, 300, 200});
        top = AddButton(*area, "Top", 10, 0);
        mid = AddButton(*area, "Mid", 10, 300);
        low = AddButton(*area, "Low", 10, 500);
        area->SetContentHeight(600);
    }
};

}  // namespace

TEST_CASE("ScrollArea: offset is clamped and survives a content update") {
    ScrollFixture f;
    CHECK(f.area->MaxOffset() == 400);
    f.area->ScrollTo(120);
    CHECK(f.area->Offset() == 120);
    f.area->ScrollTo(9999);
    CHECK(f.area->Offset() == 400);
    f.area->ScrollTo(-5);
    CHECK(f.area->Offset() == 0);

    f.area->ScrollTo(120);
    f.area->SetContentHeight(1000);  // the content grew: the place is kept
    CHECK(f.area->Offset() == 120);
    f.area->SetContentHeight(250);   // it shrank below the place: clamped
    CHECK(f.area->Offset() == 50);
}

TEST_CASE("ScrollArea: children move with the offset and are clipped") {
    ScrollFixture f;
    f.area->ScrollTo(100);
    CHECK(f.mid->Abs().y == 200);
    f.ui.Frame();
    // The area clips its children: a PushClip of the viewport comes before the Button's drawing.
    REQUIRE(f.ui.canvas.Count(Call::PushClip) == 1);
    CHECK(f.ui.canvas.Count(Call::PopClip) == 1);
    const auto clips = f.ui.canvas.Of(Call::PushClip);
    CHECK(clips[0]->rect.w == 300);
    CHECK(clips[0]->rect.h == 200);
    // A thin scrollbar is drawn when the content is taller than the view.
    bool bar = false;
    for (const Call* c : f.ui.canvas.Of(Call::FillRect))
        if (c->rect.w == kScrollbarWidth) bar = true;
    CHECK(bar);
}

TEST_CASE("ScrollArea: clipped children cannot be hit; widgets below it can") {
    // The sibling is added first, so the ScrollArea (and its children) would be on top of it.
    Ui ui;
    int below = 0;
    auto* sibling = AddButton(ui.screen.Root(), "Below", 10, 210);
    sibling->SetOnActivate([&] { ++below; });
    auto* area = ui.screen.Root().Add<ScrollArea>();
    area->SetBounds({0, 0, 300, 200});
    auto* low = AddButton(*area, "Low", 10, 500);
    area->SetContentHeight(600);
    area->ScrollTo(300);
    // `low` is at content y 500 -> view y 200..234: outside the 200 high viewport, over the sibling.
    CHECK(low->Abs().y == 200);
    ui.Click(20, 215);
    CHECK(below == 1);
}

TEST_CASE("ScrollArea: keys PageDown, PageUp, Home, End from a focused child") {
    ScrollFixture f;
    f.top->Focus();
    f.ui.Key(Key::PageDown);
    CHECK(f.area->Offset() == doctest::Approx(180));  // 0.9 of the view
    f.ui.Key(Key::End);
    CHECK(f.area->Offset() == 400);
    f.ui.Key(Key::PageUp);
    CHECK(f.area->Offset() == doctest::Approx(220));
    f.ui.Key(Key::Home);
    CHECK(f.area->Offset() == 0);
    f.ui.Key(Key::Down);
    CHECK(f.area->Offset() == kScrollKeyStep);
}

TEST_CASE("ScrollArea: Tab to a hidden control scrolls it into view") {
    ScrollFixture f;
    f.ui.Key(Key::Tab);  // Top
    CHECK(f.area->Offset() == 0);
    f.ui.Key(Key::Tab);  // Mid, at content y 300..334, view height 200
    CHECK(f.ui.screen.Focused() == f.mid);
    CHECK(f.mid->Abs().Bottom() <= 200 - 4 + 0.01f);
    CHECK(f.mid->Abs().y >= 0);
    f.ui.Key(Key::Tab);  // Low
    CHECK(f.ui.screen.Focused() == f.low);
    CHECK(f.low->Abs().Bottom() <= 196.01f);
    CHECK(f.area->Offset() == doctest::Approx(500 + 34 - 196));
    f.ui.Key(Key::Tab, true);  // back up to Mid
    CHECK(f.mid->Abs().y >= 0);
    CHECK(f.mid->Abs().Bottom() <= 196.01f);
}

TEST_CASE("ScrollArea: ScrollIntoView aligns the top of a widget taller than the view") {
    ScrollFixture f;
    f.mid->SetBounds({10, 300, 100, 300});  // taller than the 200 high view
    f.area->ScrollIntoView(f.mid);
    CHECK(f.mid->Abs().y == doctest::Approx(4));
}

TEST_CASE("ScrollArea: the scrollbar thumb can be dragged") {
    ScrollFixture f;
    const RectF thumb = f.area->ThumbRect();
    CHECK(thumb.y == 0);
    CHECK(thumb.h == doctest::Approx(200 * 200 / 600.0f));
    const float x = thumb.x + 1;
    f.ui.Down(x, 10);
    f.ui.Move(x, 110);
    CHECK(f.area->Offset() == doctest::Approx(300));
    f.ui.Up(x, 110);
    f.ui.Move(x, 190);  // no longer dragging
    CHECK(f.area->Offset() == doctest::Approx(300));
}

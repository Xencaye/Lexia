// HitTestWindow (K10): which points of the Settings window drag the window (HTCAPTION) and which belong to the
// widget tree (HTCLIENT). Pure: a Screen with a FakeCanvas, no window.
#include "native_ui/controls.h"
#include "native_ui/display.h"
#include "native_ui/pages/home_page.h"
#include "native_ui/pages/layout.h"
#include "native_ui/scroll_area.h"
#include "native_ui/select.h"
#include "native_ui/settings_ui.h"
#include "native_ui/sheet.h"
#include "native_ui/slider.h"
#include "native_ui/window_hit.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

using namespace nui;
using namespace nui::test;

namespace {

WindowHit At(const Ui& ui, PointF p) { return HitTestWindow(ui.screen, p); }

}  // namespace

TEST_CASE("window hit: bare paper drags the window, a control is the tree's") {
    Ui ui;
    auto* button = ui.screen.Root().Add<Button>("OK");
    button->SetBounds({100, 100, 100, 34});
    auto* slider = ui.screen.Root().Add<Slider>();
    slider->SetBounds({100, 200, 200, 24});
    ui.Frame();

    CHECK(At(ui, {10, 10}) == WindowHit::Caption);        // nothing there
    CHECK(At(ui, {150, 117}) == WindowHit::Client);       // a button
    CHECK(At(ui, {200, 212}) == WindowHit::Client);       // a slider
    CHECK(At(ui, {150, 150}) == WindowHit::Caption);      // between them
}

TEST_CASE("window hit: plain text and pictures are not controls, so they drag the window too") {
    Ui ui;
    auto* label = ui.screen.Root().Add<Label>("Some words");
    label->SetBounds({100, 100, 150, 20});
    auto* paragraph = ui.screen.Root().Add<Paragraph>("A paragraph of text.");
    paragraph->SetBounds({100, 140, 200, 40});
    auto* picture = ui.screen.Root().Add<PictureWidget>("flute", PictureTint::Flute);
    picture->SetBounds({400, 100, 100, 150});
    ui.Frame();

    REQUIRE(ui.screen.WidgetAt({150, 110}) == label);  // it is a hit target, and still passive
    CHECK(At(ui, {150, 110}) == WindowHit::Caption);
    CHECK(At(ui, {150, 160}) == WindowHit::Caption);
    CHECK(At(ui, {450, 180}) == WindowHit::Caption);
}

TEST_CASE("window hit: a scroll area keeps its content and scrollbar for the tree") {
    Ui ui;
    auto* area = ui.screen.Root().Add<ScrollArea>();
    area->SetBounds({100, 100, 300, 200});
    auto* label = area->Add<Label>("Inside the list");
    label->SetBounds({0, 0, 200, 20});
    area->SetContentHeight(600);
    ui.Frame();

    CHECK(At(ui, {150, 110}) == WindowHit::Client);   // text, but inside a scroll area
    CHECK(At(ui, {150, 250}) == WindowHit::Client);   // the area's empty part
    const RectF thumb = area->ThumbRect();
    CHECK(At(ui, {thumb.x + thumb.w / 2, thumb.y + thumb.h / 2}) == WindowHit::Client);
    CHECK(At(ui, {50, 50}) == WindowHit::Caption);    // outside it
}

TEST_CASE("window hit: while a sheet or an open list is up, every point is the tree's") {
    Ui ui;
    auto* select = ui.screen.Root().Add<Select>();
    select->SetBounds({100, 100, 200, 28});
    select->SetItems({"Anna", "Brian"});
    select->SetSelected(0);
    ui.Frame();
    REQUIRE(At(ui, {10, 10}) == WindowHit::Caption);

    ui.Click(200, 114);  // opens the list
    ui.Frame();
    REQUIRE(ui.screen.Popup());
    CHECK(At(ui, {10, 10}) == WindowHit::Client);  // a press outside the list must close it: not a window move
    ui.screen.ClosePopup();
    ui.Frame();
    CHECK(At(ui, {10, 10}) == WindowHit::Caption);

    auto sheet = std::make_unique<Sheet>();
    sheet->SetTitle("Title");
    sheet->AddBodyOf<Label>("Words");
    sheet->AddAction(std::make_unique<Button>("OK"));
    ui.screen.OpenSheet(std::move(sheet));
    ui.Frame();
    CHECK(At(ui, {10, 10}) == WindowHit::Client);  // the scrim
    CHECK(At(ui, {400, 300}) == WindowHit::Client);  // the sheet
}

TEST_CASE("window hit: the real frame - links, volume, the two window buttons and the power circle are the tree's") {
    Ui ui;
    SettingsUi settings(ui.screen, "en");
    ui.Frame();
    Frame& frame = settings.GetFrame();

    for (Page p : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
        CAPTURE(PageName(p));
        CHECK(At(ui, Centre(*frame.LinkFor(p))) == WindowHit::Client);
    }
    CHECK(At(ui, Centre(*frame.Volume())) == WindowHit::Client);
    CHECK(At(ui, Centre(*frame.MinimiseButton())) == WindowHit::Client);
    CHECK(At(ui, Centre(*frame.CloseButton())) == WindowHit::Client);
    // Every point of the two hit areas (not only the icon boxes) is theirs.
    const RectF m = frame.MinimiseButton()->Abs();
    CHECK(At(ui, {m.x + 0.5f, m.y + 0.5f}) == WindowHit::Client);
    CHECK(At(ui, {m.Right() - 0.5f, m.Bottom() - 0.5f}) == WindowHit::Client);
    // The 6 DIPs between the two buttons are paper: they drag.
    CHECK(At(ui, {m.Right() + 3, m.y + 14}) == WindowHit::Caption);
    CHECK(At(ui, Centre(*static_cast<HomePage*>(settings.Current())->Power())) == WindowHit::Client);

    // Paper: the corners of the window, beside the top links, under the buttons, the flute, the bee's corner
    // outside the circle, and the dotted rule.
    CHECK(At(ui, {5, 5}) == WindowHit::Caption);
    CHECK(At(ui, {60, 36}) == WindowHit::Caption);
    CHECK(At(ui, {layout::kCloseX + 12, layout::kWindowButtonsY + 40}) == WindowHit::Caption);
    CHECK(At(ui, {layout::kFlute.x + 50, layout::kFlute.y + 75}) == WindowHit::Caption);
    CHECK(At(ui, {layout::kPower.x + 2, layout::kPower.y + 2}) == WindowHit::Caption);
    CHECK(At(ui, {300, layout::kRuleTop.y}) == WindowHit::Caption);
}

TEST_CASE("window hit: the pages' own controls are the tree's") {
    Ui ui;
    SettingsUi settings(ui.screen, "en");
    for (Page p : {Page::Voices, Page::Settings, Page::Report, Page::About}) {
        CAPTURE(PageName(p));
        settings.ShowPage(p);
        ui.Frame();
        // Every widget Tab reaches is a control: a press on its centre is not a window move.
        for (Widget* w : ui.screen.FocusOrder()) {
            CAPTURE(w->Access().name);
            CHECK(At(ui, Centre(*w)) == WindowHit::Client);
        }
    }
}

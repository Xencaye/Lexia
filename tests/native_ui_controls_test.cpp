// The clickable controls of the native Settings window (K1.b): IconButton, TextLink, Button, SwitchRow,
// KeyChip, and the display widgets (Label, Notice, ProgressBar): what a click does, how they look in each
// state, what accessibility says.
#include "native_ui/controls.h"
#include "native_ui/display.h"
#include "native_ui_test_util.h"

#include <doctest/doctest.h>

using namespace nui;
using namespace nui::test;

namespace {

// Lets a transition finish and draws a frame, so the canvas shows the end look.
void Settle(Ui& ui) { ui.Advance(0.5); }

}  // namespace

// ----------------------------------------------------------------------------- IconButton ---

TEST_CASE("IconButton: press and release on it is one click; the look follows hover and press") {
    Ui ui;
    int clicks = 0;
    auto* b = ui.screen.Root().Add<IconButton>("home-05");
    b->SetPosition(10, 10);
    b->SetOnActivate([&] { ++clicks; });
    ui.Frame();

    // Idle: no tile, the icon in `line`.
    CHECK(ui.canvas.Count(Call::FillRect) == 0);
    REQUIRE(ui.canvas.FindIcon("home-05"));
    CHECK(ui.canvas.FindIcon("home-05")->colour.rgb == ui.pal.line.rgb);
    CHECK(ui.canvas.FindIcon("home-05")->rect.w == 22);

    ui.Move(20, 20);
    Settle(ui);
    REQUIRE(ui.canvas.Count(Call::FillRect) == 1);
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.line.rgb);  // hover: a `line` tile ...
    CHECK(ui.canvas.calls[0].radius == 4);
    CHECK(ui.canvas.calls[0].rect.w == 28);
    CHECK(ui.canvas.FindIcon("home-05")->colour.rgb == ui.pal.accent.rgb);  // ... the icon in accent

    ui.Down(20, 20);
    Settle(ui);
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.accent.rgb);  // pressed: an accent tile
    CHECK(ui.canvas.FindIcon("home-05")->colour.rgb == ui.pal.onAccent.rgb);
    CHECK(clicks == 0);                                          // not yet: the release makes the click
    ui.Up(20, 20);
    CHECK(clicks == 1);
}

TEST_CASE("IconButton: a press that is released away from it is not a click") {
    Ui ui;
    int clicks = 0;
    auto* b = ui.screen.Root().Add<IconButton>("x");
    b->SetPosition(10, 10);
    b->SetOnActivate([&] { ++clicks; });
    ui.Move(20, 20);
    ui.Down(20, 20);
    ui.Move(300, 300);  // dragged off: the capture keeps the press with the button
    CHECK(ui.Up(300, 300));
    CHECK(clicks == 0);
}

TEST_CASE("IconButton: a right click does nothing") {
    Ui ui;
    int clicks = 0;
    auto* b = ui.screen.Root().Add<IconButton>("x");
    b->SetPosition(10, 10);
    b->SetOnActivate([&] { ++clicks; });
    ui.Down(20, 20, MouseButton::Right);
    ui.Up(20, 20, MouseButton::Right);
    CHECK(clicks == 0);
}

TEST_CASE("IconButton: SetPressed gives the pressed look and a toggle state") {
    Ui ui;
    auto* b = ui.screen.Root().Add<IconButton>("pause");
    b->SetPosition(10, 10);
    CHECK(b->Access().toggle == Toggle::None);
    b->SetPressed(true);
    CHECK(b->Access().toggle == Toggle::On);
    Settle(ui);
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.accent.rgb);
    b->SetPressed(false);
    CHECK(b->Access().toggle == Toggle::Off);
}

TEST_CASE("IconButton: disabled looks idle, does nothing, keeps focus and tooltip") {
    Ui ui;
    int clicks = 0;
    auto* b = ui.screen.Root().Add<IconButton>("skip");
    b->SetPosition(10, 10);
    b->SetOnActivate([&] { ++clicks; });
    b->SetTooltip("Nothing to skip");
    b->SetEnabled(false);
    ui.Frame();

    ui.Move(20, 20);
    Settle(ui);
    CHECK(ui.canvas.Count(Call::FillRect) == 0);  // no hover tile
    CHECK(ui.canvas.FindIcon("skip")->colour.rgb == ui.pal.line.rgb);
    ui.Click(20, 20);
    CHECK(clicks == 0);
    ui.Key(Key::Enter);
    CHECK(clicks == 0);

    // The focus can reach it, and the tooltip shows (a press hid it, so the pointer comes in again).
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == b);
    ui.Leave();
    ui.Move(20, 20);
    ui.screen.Tick(5.0);
    REQUIRE(ui.screen.Tooltip().has_value());
    CHECK(ui.screen.Tooltip()->text == "Nothing to skip");
    CHECK(ui.screen.Cursor() == CursorHint::Arrow);
    CHECK_FALSE(b->Access().enabled);
    CHECK(b->Access().focusable);
}

TEST_CASE("Enter and Space click the focused control; a held key does not repeat") {
    Ui ui;
    int clicks = 0;
    auto* b = ui.screen.Root().Add<IconButton>("x");
    b->SetPosition(10, 10);
    b->SetOnActivate([&] { ++clicks; });
    b->Focus();
    CHECK(ui.Key(Key::Enter));
    CHECK(ui.Key(Key::Space));
    CHECK(clicks == 2);
    ui.Key(Key::Enter, false, true);  // auto-repeat
    CHECK(clicks == 2);
}

// -------------------------------------------------------------------------------- TextLink ---

TEST_CASE("TextLink: the bold width is always reserved, so the links never shift") {
    Ui ui;
    ui.canvas.boldFactor = 1.25f;
    auto* a = ui.screen.Root().Add<TextLink>("Voices", "voices-icon");
    auto* c = ui.screen.Root().Add<TextLink>("Voices", "voices-icon");
    c->SetCurrent(true);
    const SizeF wa = a->Measure(ui.canvas, 1000);
    const SizeF wc = c->Measure(ui.canvas, 1000);
    CHECK(wa.w == wc.w);
    // icon 22 + gap 7 + the bold word (6 letters * 8 * 1.25)
    CHECK(wa.w == doctest::Approx(22 + 7 + 60));
}

TEST_CASE("TextLink: icon in accent, word in ink; hover underlines; current is bold; click") {
    Ui ui;
    int clicks = 0;
    auto* l = ui.screen.Root().Add<TextLink>("Help", "help-icon");
    l->SetBounds({20, 20, 100, 22});
    l->SetOnActivate([&] { ++clicks; });
    ui.Frame();
    CHECK(ui.canvas.FindIcon("help-icon")->colour.rgb == ui.pal.accent.rgb);
    const Call* word = ui.canvas.FindText("Help");
    REQUIRE(word);
    CHECK(word->colour.rgb == ui.pal.ink.rgb);
    CHECK_FALSE(word->style.underline);
    CHECK(word->rect.x == 20 + 22 + 7);

    ui.Move(30, 30);
    ui.Frame();
    CHECK(ui.canvas.FindText("Help")->style.underline);
    ui.Click(30, 30);
    CHECK(clicks == 1);

    l->SetCurrent(true);
    ui.Frame();
    CHECK(ui.canvas.FindText("Help")->style.weight == FontWeight::Bold);
    CHECK(l->Access().selected);
    CHECK(l->Access().role == Role::Link);
}

TEST_CASE("TextLink: small is 13 px and always underlined; disabled is faded and silent") {
    Ui ui;
    int clicks = 0;
    auto* l = ui.screen.Root().Add<TextLink>("Notices");
    l->SetBounds({20, 20, 100, 20});
    l->SetOnActivate([&] { ++clicks; });
    l->SetSmall(true);
    ui.Frame();
    CHECK(ui.canvas.FindText("Notices")->style.size == 13);
    CHECK(ui.canvas.FindText("Notices")->style.underline);

    l->SetEnabled(false);
    ui.Frame();
    CHECK(ui.canvas.HasOpacity(0.55f));
    ui.Click(30, 28);
    CHECK(clicks == 0);
}

// ---------------------------------------------------------------------------------- Button ---

TEST_CASE("Button: sizes and colours") {
    Ui ui;
    auto* b = ui.screen.Root().Add<Button>("Download");
    // 8 letters * 8 = 64 + 2 * 18
    CHECK(b->Measure(ui.canvas, 1000).w == 64 + 36);
    CHECK(b->Measure(ui.canvas, 1000).h == 34);
    b->SetBounds({10, 10, 100, 34});
    ui.Frame();
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.accent.rgb);
    CHECK(ui.canvas.calls[0].radius == 4);
    CHECK(ui.canvas.FindText("Download")->colour.rgb == ui.pal.onAccent.rgb);
    CHECK(ui.canvas.FindText("Download")->style.weight == FontWeight::Bold);
    CHECK(ui.canvas.FindText("Download")->style.size == 16);

    b->SetQuiet(true);
    ui.Frame();
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.line.rgb);
    CHECK(ui.canvas.FindText("Download")->colour.rgb == ui.pal.accent.rgb);

    b->SetQuiet(false);
    b->SetSmall(true);
    CHECK(b->Measure(ui.canvas, 1000).h == 28);
    CHECK(b->Measure(ui.canvas, 1000).w == 7 * 8 + 24);  // 14 px letters are 7 wide, padding 12
    ui.Frame();
    CHECK(ui.canvas.FindText("Download")->style.size == 14);
}

TEST_CASE("Button: disabled is the line fill, faded, not clickable and not focusable") {
    Ui ui;
    int clicks = 0;
    auto* b1 = ui.screen.Root().Add<Button>("One");
    auto* b2 = ui.screen.Root().Add<Button>("Two");
    b1->SetBounds({10, 10, 80, 34});
    b2->SetBounds({10, 60, 80, 34});
    b1->SetOnActivate([&] { ++clicks; });
    b1->SetEnabled(false);
    ui.Frame();
    CHECK(ui.canvas.calls[1].colour.rgb == ui.pal.line.rgb);  // call 0 is the opacity push
    CHECK(ui.canvas.HasOpacity(0.7f));
    CHECK(ui.canvas.FindText("One")->colour.rgb == ui.pal.accent.rgb);
    ui.Click(20, 20);
    CHECK(clicks == 0);
    CHECK(ui.screen.Focused() == nullptr);  // the click on a disabled button focuses nothing
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == b2);       // Tab skips it
}

// -------------------------------------------------------------------------------- SwitchRow ---

TEST_CASE("SwitchRow: a click asks, it does not flip; SetChecked shows the state") {
    Ui ui;
    int clicks = 0;
    auto* s = ui.screen.Root().Add<SwitchRow>("Run at startup");
    s->SetBounds({100, 100, 400, 28});
    s->SetOnActivate([&] { ++clicks; });
    ui.Frame();
    ui.Click(450, 114);
    CHECK(clicks == 1);
    CHECK_FALSE(s->Checked());  // the page answers with the next state
    CHECK(s->Access().toggle == Toggle::Off);

    s->SetChecked(true);
    CHECK(s->Access().toggle == Toggle::On);
    CHECK(s->Access().role == Role::Switch);
    Settle(ui);
    // The pill: 22 x 12 at the right edge, accent fill when on.
    const auto fills = ui.canvas.Of(Call::FillRect);
    REQUIRE(fills.size() == 1);
    CHECK(fills[0]->rect.x == 500 - 22);
    CHECK(fills[0]->rect.w == 22);
    CHECK(fills[0]->rect.h == 12);
    CHECK(fills[0]->radius == 6);
    CHECK(fills[0]->colour.rgb == ui.pal.accent.rgb);
    CHECK(fills[0]->colour.alpha == doctest::Approx(1.0f));
    // The knob ring (12) sits 10 to the right of the pill's left end when on.
    const auto rings = ui.canvas.Of(Call::StrokeEllipse);
    REQUIRE(rings.size() == 1);
    CHECK(rings[0]->rect.x == doctest::Approx(500 - 22 + 10 + 0.75f));
    CHECK(rings[0]->number == 1.5f);
    const auto knobFill = ui.canvas.Of(Call::FillEllipse);
    REQUIRE(knobFill.size() == 1);
    CHECK(knobFill[0]->colour.rgb == ui.pal.onAccent.rgb);

    s->SetChecked(false);
    Settle(ui);
    CHECK(ui.canvas.Of(Call::StrokeEllipse)[0]->rect.x == doctest::Approx(500 - 22 + 0.75f));
    CHECK(ui.canvas.Of(Call::FillRect)[0]->colour.alpha == doctest::Approx(0.0f));
}

TEST_CASE("SwitchRow: the label is right-aligned and underlined on hover") {
    Ui ui;
    auto* s = ui.screen.Root().Add<SwitchRow>("Left-handed");
    s->SetBounds({100, 100, 400, 28});
    ui.Frame();
    const Call* label = ui.canvas.FindText("Left-handed");
    REQUIRE(label);
    CHECK(label->style.align == TextAlign::Right);
    CHECK(label->rect.Right() == 500 - 22 - 8);  // 8 before the pill
    CHECK_FALSE(label->style.underline);
    ui.Move(200, 110);
    ui.Frame();
    CHECK(ui.canvas.FindText("Left-handed")->style.underline);
}

TEST_CASE("SwitchRow: disabled is faded, has no hover and does nothing, but keeps the focus") {
    Ui ui;
    int clicks = 0;
    auto* s = ui.screen.Root().Add<SwitchRow>("Start minimised");
    s->SetBounds({100, 100, 400, 28});
    s->SetOnActivate([&] { ++clicks; });
    s->SetEnabled(false);
    ui.Move(200, 110);
    ui.Frame();
    CHECK(ui.canvas.HasOpacity(0.5f));
    CHECK_FALSE(ui.canvas.FindText("Start minimised")->style.underline);
    ui.Click(200, 110);
    CHECK(clicks == 0);
    ui.Key(Key::Tab);
    CHECK(ui.screen.Focused() == s);
}

// ---------------------------------------------------------------------------------- KeyChip ---

TEST_CASE("KeyChip: the three looks") {
    Ui ui;
    auto* k = ui.screen.Root().Add<KeyChip>("F8");
    k->SetBounds({10, 10, 70, 24});
    k->SetState(KeyChip::State::Set);
    ui.Frame();
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.accent.rgb);
    CHECK(ui.canvas.calls[0].radius == 2);
    CHECK(ui.canvas.FindText("F8")->colour.rgb == ui.pal.onAccent.rgb);
    CHECK(ui.canvas.FindText("F8")->style.size == 17);

    k->SetText("none");
    k->SetState(KeyChip::State::None);
    ui.Frame();
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.line.rgb);
    CHECK(ui.canvas.FindText("none")->colour.rgb == ui.pal.accent.rgb);

    k->SetState(KeyChip::State::Capturing);
    ui.Frame();
    CHECK(ui.canvas.calls[0].colour.rgb == ui.pal.card.rgb);
    const auto outline = ui.canvas.Of(Call::StrokeRect);
    REQUIRE(outline.size() == 1);
    CHECK(outline[0]->number == 2);
    CHECK(outline[0]->rect.x == 11);  // inside the chip
    CHECK(ui.canvas.FindText("none")->colour.rgb == ui.pal.accent.rgb);
}

TEST_CASE("KeyChip: minimum width 70, grows with the text") {
    Ui ui;
    auto* k = ui.screen.Root().Add<KeyChip>("F8");
    CHECK(k->Measure(ui.canvas, 1000).w == 70);
    CHECK(k->Measure(ui.canvas, 1000).h == 24);
    k->SetText("Ctrl+Shift+F12");  // 14 * 8.5 + 16
    CHECK(k->Measure(ui.canvas, 1000).w == doctest::Approx(14 * 8.5f + 16));
}

TEST_CASE("KeyChip: while capturing and focused it takes every key, Tab and Esc too") {
    Ui ui;
    std::vector<Key> got;
    int clicks = 0;
    auto* k = ui.screen.Root().Add<KeyChip>("F8");
    auto* other = ui.screen.Root().Add<Button>("Other");
    k->SetBounds({10, 10, 70, 24});
    other->SetBounds({10, 50, 80, 34});
    k->SetOnActivate([&] { ++clicks; });
    k->SetOnKey([&](const KeyEvent& e) { got.push_back(e.key); });

    k->Focus();
    ui.Key(Key::Enter);  // not capturing: a click
    CHECK(clicks == 1);
    CHECK(got.empty());

    k->SetState(KeyChip::State::Capturing);
    CHECK(ui.Key(Key::Tab));
    CHECK(ui.Key(Key::Escape));
    CHECK(ui.Key(Key::Other));
    CHECK(clicks == 1);
    REQUIRE(got.size() == 3);
    CHECK(got[0] == Key::Tab);
    CHECK(ui.screen.Focused() == k);  // Tab did not move the focus
    CHECK(k->Access().role == Role::KeyChip);
    CHECK(k->Access().value == "F8");
}

// ----------------------------------------------------------------------------- display widgets ---

TEST_CASE("Label draws one line and says its text") {
    Ui ui;
    auto* l = ui.screen.Root().Add<Label>("Hello");
    l->SetBounds({10, 10, 100, 22});
    l->SetStyle(22, FontWeight::Bold);
    l->SetColour(ColourRole::Accent);
    l->SetAlign(TextAlign::Centre);
    ui.Frame();
    const Call* t = ui.canvas.FindText("Hello");
    REQUIRE(t);
    CHECK(t->style.size == 22);
    CHECK(t->style.weight == FontWeight::Bold);
    CHECK(t->style.align == TextAlign::Centre);
    CHECK(t->colour.rgb == ui.pal.accent.rgb);
    CHECK(l->Access().role == Role::Text);
    CHECK(l->Access().value == "Hello");
    CHECK(l->Measure(ui.canvas, 0).w == 5 * 11);
}

TEST_CASE("Notice: left border, mark, lead, text; bad is danger") {
    Ui ui;
    auto* n = ui.screen.Root().Add<Notice>();
    n->SetLead("Done.");
    n->SetText("The Voice is installed.");
    n->SetBounds({10, 10, 380, 60});
    ui.Frame();
    const auto fills = ui.canvas.Of(Call::FillRect);
    REQUIRE(fills.size() == 2);
    CHECK(fills[0]->colour.rgb == ui.pal.paper.rgb);
    CHECK(fills[1]->colour.rgb == ui.pal.accent.rgb);
    CHECK(fills[1]->rect.w == 3);
    CHECK(ui.canvas.Count(Call::Polyline) == 1);  // the check mark
    CHECK(ui.canvas.FindText("Done.")->style.weight == FontWeight::Bold);
    CHECK(ui.canvas.FindText("Done.")->colour.rgb == ui.pal.accent.rgb);
    CHECK(ui.canvas.FindText("The")->colour.rgb == ui.pal.ink.rgb);

    n->SetKind(Notice::Kind::Bad);
    ui.Frame();
    CHECK(ui.canvas.Of(Call::FillRect)[1]->colour.rgb == ui.pal.danger.rgb);
    CHECK(ui.canvas.Count(Call::Polyline) == 0);
    CHECK(ui.canvas.Count(Call::Line) == 1);  // the "!"
    CHECK(ui.canvas.FindText("Done.")->colour.rgb == ui.pal.danger.rgb);
    CHECK(ui.canvas.FindText("The")->colour.rgb == ui.pal.danger.rgb);
}

TEST_CASE("Notice: the text wraps in the column beside the lead; height follows") {
    Ui ui;
    auto* n = ui.screen.Root().Add<Notice>();
    n->SetLead("Done.");  // 5 * 8 bold = 40 (boldFactor 1)
    n->SetText("one two three four five six seven eight nine ten");
    // The text column starts 3 + 12 + 20 + 9 + 40 + 9 = 93 from the left and ends 12 before the right
    // edge: 195 wide. "one two three four five" is 184 (a sixth word would make it 216), then
    // "six seven eight nine ten" is 192: two lines.
    const SizeF s = n->Measure(ui.canvas, 300);
    CHECK(s.h == doctest::Approx(2 * 10 + 2 * 21.6f));
    // It is a live region so a result is announced.
    CHECK(n->Access().liveRegion);
    CHECK(n->Access().name == "Done.");
}

TEST_CASE("ProgressBar: 6 tall, a line track and an accent fill") {
    Ui ui;
    auto* p = ui.screen.Root().Add<ProgressBar>();
    p->SetBounds({10, 10, 200, 6});
    p->SetFraction(0.25);
    ui.Frame();
    const auto fills = ui.canvas.Of(Call::FillRect);
    REQUIRE(fills.size() == 2);
    CHECK(fills[0]->colour.rgb == ui.pal.line.rgb);
    CHECK(fills[0]->radius == 3);
    CHECK(fills[1]->colour.rgb == ui.pal.accent.rgb);
    CHECK(fills[1]->rect.w == 50);
    p->SetFraction(7);  // held to 0..1
    CHECK(p->Fraction() == 1.0);
    CHECK(p->Access().role == Role::ProgressBar);
    CHECK(p->Access().hasRange);
    CHECK(p->Measure(ui.canvas, 300).h == 6);
}

TEST_CASE("IconWidget and PictureWidget draw what they are told") {
    Ui ui;
    auto* i = ui.screen.Root().Add<IconWidget>("check", ColourRole::Danger);
    i->SetBounds({0, 0, 20, 20});
    auto* p = ui.screen.Root().Add<PictureWidget>("flute", PictureTint::Dog);
    p->SetBounds({100, 0, 200, 300});
    p->SetOpacity(0.5f);
    ui.Frame();
    CHECK(ui.canvas.FindIcon("check")->colour.rgb == ui.pal.danger.rgb);
    const auto pics = ui.canvas.Of(Call::Picture);
    REQUIRE(pics.size() == 1);
    CHECK(pics[0]->colour.rgb == ui.pal.tintDog.rgb);
    CHECK(pics[0]->number == 0.5f);
    CHECK(pics[0]->text == "flute");
}

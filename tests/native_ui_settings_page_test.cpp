// The Settings page (K.4): the key list and its card, the key capture, the play button and the six
// switches. Driven through the Screen with a FakeCanvas, like the other native UI tests: what the page draws
// from a state, and which PageMessage a press or a key sends to the host.
#include "native_ui/pages/settings_page.h"
#include "native_ui/settings_ui.h"
#include "native_ui_test_util.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace nui;
using namespace nui::test;

namespace {

// Windows virtual-key codes used by the tests (the window sends them in KeyEvent::code).
constexpr int kVkTab = 0x09, kVkReturn = 0x0D, kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12,
              kVkEscape = 0x1B, kVkSpace = 0x20, kVkF1 = 0x70, kVkSnapshot = 0x2C;

struct Rig {
    Ui ui;
    SettingsUi settings{ui.screen, "en"};
    std::vector<PageMessage> sent;

    Rig() {
        ui.screen.SetMessageSink([this](const PageMessage& m) { sent.push_back(m); });
        ui.Frame();
        settings.ShowPage(Page::Settings);
        Push(State());
    }
    static UiState State() {
        UiState s;
        s.on = true;
        s.version = "0.5.0";
        s.masterVolume = {50, 0, 100, 100, 5};
        // The host sends a language key too; the page leaves it out (no Language key in 0.5).
        s.keys = {{"read", "F8"}, {"pause", "F7"}, {"language", ""}, {"speech", "F6"}};
        return s;
    }
    void Push(const UiState& s) {
        settings.SetState(s);
        ui.Advance(0.5);
    }
    SettingsPage& Page_() { return *static_cast<SettingsPage*>(settings.Current()); }
    KeyChip* Chip(const char* action) { return Page_().RowFor(action)->Chip(); }
    void ClickChip(const char* action) {
        const PointF c = Centre(*Chip(action));
        ui.Click(c.x, c.y);
        ui.Frame();
    }
    // A key the window would send: the toolkit key, the VK code and the modifiers.
    bool Press(Key key, int code, bool ctrl = false, bool alt = false, bool shift = false, bool win = false,
               bool repeat = false) {
        KeyEvent e;
        e.key = key;
        e.code = code;
        e.ctrl = ctrl;
        e.alt = alt;
        e.shift = shift;
        e.win = win;
        e.repeat = repeat;
        const bool used = ui.screen.HandleKey(e);
        ui.Frame();
        return used;
    }
    bool PressVk(int code, bool ctrl = false, bool alt = false, bool shift = false, bool win = false,
                 bool repeat = false) {
        return Press(Key::Other, code, ctrl, alt, shift, win, repeat);
    }
    // The messages of one type, in order.
    std::vector<PageMessage> Of(PageMessageType t) const {
        std::vector<PageMessage> out;
        for (const PageMessage& m : sent)
            if (m.type == t) out.push_back(m);
        return out;
    }
    std::string Words(const char* id) const { return strings::Text("en", id); }
};

}  // namespace

// ------------------------------------------------------------------------------------- rows ---

TEST_CASE("Settings: one row per Key of the state, the language action left out, the first selected") {
    Rig r;
    const auto& rows = r.Page_().Rows();
    REQUIRE(rows.size() == 3);
    CHECK(rows[0]->Action() == "read");
    CHECK(rows[1]->Action() == "pause");
    CHECK(rows[2]->Action() == "speech");
    CHECK(r.Page_().RowFor("language") == nullptr);

    CHECK(r.Chip("read")->GetState() == KeyChip::State::Set);
    CHECK(r.Chip("read")->Access().value == "F8");
    CHECK(r.Chip("pause")->Access().value == "F7");
    CHECK(r.Chip("speech")->Access().value == "F6");
    CHECK(r.Page_().SelectedAction() == "read");
    CHECK(rows[0]->Selected());
    CHECK_FALSE(rows[1]->Selected());

    // The rows sit where the old page put them: 179, then 41 apart; the chips end at 253.
    CHECK(rows[0]->Abs().y == doctest::Approx(179));
    CHECK(rows[1]->Abs().y == doctest::Approx(220));
    CHECK(rows[2]->Abs().y == doctest::Approx(261));
    CHECK(r.Chip("read")->Abs().Right() == doctest::Approx(253));
    CHECK(r.Chip("read")->Abs().w >= 70);
    CHECK(r.Chip("read")->Abs().h == doctest::Approx(24));
}

TEST_CASE("Settings: a Key with no key shows the word for none, in the none look") {
    Rig r;
    UiState s = Rig::State();
    s.keys[1].key = "";
    r.Push(s);
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::None);
    CHECK(r.Chip("pause")->Access().value == r.Words(strings::id::kKeyNone));
    CHECK(r.Chip("pause")->Access().name.find(r.Words(strings::id::kKeyNone)) != std::string::npos);
}

TEST_CASE("Settings: the card shows the selected Key: title, the lines of its text, the read button") {
    Rig r;
    CHECK(r.Page_().CardTitle()->Text() == r.Words(strings::id::kWindowKeysRead));
    CHECK(r.Page_().CardBody()->PlainText() == r.Words(strings::id::kWindowKeysReadDesc));
    CHECK(r.Page_().ReadButton()->Access().name ==
          strings::Format("en", strings::id::kWindowKeysReadAloud, {{"action", r.Words(strings::id::kWindowKeysRead)}}));
    CHECK(r.Page_().ReadButton()->Tooltip() == r.Page_().ReadButton()->Access().name);

    // The card is where the old page had it, with the wine engraving inside and the card colour behind.
    r.ui.Frame();
    bool card = false, wine = false;
    for (const Call* c : r.ui.canvas.Of(Call::FillRect))
        card |= c->rect.x == 273 && c->rect.y == 160 && c->rect.w == 355 && c->rect.h == 342 &&
                c->colour.rgb == r.ui.pal.card.rgb;
    for (const Call* c : r.ui.canvas.Of(Call::Picture)) wine |= c->text == "wine";
    CHECK(card);
    CHECK(wine);
    // The read button (the Master volume's speaker icon, no ring) is the first thing of the head: its icon
    // sits on the card's 20 padding line, the title 10 after the icon, the body 22 under the head.
    CHECK(r.Page_().ReadButton()->Abs().w == 28);  // the hit area
    CHECK(r.Page_().ReadButton()->Abs().h == 28);
    CHECK(r.Page_().ReadButton()->Abs().Right() < r.Page_().CardTitle()->Abs().x);
    CHECK(r.Page_().CardTitle()->Abs().x == doctest::Approx(273 + 20 + 24 + 10));
    CHECK(r.Page_().CardTitle()->Abs().y == doctest::Approx(178));
    const Call* icon = nullptr;  // the card's one (the Master volume has its own at the bottom of the window)
    for (const Call* c : r.ui.canvas.Of(Call::Icon)) {
        if (c->text == "volume-05" && c->rect.y < 300) icon = c;
    }
    REQUIRE(icon != nullptr);
    CHECK(r.ui.canvas.FindIcon("play-03") == nullptr);  // the old play icon is gone
    CHECK(icon->rect.x == doctest::Approx(273 + 20));
    CHECK(icon->rect.w == 24);
    CHECK(icon->colour.rgb == r.ui.pal.accent.rgb);
    CHECK(icon->rect.Right() + 10 == doctest::Approx(r.Page_().CardTitle()->Abs().x));
    // No ring around it: nothing is stroked inside the button's rectangle while it is idle and unfocused.
    bool ring = false;
    for (const Call* c : r.ui.canvas.Of(Call::StrokeRect)) {
        ring |= c->rect.x >= r.Page_().ReadButton()->Abs().x - 1 && c->rect.Right() <= r.Page_().ReadButton()->Abs().Right() + 1;
    }
    CHECK_FALSE(ring);
    CHECK(r.Page_().CardBody()->Abs().y == doctest::Approx(228));
}

TEST_CASE("Settings: the headings and the six switches are the page's words") {
    Rig r;
    CHECK(r.ui.canvas.FindText(r.Words(strings::id::kWindowKeysTitle)));
    CHECK(r.ui.canvas.FindText(r.Words(strings::id::kWindowSettingsMisc)));
    CHECK(r.Page_().RunAtStartup()->Access().name == r.Words(strings::id::kWindowSettingsRunAtStartup));
    CHECK(r.Page_().StartMinimised()->Access().name == r.Words(strings::id::kWindowSettingsStartMinimised));
    CHECK(r.Page_().RunAsAdmin()->Access().name == r.Words(strings::id::kWindowSettingsRunAsAdmin));
    CHECK(r.Page_().DarkMode()->Access().name == r.Words(strings::id::kWindowSettingsDarkMode));
    CHECK(r.Page_().LeftHanded()->Access().name == r.Words(strings::id::kWindowSettingsLeftHanded));
    CHECK(r.Page_().KeepWordMark()->Access().name == r.Words(strings::id::kWindowSettingsKeepWordMark));
    // 377, then 28 apart, 250 wide.
    CHECK(r.Page_().RunAtStartup()->Abs().y == doctest::Approx(377));
    CHECK(r.Page_().LeftHanded()->Abs().y == doctest::Approx(377 + 4 * 28));
    CHECK(r.Page_().KeepWordMark()->Abs().y == doctest::Approx(377 + 5 * 28));
    CHECK(r.Page_().KeepWordMark()->Abs().y + r.Page_().KeepWordMark()->Abs().h <= 600);  // inside the 800 x 600 window
    CHECK(r.Page_().RunAtStartup()->Abs().w == doctest::Approx(250));
}

// ------------------------------------------------------------------------------- selection ---

TEST_CASE("Settings: hovering, focusing or pressing a row selects it, and the card follows") {
    Rig r;
    // Hover the pause row's name (left of the chip).
    const RectF row = r.Page_().RowFor("pause")->Abs();
    r.ui.Move(row.x + 20, row.y + 12);
    CHECK(r.Page_().SelectedAction() == "pause");
    CHECK(r.Page_().CardTitle()->Text() == r.Words(strings::id::kWindowKeysPause));
    CHECK(r.Page_().RowFor("pause")->Selected());
    CHECK_FALSE(r.Page_().RowFor("read")->Selected());

    // Hover the speech chip.
    const PointF c = Centre(*r.Chip("speech"));
    r.ui.Move(c.x, c.y);
    CHECK(r.Page_().SelectedAction() == "speech");

    // Keyboard focus on the read chip.
    r.Chip("read")->Focus();
    CHECK(r.Page_().SelectedAction() == "read");
    CHECK(r.Page_().CardBody()->PlainText() == r.Words(strings::id::kWindowKeysReadDesc));

    // A press on the name selects it, and sends nothing.
    const RectF row2 = r.Page_().RowFor("speech")->Abs();
    r.ui.Leave();
    r.ui.Down(row2.x + 20, row2.y + 12);
    r.ui.Up(row2.x + 20, row2.y + 12);
    CHECK(r.Page_().SelectedAction() == "speech");
    CHECK(r.sent.empty());
}

TEST_CASE("Settings: the selection is the page's own: a new state does not move it") {
    Rig r;
    const PointF c = Centre(*r.Chip("speech"));
    r.ui.Move(c.x, c.y);
    CHECK(r.Page_().SelectedAction() == "speech");
    r.Push(Rig::State());
    CHECK(r.Page_().SelectedAction() == "speech");
    // A state without that Key sends the selection back to the first row.
    UiState s = Rig::State();
    s.keys = {{"read", "F8"}, {"pause", "F7"}};
    r.Push(s);
    CHECK(r.Page_().Rows().size() == 2);
    CHECK(r.Page_().SelectedAction() == "read");
}

// ---------------------------------------------------------------------------------- capture ---

TEST_CASE("Settings capture: a press on a chip sends captureKey, and the chip waits with the hint shown") {
    Rig r;
    CHECK(r.Page_().Hint()->Text().empty());
    r.ClickChip("pause");
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::CaptureKey);
    CHECK(r.sent[0].action == "pause");
    CHECK(r.Page_().CapturingAction() == "pause");
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::Capturing);
    CHECK(r.Chip("pause")->Access().value == r.Words(strings::id::kWindowKeysCapturing));
    CHECK(r.Page_().Hint()->Text() == r.Words(strings::id::kWindowKeysCaptureHint));
    CHECK(r.Page_().Hint()->Access().liveRegion);
    CHECK(r.ui.screen.Focused() == r.Chip("pause"));
    CHECK(r.ui.screen.Focused()->CapturesAllKeys());
    CHECK(r.Page_().SelectedAction() == "pause");
    // The chip is wider than a normal one while it says "Press a key" (min width 70 still holds).
    CHECK(r.Chip("pause")->Abs().Right() == doctest::Approx(253));

    // Another press on the same chip does not start a second wait.
    r.ClickChip("pause");
    CHECK(r.Of(PageMessageType::CaptureKey).size() == 1);
}

TEST_CASE("Settings capture: while a chip waits the card stays on that Key, whatever the pointer does") {
    Rig r;
    r.ClickChip("pause");
    const RectF row = r.Page_().RowFor("speech")->Abs();
    r.ui.Move(row.x + 20, row.y + 12);  // hover another row
    CHECK(r.Page_().SelectedAction() == "pause");
    CHECK(r.Page_().CardTitle()->Text() == r.Words(strings::id::kWindowKeysPause));
}

TEST_CASE("Settings capture: a key with its modifiers is sent as setKey, and the chip shows the state's key") {
    Rig r;
    r.ClickChip("read");
    r.sent.clear();
    CHECK(r.PressVk('R', /*ctrl*/ true, /*alt*/ true));
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::SetKey);
    CHECK(r.sent[0].action == "read");
    CHECK(r.sent[0].key == "Ctrl+Alt+R");
    // The wait is over; the chip shows the Key of the last state until the host answers; it keeps the focus.
    CHECK(r.Page_().CapturingAction().empty());
    CHECK(r.Chip("read")->GetState() == KeyChip::State::Set);
    CHECK(r.Chip("read")->Access().value == "F8");
    CHECK(r.Page_().Hint()->Text().empty());
    CHECK(r.ui.screen.Focused() == r.Chip("read"));
    CHECK_FALSE(r.ui.screen.Focused()->CapturesAllKeys());

    // The host answers with a state that has the new key.
    UiState s = Rig::State();
    s.keys[0].key = "Ctrl+Alt+R";
    r.Push(s);
    CHECK(r.Chip("read")->Access().value == "Ctrl+Alt+R");
}

TEST_CASE("Settings capture: F-keys, named keys and every modifier are sent in the settings.json spelling") {
    struct Case {
        int vk;
        bool ctrl, alt, shift, win;
        const char* key;
    };
    const Case cases[] = {{kVkF1 + 8, false, false, false, false, "F9"},
                          {kVkF1 + 9, false, false, false, false, "F10"},  // F10 arrives as a system key
                          {kVkSpace, true, false, false, false, "Ctrl+Space"},
                          {kVkReturn, false, true, false, false, "Alt+Enter"},
                          {kVkSnapshot, false, false, false, false, "PrintScreen"},  // key-up only in Windows
                          {'Q', true, true, true, true, "Ctrl+Alt+Shift+Windows+Q"},
                          {'5', false, false, true, false, "Shift+5"}};
    for (const Case& c : cases) {
        Rig r;
        r.ClickChip("speech");
        r.sent.clear();
        r.PressVk(c.vk, c.ctrl, c.alt, c.shift, c.win);
        REQUIRE(r.sent.size() == 1);
        CHECK(r.sent[0].type == PageMessageType::SetKey);
        CHECK(r.sent[0].action == "speech");
        CHECK(r.sent[0].key == c.key);
    }
}

TEST_CASE("Settings capture: a modifier alone, a held key and a key with no name keep waiting") {
    Rig r;
    r.ClickChip("pause");
    r.sent.clear();
    // Each modifier on its own (with itself as the modifier flag, as Windows reports it).
    r.PressVk(kVkControl, true);
    r.PressVk(kVkShift, false, false, true);
    r.PressVk(kVkMenu, false, true);
    r.PressVk(0x5B, false, false, false, true);  // the left Windows key
    r.PressVk(0xA2, true);                       // left Control
    // A key with no name: Backspace, ';'.
    r.PressVk(0x08);
    r.PressVk(0xBA, true);
    // Auto-repeat of a usable key is not an answer.
    r.PressVk(kVkF1 + 8, false, false, false, false, /*repeat*/ true);
    CHECK(r.sent.empty());
    CHECK(r.Page_().CapturingAction() == "pause");
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::Capturing);

    // The real key then comes, and it is the answer.
    r.PressVk(kVkF1 + 8, true);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].key == "Ctrl+F9");
}

TEST_CASE("Settings capture: Esc cancels, the chip keeps the focus and shows its key again") {
    Rig r;
    r.ClickChip("pause");
    r.sent.clear();
    CHECK(r.Press(Key::Escape, kVkEscape));
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
    CHECK(r.sent[0].action == "pause");
    CHECK(r.Page_().CapturingAction().empty());
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::Set);
    CHECK(r.Chip("pause")->Access().value == "F7");
    CHECK(r.Page_().Hint()->Text().empty());
    CHECK(r.ui.screen.Focused() == r.Chip("pause"));

    // The chip no longer captures: Esc now does nothing, and sends nothing.
    r.sent.clear();
    r.Press(Key::Escape, kVkEscape);
    CHECK(r.sent.empty());
}

TEST_CASE("Settings capture: Tab cancels and the focus moves on to the next widget") {
    Rig r;
    r.ClickChip("pause");
    r.sent.clear();
    r.Press(Key::Tab, kVkTab);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
    CHECK(r.Page_().CapturingAction().empty());
    CHECK(r.ui.screen.Focused() == r.Chip("speech"));  // the next chip in the tab order
    CHECK(r.Chip("pause")->Access().value == "F7");
    CHECK(r.Of(PageMessageType::SetKey).empty());

    // Shift+Tab goes back.
    r.ClickChip("speech");
    r.sent.clear();
    r.Press(Key::Tab, kVkTab, false, false, /*shift*/ true);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
    CHECK(r.ui.screen.Focused() == r.Chip("pause"));
}

TEST_CASE("Settings capture: a press outside, or the focus going away, cancels") {
    {
        Rig r;
        r.ClickChip("pause");
        r.sent.clear();
        r.ui.Click(700, 300);  // bare paper
        REQUIRE(r.sent.size() == 1);
        CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
        CHECK(r.sent[0].action == "pause");
        CHECK(r.Page_().CapturingAction().empty());
        CHECK(r.Chip("pause")->GetState() == KeyChip::State::Set);
    }
    {
        Rig r;
        r.ClickChip("pause");
        r.sent.clear();
        r.ui.screen.SetFocus(nullptr);  // the window went inactive
        REQUIRE(r.sent.size() == 1);
        CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
        CHECK(r.Page_().CapturingAction().empty());
    }
    {
        Rig r;  // a press on a switch takes the focus too
        r.ClickChip("pause");
        r.sent.clear();
        const PointF c = Centre(*r.Page_().DarkMode());
        r.ui.Click(c.x, c.y);
        REQUIRE(r.sent.size() == 2);
        CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
        CHECK(r.sent[1].type == PageMessageType::SetTheme);
    }
    {
        Rig r;  // the page is replaced (App opens another page): the wait ends
        r.ClickChip("pause");
        r.sent.clear();
        r.settings.ShowPage(Page::Help);
        REQUIRE(r.sent.size() == 1);
        CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
    }
}

TEST_CASE("Settings capture: a press on another chip ends the first wait and starts the second") {
    Rig r;
    r.ClickChip("read");
    r.sent.clear();
    r.ClickChip("speech");
    REQUIRE(r.sent.size() == 2);
    CHECK(r.sent[0].type == PageMessageType::CancelKeyCapture);
    CHECK(r.sent[0].action == "read");
    CHECK(r.sent[1].type == PageMessageType::CaptureKey);
    CHECK(r.sent[1].action == "speech");
    CHECK(r.Page_().CapturingAction() == "speech");
    CHECK(r.Chip("read")->GetState() == KeyChip::State::Set);
    CHECK(r.Chip("speech")->GetState() == KeyChip::State::Capturing);
    CHECK(r.Page_().SelectedAction() == "speech");
}

TEST_CASE("Settings capture: a new state during the wait keeps it; a refused key puts the old one back") {
    Rig r;
    r.ClickChip("pause");
    r.Push(Rig::State());
    CHECK(r.Page_().CapturingAction() == "pause");
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::Capturing);

    r.PressVk('A');  // a letter alone: the host will refuse it
    r.Push(Rig::State());  // its answer: a state with the old key
    CHECK(r.Chip("pause")->Access().value == "F7");
    CHECK(r.Chip("pause")->GetState() == KeyChip::State::Set);
}

TEST_CASE("Settings capture: Enter or Space on a focused chip starts the wait (and the held key does not answer it)") {
    Rig r;
    r.Chip("speech")->Focus();
    r.Press(Key::Space, kVkSpace);
    REQUIRE(r.Of(PageMessageType::CaptureKey).size() == 1);
    CHECK(r.Page_().CapturingAction() == "speech");
    // The key repeats while it is held down: not an answer.
    r.Press(Key::Space, kVkSpace, false, false, false, false, /*repeat*/ true);
    CHECK(r.Of(PageMessageType::SetKey).empty());
    CHECK(r.Page_().CapturingAction() == "speech");
}

TEST_CASE("Settings: opening on a Key selects it, and with capture starts that chip's wait") {
    Rig r;
    ShowOptions o;
    o.selectAction = "speech";
    r.settings.ShowPage(Page::Settings, o);
    CHECK(r.Page_().SelectedAction() == "speech");
    CHECK(r.Page_().CardTitle()->Text() == r.Words(strings::id::kWindowKeysSpeech));
    CHECK(r.Page_().CapturingAction().empty());

    ShowOptions c;
    c.capture = "pause";
    r.sent.clear();
    r.settings.ShowPage(Page::Settings, c);
    CHECK(r.Page_().CapturingAction() == "pause");
    CHECK(r.Page_().SelectedAction() == "pause");
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::CaptureKey);

    // Another page ignores the options; an unknown Key changes nothing.
    ShowOptions bad;
    bad.selectAction = "nope";
    r.settings.ShowPage(Page::Help, bad);
    CHECK(r.settings.CurrentPage() == Page::Help);
}

// ------------------------------------------------------------------------------- read button ---

TEST_CASE("Settings: the read button sends readKeyCard for the selected Key, never the text") {
    Rig r;
    r.sent.clear();
    PointF c = Centre(*r.Page_().ReadButton());
    r.ui.Click(c.x, c.y);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::ReadKeyCard);
    CHECK(r.sent[0].action == "read");
    CHECK(r.sent[0].key.empty());

    // Hover another row: the card follows, and the button reads that Key.
    const RectF row = r.Page_().RowFor("speech")->Abs();
    r.ui.Move(row.x + 20, row.y + 12);
    r.ui.Frame();
    c = Centre(*r.Page_().ReadButton());
    r.ui.Click(c.x, c.y);
    REQUIRE(r.sent.size() == 2);
    CHECK(r.sent[1].action == "speech");
}

// The owner's rule: the read button is on the card of every Key, not only Read's. The card is one widget for
// the three Keys, so this walks them all: the speaker icon left of that Key's title, the button's name for that
// Key, and a press that reads that Key's card.
TEST_CASE("Settings: every Key's card has the read button left of its title, and it reads that Key") {
    struct KeyCase {
        const char* action;
        const char* title;  // a string id (strings/string_ids.h): the Key's full name, its card's title
    };
    const KeyCase keys[] = {{"read", strings::id::kWindowKeysRead},
                            {"pause", strings::id::kWindowKeysPause},
                            {"speech", strings::id::kWindowKeysSpeech}};
    Rig r;
    for (const KeyCase& k : keys) {
        CAPTURE(k.action);
        // Select the Key the way a person does: the pointer over its row's name.
        const RectF row = r.Page_().RowFor(k.action)->Abs();
        r.ui.Move(row.x + 20, row.y + 12);
        r.ui.Frame();
        REQUIRE(r.Page_().SelectedAction() == k.action);
        CHECK(r.Page_().CardTitle()->Text() == r.Words(k.title));

        Widget* button = r.Page_().ReadButton();
        CHECK(button->Visible());
        CHECK(button->Enabled());
        CHECK(button->Access().name ==
              strings::Format("en", strings::id::kWindowKeysReadAloud, {{"action", r.Words(k.title)}}));
        CHECK(button->Abs().Right() < r.Page_().CardTitle()->Abs().x);

        // The speaker icon is drawn in the card's head, left of this title.
        const Call* icon = nullptr;
        for (const Call* c : r.ui.canvas.Of(Call::Icon)) {
            if (c->text == "volume-05" && c->rect.y < 300) icon = c;
        }
        REQUIRE(icon != nullptr);
        CHECK(icon->rect.Right() + 10 == doctest::Approx(r.Page_().CardTitle()->Abs().x));

        r.sent.clear();
        const PointF c = Centre(*button);
        r.ui.Click(c.x, c.y);
        REQUIRE(r.sent.size() == 1);
        CHECK(r.sent[0].type == PageMessageType::ReadKeyCard);
        CHECK(r.sent[0].action == k.action);
    }
}

// ---------------------------------------------------------------------------------- switches ---

TEST_CASE("Settings: the switches show the state, and a press changes nothing until the next state") {
    Rig r;
    UiState s = Rig::State();
    s.runAtStartup = true;
    s.startMinimised = false;
    s.runAsAdmin = true;
    s.dark = true;
    s.leftHanded = true;
    s.keepWordMark = true;
    r.Push(s);
    CHECK(r.Page_().RunAtStartup()->Checked());
    CHECK_FALSE(r.Page_().StartMinimised()->Checked());
    CHECK(r.Page_().RunAsAdmin()->Checked());
    CHECK(r.Page_().DarkMode()->Checked());
    CHECK(r.Page_().LeftHanded()->Checked());
    CHECK(r.Page_().KeepWordMark()->Checked());
}

TEST_CASE("Settings: each switch sends its own message with the opposite value") {
    Rig r;
    UiState s = Rig::State();
    s.runAtStartup = true;
    s.startMinimised = true;
    s.runAsAdmin = false;
    s.dark = false;
    s.leftHanded = false;
    s.keepWordMark = false;
    r.Push(s);
    r.sent.clear();

    auto click = [&](SwitchRow* w) {
        const PointF c = Centre(*w);
        r.ui.Click(c.x, c.y);
    };
    click(r.Page_().RunAtStartup());
    click(r.Page_().StartMinimised());
    click(r.Page_().RunAsAdmin());
    click(r.Page_().DarkMode());
    click(r.Page_().LeftHanded());
    click(r.Page_().KeepWordMark());
    REQUIRE(r.sent.size() == 6);
    CHECK(r.sent[0].type == PageMessageType::SetRunAtStartup);
    CHECK(r.sent[0].on == false);
    CHECK(r.sent[1].type == PageMessageType::SetStartMinimised);
    CHECK(r.sent[1].on == false);
    CHECK(r.sent[2].type == PageMessageType::SetRunAsAdmin);
    CHECK(r.sent[2].on == true);
    CHECK(r.sent[3].type == PageMessageType::SetTheme);
    CHECK(r.sent[3].theme == "dark");
    CHECK(r.sent[4].type == PageMessageType::SetLeftHanded);
    CHECK(r.sent[4].on == true);
    CHECK(r.sent[5].type == PageMessageType::SetKeepWordMark);
    CHECK(r.sent[5].on == true);
    // Nothing flipped by itself.
    CHECK(r.Page_().RunAtStartup()->Checked());
    CHECK_FALSE(r.Page_().DarkMode()->Checked());
}

TEST_CASE("Settings: dark mode sends the other of the theme in use, not of the stored choice") {
    Rig r;
    UiState s = Rig::State();
    s.theme = "system";  // the stored choice
    s.dark = true;       // Windows is dark: the theme in use
    r.Push(s);
    r.sent.clear();
    const PointF c = Centre(*r.Page_().DarkMode());
    r.ui.Click(c.x, c.y);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].theme == "light");
}

TEST_CASE("Settings: Start minimised is disabled unless Run at startup is on, and sends nothing then") {
    Rig r;
    UiState s = Rig::State();
    s.runAtStartup = false;
    s.startMinimised = true;
    r.Push(s);
    CHECK_FALSE(r.Page_().StartMinimised()->Enabled());
    CHECK(r.Page_().StartMinimised()->Checked());  // still shows its setting
    CHECK(r.Page_().StartMinimised()->Access().focusable);  // keeps the focus, like aria-disabled
    r.sent.clear();
    const PointF c = Centre(*r.Page_().StartMinimised());
    r.ui.Click(c.x, c.y);
    r.Page_().StartMinimised()->Focus();
    r.Press(Key::Space, kVkSpace);  // the keyboard does not work it either
    CHECK(r.sent.empty());

    s.runAtStartup = true;
    r.Push(s);
    CHECK(r.Page_().StartMinimised()->Enabled());
    r.ui.Click(c.x, c.y);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::SetStartMinimised);
    CHECK(r.sent[0].on == false);
}

TEST_CASE("Settings: a switch is drawn lighter while it is disabled") {
    Rig r;
    UiState s = Rig::State();
    s.runAtStartup = false;
    r.Push(s);
    r.ui.Frame();
    CHECK(r.ui.canvas.HasOpacity(0.5f));
    s.runAtStartup = true;
    r.Push(s);
    r.ui.Frame();
    CHECK_FALSE(r.ui.canvas.HasOpacity(0.5f));
}

TEST_CASE("Settings: the page does not add widgets when a state arrives again") {
    Rig r;
    auto count = [](const Widget& w, auto&& self) -> size_t {
        size_t n = 1;
        for (const auto& c : w.Children()) n += self(*c, self);
        return n;
    };
    const size_t before = count(r.ui.screen.Root(), count);
    r.Push(Rig::State());
    r.Push(Rig::State());
    CHECK(count(r.ui.screen.Root(), count) == before);
}

// ------------------------------------------------------------------- the read button wiggles ---

namespace {

// The calls between the first PushRotation and its PopRotation (empty when there is none).
std::vector<Call> RotatedCalls(const FakeCanvas& c) {
    std::vector<Call> out;
    bool in = false;
    for (const Call& k : c.calls) {
        if (k.kind == Call::PopRotation) in = false;
        if (in) out.push_back(k);
        if (k.kind == Call::PushRotation) in = true;
    }
    return out;
}

}  // namespace

TEST_CASE("Settings: the read button is still when the page opens and wakes the Screen for 5 s later") {
    Rig r;
    const double t0 = r.ui.now;  // the Rig drew the page at this time: the button's first paint
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 0);
    CHECK(r.ui.canvas.Count(Call::PopRotation) == 0);
    REQUIRE(r.ui.screen.NextWakeTime().has_value());
    CHECK(*r.ui.screen.NextWakeTime() == doctest::Approx(t0 + 5.0));
    CHECK_FALSE(r.ui.screen.NeedsTick());  // nothing moves: no frame timer between wiggles

    // Four seconds on: still nothing, and the wake-up is where it was.
    r.ui.now = t0 + 4.0;
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 0);
    CHECK(*r.ui.screen.NextWakeTime() == doctest::Approx(t0 + 5.0));
}

TEST_CASE("Settings: the read button shakes for 0.6 s every 5 s, only the icon, about its centre") {
    Rig r;
    const double t0 = r.ui.now;

    // The wake-up time comes: the Screen asks for a repaint, and the frame shows the shake.
    r.ui.screen.Tick(t0 + 5.0 + 0.075);
    CHECK(r.ui.screen.NeedsRepaint());
    r.ui.now = t0 + 5.0 + 0.075;
    r.ui.Frame();
    REQUIRE(r.ui.canvas.Count(Call::PushRotation) == 1);
    CHECK(r.ui.canvas.Count(Call::PopRotation) == 1);
    const Call* push = r.ui.canvas.Of(Call::PushRotation)[0];
    CHECK(push->number == doctest::Approx(7.0f));
    const std::vector<Call> inside = RotatedCalls(r.ui.canvas);
    REQUIRE(inside.size() == 1);  // only the icon turns, not the hover tile or the title
    CHECK(inside[0].kind == Call::Icon);
    CHECK(inside[0].text == "volume-05");
    CHECK(push->points[0].x == doctest::Approx(inside[0].rect.x + 12));
    CHECK(push->points[0].y == doctest::Approx(inside[0].rect.y + 12));
    CHECK(r.ui.screen.NeedsTick());  // frames while it moves
    REQUIRE(r.ui.screen.NextWakeTime().has_value());
    CHECK(*r.ui.screen.NextWakeTime() == doctest::Approx(t0 + 10.0));

    // Halfway through, frames go on; after 0.6 s the icon is straight and the timer is not needed.
    r.ui.now = t0 + 5.2;
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 1);
    r.ui.now = t0 + 5.0 + kWiggleSeconds + 0.001;
    r.ui.Frame();  // the Tick of this frame paints the last (straight) picture
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 0);
    CHECK_FALSE(r.ui.screen.NeedsTick());
    CHECK(*r.ui.screen.NextWakeTime() == doctest::Approx(t0 + 10.0));

    // The second wiggle, at 10 s.
    r.ui.now = t0 + 10.0 + 0.075;
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 1);
}

TEST_CASE("Settings: with reduced motion the read button never shakes and nothing wakes the Screen") {
    Rig r;
    const double t0 = r.ui.now;
    r.ui.screen.SetReducedMotion(true);
    r.ui.Frame();
    CHECK_FALSE(r.ui.screen.NextWakeTime().has_value());
    r.ui.now = t0 + 5.0 + 0.075;
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 0);
    CHECK_FALSE(r.ui.screen.NeedsTick());
    CHECK_FALSE(r.ui.screen.NextWakeTime().has_value());
    // Back to full motion: the button counts from its first paint, which was long ago.
    r.ui.screen.SetReducedMotion(false);
    r.ui.now = t0 + 10.0 + 0.075;
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::PushRotation) == 1);
}

TEST_CASE("Settings: another page asks for no wake-up") {
    Rig r;
    r.settings.ShowPage(Page::Home);
    r.ui.Advance(0.1);
    CHECK_FALSE(r.ui.screen.NextWakeTime().has_value());
}

TEST_CASE("Settings: the read button still reads the key card with Enter, whatever its look") {
    Rig r;
    r.sent.clear();
    r.Page_().ReadButton()->Focus();
    r.ui.Key(nui::Key::Enter);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::ReadKeyCard);
    CHECK(r.Page_().ReadButton()->Access().role == Role::Button);
}

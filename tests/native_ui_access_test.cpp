// The accessible view of the native Settings window (K.5): which widgets a screen reader is offered on each page
// and sheet, their names and roles (from strings/), that no focusable widget is unnamed, the modal sheet that
// hides the page, the actions a screen reader can ask for, and the changes AccessTracker reports (focus,
// properties, structure, live text). Driven through the Screen with a FakeCanvas, like the other native_ui
// tests; the UI Automation glue itself is tested by lexia_uia_test (ctest uia_tree).
#include "native_ui/access_tree.h"
#include "native_ui/pages/voices_page.h"
#include "native_ui/settings_ui.h"
#include "native_ui/sheet.h"
#include "native_ui_test_util.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

#include <doctest/doctest.h>

#include <set>
#include <string>
#include <vector>

using namespace nui;
using namespace nui::test;

namespace {

constexpr const char* kLjspeech = "en_US-ljspeech-medium";
constexpr const char* kAlba = "en_GB-alba-medium";

std::string T(const char* id) { return strings::Text("en", id); }

UiState SampleState(bool on = false, const std::string& active = kAlba) {
    UiState s;
    s.on = on;
    s.version = "0.6.0";
    s.masterVolume = {50, 0, 100, 100, 5};
    s.keys = {{"read", "F8"}, {"pause", "F7"}, {"speech", "F6"}};
    UiVoices v;
    v.installed = {kLjspeech, kAlba};
    v.active = active;
    v.removable = {kAlba};
    s.controls["speed"] = {1.0, 0.5, 2.0, 1.0, 0.05};
    s.controls["pitch"] = {0.0, -6, 6, 0, 0.5};
    s.controls["volume"] = {1.0, 0, 2.0, 1.0, 0.05};
    s.voices["eng"] = v;
    return s;
}

struct Rig {
    Ui ui;
    SettingsUi settings{ui.screen, "en"};
    std::vector<PageMessage> sent;

    Rig() {
        ui.screen.SetMessageSink([this](const PageMessage& m) { sent.push_back(m); });
        settings.SetState(SampleState());
        ui.Advance(0.5);
    }
    void Show(Page p) {
        settings.ShowPage(p);
        ui.Advance(0.5);
    }
    Screen& S() { return ui.screen; }
    // The offered widget with this name and role, anywhere in the projection.
    Widget* Find(const std::string& name, Role role = Role::None) {
        return FindIn(nullptr, name, role);
    }
    Widget* FindIn(const Widget* parent, const std::string& name, Role role) {
        for (Widget* w : AccessChildren(ui.screen, parent)) {
            const AccessInfo a = w->Access();
            if (a.name == name && (role == Role::None || a.role == role)) return w;
            if (Widget* hit = FindIn(w, name, role)) return hit;
        }
        return nullptr;
    }
    // Every offered widget, in document order.
    void All(const Widget* parent, std::vector<Widget*>& out) {
        for (Widget* w : AccessChildren(ui.screen, parent)) {
            out.push_back(w);
            All(w, out);
        }
    }
    std::vector<Widget*> Everything() {
        std::vector<Widget*> out;
        All(nullptr, out);
        return out;
    }
};

// Records what the tracker says.
struct Log : AccessObserver {
    std::vector<int> focus;
    struct Prop {
        int id;
        unsigned changes;
        AccessInfo before, after;
    };
    std::vector<Prop> props;
    std::vector<int> structure, live;
    void FocusChanged(int id) override { focus.push_back(id); }
    void PropertiesChanged(int id, unsigned changes, const AccessInfo& b, const AccessInfo& a) override {
        props.push_back({id, changes, b, a});
    }
    void StructureChanged(int id) override { structure.push_back(id); }
    void LiveRegionChanged(int id) override { live.push_back(id); }
    void Clear() { focus.clear(); props.clear(); structure.clear(); live.clear(); }
    bool HasStructure(int id) const {
        for (int s : structure) {
            if (s == id) return true;
        }
        return false;
    }
};

}  // namespace

// ------------------------------------------------------------------------- every page ---

TEST_CASE("access: on every page, no focusable widget is unnamed and every automation id is unique") {
    Rig r;
    for (Page p : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
        CAPTURE(PageName(p));
        r.Show(p);
        // Every widget Tab can reach is offered, and called something.
        for (Widget* w : r.S().FocusOrder()) {
            const Widget* offered = FindAccessible(r.S(), w->Id());
            INFO("a focusable widget that a screen reader is not offered, id " << w->Id());
            REQUIRE(offered == w);
            const AccessInfo a = w->Access();
            INFO("a focusable widget without a name, role " << static_cast<int>(a.role) << ", id " << w->Id());
            CHECK_FALSE(AccessName(a).empty());
            CHECK(a.focusable);
        }
        // Unique ids.
        std::set<std::string> ids;
        for (Widget* w : r.Everything()) {
            const std::string id = AutomationIdOf(r.S(), *w);
            INFO("automation id " << id);
            CHECK_FALSE(id.empty());
            CHECK(ids.insert(id).second);
        }
        // Nothing is offered twice and no unnamed group or picture slips in.
        for (Widget* w : r.Everything()) {
            const AccessInfo a = w->Access();
            if (a.role == Role::Group || a.role == Role::Pane || a.role == Role::Image) CHECK_FALSE(a.name.empty());
        }
    }
}

TEST_CASE("access: the frame offers the six pages as selectable links and the Master volume as a slider") {
    Rig r;
    r.Show(Page::Voices);
    struct Link {
        const char* id;
        bool current;
    };
    for (Link l : {Link{strings::id::kWindowNavHome, false}, Link{strings::id::kWindowNavVoices, true},
                   Link{strings::id::kWindowNavSettings, false}, Link{strings::id::kWindowNavReport, false},
                   Link{strings::id::kWindowNavHelp, false}, Link{strings::id::kWindowNavAbout, false}}) {
        CAPTURE(l.id);
        Widget* w = r.Find(T(l.id), Role::Link);
        REQUIRE(w);
        const AccessInfo a = w->Access();
        CHECK(a.selectable);
        CHECK(a.selected == l.current);
    }
    Widget* vol = r.Find(T(strings::id::kWindowVolume), Role::Slider);
    REQUIRE(vol);
    const AccessInfo a = vol->Access();
    CHECK(a.hasRange);
    CHECK(a.current == 50);
    CHECK(a.min == 0);
    CHECK(a.max == 100);
    CHECK(a.value == "50%");
    // The two navigation groups are named, so a screen reader can tell them apart.
    CHECK(r.Find(T(strings::id::kWindowNavMain), Role::Group));
    CHECK(r.Find(T(strings::id::kWindowNavMore), Role::Group));
}

TEST_CASE("access: the window's minimise and close buttons are two named Buttons on every page (K10)") {
    Rig r;
    for (Page p : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
        CAPTURE(PageName(p));
        r.Show(p);
        Widget* minimise = r.Find(T(strings::id::kWindowMinimise), Role::Button);
        Widget* close = r.Find(T(strings::id::kWindowClose), Role::Button);
        REQUIRE(minimise);
        REQUIRE(close);
        CHECK(minimise != close);
        CHECK(AutomationIdOf(r.S(), *minimise) == "frame.minimise");
        CHECK(AutomationIdOf(r.S(), *close) == "frame.close");
        for (Widget* w : {minimise, close}) {
            const AccessInfo a = w->Access();
            CHECK(a.role == Role::Button);
            CHECK(a.focusable);
            CHECK(a.enabled);
            CHECK(a.toggle == Toggle::None);  // a plain button, not a toggle
        }
    }
}

TEST_CASE("access: Home offers the power button as a toggle and the enable button") {
    Rig r;
    r.Show(Page::Home);
    Widget* power = r.Find(T(strings::id::kWindowHomePower), Role::Button);
    REQUIRE(power);
    CHECK(power->Access().toggle == Toggle::Off);
    Widget* enable = r.Find(T(strings::id::kWindowHomeEnable), Role::Button);
    REQUIRE(enable);
    CHECK(AutomationIdOf(r.S(), *power) != AutomationIdOf(r.S(), *enable));
}

TEST_CASE("access: Voices offers the Voice choice, its buttons and three sliders with ranges") {
    Rig r;
    r.Show(Page::Voices);
    Widget* voice = r.Find(T(strings::id::kWindowVoiceVoice), Role::ComboBox);
    REQUIRE(voice);
    CHECK(voice->Access().value == kAlba);
    CHECK(voice->Access().expanded == Tri::No);
    CHECK(r.Find(T(strings::id::kWindowVoiceDownload), Role::Button));
    CHECK(r.Find(T(strings::id::kWindowVoiceAddFromFile), Role::Button));
    CHECK(r.Find(T(strings::id::kWindowVoiceDelete), Role::Button));
    for (const char* id : {strings::id::kWindowVoiceSpeed, strings::id::kWindowVoicePitch, strings::id::kWindowVoiceVolume}) {
        CAPTURE(id);
        Widget* s = r.Find(T(id), Role::Slider);
        REQUIRE(s);
        CHECK(s->Access().hasRange);
        CHECK(s->Access().step > 0);
    }
    CHECK(r.Find(T(strings::id::kWindowVoiceSavePreset), Role::Link));
    CHECK(r.Find(T(strings::id::kWindowVoiceLoadPreset), Role::Link));
}

TEST_CASE("access: Settings offers the key rows with their chips, the card and six switches") {
    Rig r;
    r.Show(Page::Settings);
    for (const char* id : {strings::id::kWindowKeysReadRow, strings::id::kWindowKeysPauseRow, strings::id::kWindowKeysSpeechRow}) {
        Widget* row = r.Find(T(id), Role::ListItem);
        REQUIRE(row);
        CHECK(row->Access().selectable);
        // The chip is inside its row and says both the key and what it is for.
        const std::vector<Widget*> kids = AccessChildren(r.S(), row);
        REQUIRE(kids.size() == 1);
        CHECK(kids[0]->Access().role == Role::KeyChip);
        CHECK(kids[0]->Access().name.find("F") != std::string::npos);
    }
    int switches = 0;
    for (Widget* w : r.Everything()) {
        const AccessInfo a = w->Access();
        if (a.role == Role::Switch) {
            ++switches;
            CHECK(a.toggle != Toggle::None);
        }
    }
    CHECK(switches == 6);
    // The Start minimised switch is off while Run at startup is off, and still offered (it explains itself).
    Widget* minimised = r.Find(T(strings::id::kWindowSettingsStartMinimised), Role::Switch);
    REQUIRE(minimised);
    CHECK_FALSE(minimised->Access().enabled);
    CHECK(minimised->Access().focusable);
}

// K9: a switch has a card like a Key. The row stays a Toggle (no selection pattern, so a screen reader reads it
// as a switch), and the card, which is the same group, takes the switch's title as its name.
TEST_CASE("access: selecting a switch changes the card's name, and the switch keeps its Toggle") {
    Rig r;
    r.Show(Page::Settings);
    Widget* card = r.Find(T(strings::id::kWindowKeysRead), Role::Group);
    REQUIRE(card);
    CHECK(card->Access().name == T(strings::id::kWindowKeysRead));
    Widget* dark = r.Find(T(strings::id::kWindowSettingsDarkMode), Role::Switch);
    REQUIRE(dark);
    CHECK_FALSE(dark->Access().selectable);
    CHECK(dark->Access().toggle != Toggle::None);
    dark->Focus();  // the keyboard reaches the row: its card shows
    CHECK(card->Access().name == T("window.switches.darkModeTitle"));
    CHECK(r.Find(T("window.switches.darkModeTitle"), Role::Group) == card);
}

TEST_CASE("access: Report, Help and About offer their texts and links") {
    Rig r;
    for (Page p : {Page::Report, Page::Help, Page::About}) {
        CAPTURE(PageName(p));
        r.Show(p);
        int texts = 0, links = 0;
        for (Widget* w : r.Everything()) {
            const AccessInfo a = w->Access();
            if (a.role == Role::Text) {
                ++texts;
                CHECK_FALSE(AccessName(a).empty());
            }
            if (a.role == Role::Link) ++links;
        }
        CHECK(texts > 0);
        CHECK(links >= 6);  // the six links of the frame, and the page's own
    }
}

// ----------------------------------------------------------------------------- layers ---

TEST_CASE("access: a modal sheet hides the page beneath it, and the page comes back when it closes") {
    Rig r;
    r.Show(Page::Voices);
    Widget* speed = r.Find(T(strings::id::kWindowVoiceSpeed), Role::Slider);
    REQUIRE(speed);
    const int speedId = speed->Id();
    const size_t before = AccessChildren(r.S(), nullptr).size();
    REQUIRE(before > 1);

    static_cast<VoicesPage*>(r.settings.Current())->OpenDeleteSheet();
    REQUIRE(r.S().SheetCount() == 1);
    const std::vector<Widget*> top = AccessChildren(r.S(), nullptr);
    REQUIRE(top.size() == 1);
    const AccessInfo sheet = top[0]->Access();
    CHECK(sheet.role == Role::Dialog);
    CHECK_FALSE(sheet.name.empty());
    CHECK(FindAccessible(r.S(), speedId) == nullptr);   // the page is not offered
    CHECK(CheckAccess(r.S(), speedId, {AccessAction::SetValue, 1.0}) == AccessResult::Gone);
    // The sheet's own controls are offered, and the focus is in them.
    CHECK(AccessChildren(r.S(), top[0]).size() >= 2);
    const Widget* focus = AccessFocus(r.S());
    REQUIRE(focus);
    CHECK(AccessParent(r.S(), *focus) == top[0]);

    r.S().CloseTopSheet();
    r.ui.Frame();
    CHECK(r.S().SheetCount() == 0);
    CHECK(FindAccessible(r.S(), speedId) == speed);
    CHECK(AccessChildren(r.S(), nullptr).size() == before);
}

TEST_CASE("access: an open Select offers its list, and the highlighted row has the focus") {
    Rig r;
    r.Show(Page::Voices);
    Widget* voice = r.Find(T(strings::id::kWindowVoiceVoice), Role::ComboBox);
    REQUIRE(voice);
    voice->Focus();
    CHECK(AccessFocus(r.S()) == voice);

    REQUIRE(PerformAccess(r.S(), voice->Id(), {AccessAction::Expand}) == AccessResult::Done);
    r.ui.Frame();
    CHECK(voice->Access().expanded == Tri::Yes);
    // The page stays offered under a popup (it is not modal) and the list is a new top-level element.
    Widget* list = r.S().Popup();
    REQUIRE(list);
    CHECK(FindAccessible(r.S(), list->Id()) == list);
    CHECK(FindAccessible(r.S(), voice->Id()) == voice);
    const std::vector<Widget*> rows = AccessChildren(r.S(), list);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0]->Access().name == kLjspeech);
    CHECK(rows[1]->Access().selected);  // alba is the shown choice
    CHECK(AccessFocus(r.S()) == rows[1]);
    CHECK(AccessParent(r.S(), *rows[0]) == list);

    r.ui.Key(Key::Up);
    CHECK(AccessFocus(r.S()) == rows[0]);  // the arrow keys move the screen reader's focus with the highlight

    // Select on a row chooses it, as a click does, and closes the list.
    REQUIRE(PerformAccess(r.S(), rows[0]->Id(), {AccessAction::Select}) == AccessResult::Done);
    r.ui.Frame();
    REQUIRE_FALSE(r.sent.empty());
    CHECK(r.sent.back().type == PageMessageType::SetVoice);
    CHECK(r.sent.back().voice == kLjspeech);
    CHECK(r.S().Popup() == nullptr);
    CHECK(AccessFocus(r.S()) == voice);
}

TEST_CASE("access: a widget that is hidden or removed is gone") {
    Rig r;
    r.Show(Page::Home);
    Widget* power = r.Find(T(strings::id::kWindowHomePower));
    REQUIRE(power);
    const int id = power->Id();
    CHECK(FindAccessible(r.S(), id) == power);
    power->SetVisible(false);
    CHECK(FindAccessible(r.S(), id) == nullptr);
    power->SetVisible(true);
    CHECK(FindAccessible(r.S(), id) == power);
    r.Show(Page::Settings);  // the Home page is destroyed
    CHECK(FindAccessible(r.S(), id) == nullptr);
    CHECK(CheckAccess(r.S(), id, {AccessAction::Invoke}) == AccessResult::Gone);
    CHECK(PerformAccess(r.S(), id, {AccessAction::Invoke}) == AccessResult::Gone);
}

// ---------------------------------------------------------------------------- actions ---

TEST_CASE("access: Invoke and Toggle do what a click does") {
    Rig r;
    r.Show(Page::Home);
    Widget* power = r.Find(T(strings::id::kWindowHomePower));
    REQUIRE(power);
    REQUIRE(PerformAccess(r.S(), power->Id(), {AccessAction::Toggle}) == AccessResult::Done);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent.back().type == PageMessageType::SetOn);
    CHECK(r.sent.back().on);                       // it was Off
    CHECK(power->Access().toggle == Toggle::Off);  // and it still shows the last state

    r.Show(Page::Settings);
    Widget* startup = r.Find(T(strings::id::kWindowSettingsRunAtStartup), Role::Switch);
    REQUIRE(startup);
    CHECK(CheckAccess(r.S(), startup->Id(), {AccessAction::Invoke}) == AccessResult::NotSupported);
    REQUIRE(PerformAccess(r.S(), startup->Id(), {AccessAction::Toggle}) == AccessResult::Done);
    CHECK(r.sent.back().type == PageMessageType::SetRunAtStartup);
    CHECK(r.sent.back().on);

    // A disabled switch refuses.
    Widget* minimised = r.Find(T(strings::id::kWindowSettingsStartMinimised), Role::Switch);
    REQUIRE(minimised);
    const size_t n = r.sent.size();
    CHECK(PerformAccess(r.S(), minimised->Id(), {AccessAction::Toggle}) == AccessResult::NotEnabled);
    CHECK(r.sent.size() == n);

    // Invoke on a plain button, and a key chip starts the capture.
    Widget* chip = nullptr;
    for (Widget* w : r.Everything()) {
        if (w->Access().role == Role::KeyChip) { chip = w; break; }
    }
    REQUIRE(chip);
    REQUIRE(PerformAccess(r.S(), chip->Id(), {AccessAction::Invoke}) == AccessResult::Done);
    r.ui.Frame();
    CHECK(r.sent.back().type == PageMessageType::CaptureKey);
}

TEST_CASE("access: SetValue goes the way a drag does, and refuses a value outside the range") {
    Rig r;
    r.Show(Page::Voices);
    Widget* speed = r.Find(T(strings::id::kWindowVoiceSpeed), Role::Slider);
    REQUIRE(speed);
    REQUIRE(PerformAccess(r.S(), speed->Id(), {AccessAction::SetValue, 1.5}) == AccessResult::Done);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent.back().type == PageMessageType::SetControl);
    CHECK(r.sent.back().name == "speed");
    CHECK(r.sent.back().value == doctest::Approx(1.5));
    CHECK(speed->Access().current == doctest::Approx(1.0));  // the last state's value stays shown

    CHECK(CheckAccess(r.S(), speed->Id(), {AccessAction::SetValue, 5.0}) == AccessResult::BadValue);
    CHECK(CheckAccess(r.S(), speed->Id(), {AccessAction::SetValue, -1.0}) == AccessResult::BadValue);
    CHECK(CheckAccess(r.S(), speed->Id(), {AccessAction::Invoke}) == AccessResult::NotSupported);

    Widget* vol = r.Find(T(strings::id::kWindowVolume), Role::Slider);
    REQUIRE(vol);
    r.sent.clear();
    REQUIRE(PerformAccess(r.S(), vol->Id(), {AccessAction::SetValue, 80}) == AccessResult::Done);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent.back().type == PageMessageType::SetMasterVolume);
    CHECK(r.sent.back().value == doctest::Approx(80));
}

TEST_CASE("access: a callback that removes the page is safe inside an action") {
    Rig r;
    r.Show(Page::Home);
    // The nav link switches the page (deferred) from inside its own callback.
    Widget* settings = r.Find(T(strings::id::kWindowNavSettings), Role::Link);
    REQUIRE(settings);
    REQUIRE(PerformAccess(r.S(), settings->Id(), {AccessAction::Select}) == AccessResult::Done);
    CHECK(r.settings.CurrentPage() == Page::Settings);
    r.ui.Frame();
    Widget* now = r.Find(T(strings::id::kWindowNavSettings), Role::Link);
    REQUIRE(now);
    CHECK(now->Access().selected);
}

TEST_CASE("access: Close closes a sheet, SetFocus focuses, ScrollIntoView needs a scroll area") {
    Rig r;
    r.Show(Page::Voices);
    Widget* download = r.Find(T(strings::id::kWindowVoiceDownload), Role::Button);
    REQUIRE(download);
    CHECK(CheckAccess(r.S(), download->Id(), {AccessAction::ScrollIntoView}) == AccessResult::NotSupported);
    REQUIRE(PerformAccess(r.S(), download->Id(), {AccessAction::SetFocus}) == AccessResult::Done);
    CHECK(AccessFocus(r.S()) == download);

    REQUIRE(PerformAccess(r.S(), download->Id(), {AccessAction::Invoke}) == AccessResult::Done);
    r.ui.Frame();
    REQUIRE(r.S().SheetCount() == 1);
    Widget* sheet = AccessChildren(r.S(), nullptr).front();
    CHECK(CheckAccess(r.S(), sheet->Id(), {AccessAction::Close}) == AccessResult::Done);
    CHECK(CheckAccess(r.S(), download->Id(), {AccessAction::Close}) == AccessResult::Gone);  // hidden by the sheet
    REQUIRE(PerformAccess(r.S(), sheet->Id(), {AccessAction::Close}) == AccessResult::Done);
    r.ui.Frame();
    CHECK(r.S().SheetCount() == 0);
}

// ------------------------------------------------------------------------------ tracker ---

TEST_CASE("access tracker: the first Update only looks; focus, properties and structure are reported after") {
    Rig r;
    r.Show(Page::Settings);
    Log log;
    AccessTracker tracker(r.S());
    tracker.SetObserver(&log);
    tracker.Update();  // takes the snapshot silently
    CHECK(log.focus.empty());
    CHECK(log.props.empty());
    CHECK(log.structure.empty());

    // Focus.
    Widget* startup = r.Find(T(strings::id::kWindowSettingsRunAtStartup), Role::Switch);
    REQUIRE(startup);
    startup->Focus();
    tracker.Update();
    REQUIRE(log.focus.size() == 1);
    CHECK(log.focus[0] == startup->Id());
    tracker.Update();
    CHECK(log.focus.size() == 1);  // nothing new, nothing said

    // A property: the next state turns the switch on and enables Start minimised.
    log.Clear();
    UiState s = SampleState();
    s.runAtStartup = true;
    r.settings.SetState(s);
    tracker.Update();
    bool toggled = false, enabled = false;
    for (const Log::Prop& p : log.props) {
        if (p.id == startup->Id()) {
            toggled = (p.changes & kChangeToggle) && p.before.toggle == Toggle::Off && p.after.toggle == Toggle::On;
        }
        Widget* minimised = r.Find(T(strings::id::kWindowSettingsStartMinimised), Role::Switch);
        if (minimised && p.id == minimised->Id()) enabled = (p.changes & kChangeEnabled) && p.after.enabled;
    }
    CHECK(toggled);
    CHECK(enabled);

    // Structure: a page switch changes the window's children.
    log.Clear();
    r.Show(Page::Home);
    tracker.Update();
    CHECK(log.HasStructure(0));

    // A sheet opening: the page goes, the dialog comes.
    log.Clear();
    r.Show(Page::Voices);
    tracker.Update();
    log.Clear();
    static_cast<VoicesPage*>(r.settings.Current())->OpenDeleteSheet();
    tracker.Update();
    CHECK(log.HasStructure(0));
    REQUIRE_FALSE(log.focus.empty());  // the focus moved into the sheet
    CHECK(log.focus.back() != 0);
}

TEST_CASE("access tracker: a value change of a slider is reported with the old and the new value") {
    Rig r;
    r.Show(Page::Voices);
    Widget* speed = r.Find(T(strings::id::kWindowVoiceSpeed), Role::Slider);
    REQUIRE(speed);
    Log log;
    AccessTracker tracker(r.S());
    tracker.SetObserver(&log);
    tracker.Update();
    UiState s = SampleState();
    s.controls["speed"].value = 1.25;
    r.settings.SetState(s);
    tracker.Update();
    bool seen = false;
    for (const Log::Prop& p : log.props) {
        if (p.id == speed->Id() && (p.changes & kChangeRange)) {
            seen = p.before.current == doctest::Approx(1.0) && p.after.current == doctest::Approx(1.25);
        }
    }
    CHECK(seen);
}

TEST_CASE("access tracker: the key capture hint is a live region and is announced when it gets its words") {
    Rig r;
    r.Show(Page::Settings);
    Log log;
    AccessTracker tracker(r.S());
    tracker.SetObserver(&log);
    tracker.Update();
    // Start a capture on the first chip: the hint under the card now has words.
    Widget* chip = nullptr;
    for (Widget* w : r.Everything()) {
        if (w->Access().role == Role::KeyChip) { chip = w; break; }
    }
    REQUIRE(chip);
    PerformAccess(r.S(), chip->Id(), {AccessAction::Invoke});
    r.ui.Frame();
    tracker.Update();
    // The hint is a Text that was empty (not offered) and now is: it appears, as a live region with words.
    REQUIRE_FALSE(log.live.empty());
    Widget* hint = FindAccessible(r.S(), log.live.back());
    REQUIRE(hint);
    CHECK(hint->Access().liveRegion);
    CHECK(AccessName(hint->Access()) == T(strings::id::kWindowKeysCaptureHint));
}

TEST_CASE("access tracker: Forget starts again without reporting what happened meanwhile") {
    Rig r;
    r.Show(Page::Home);
    Log log;
    AccessTracker tracker(r.S());
    tracker.SetObserver(&log);
    tracker.Update();
    tracker.Forget();
    r.Show(Page::Settings);
    tracker.Update();  // silent
    CHECK(log.structure.empty());
    r.Show(Page::Home);
    tracker.Update();
    CHECK(log.HasStructure(0));
}

TEST_CASE("access: bounds are clipped to the scroll areas and the window, and a clipped-out row is offscreen") {
    Rig r;
    r.Show(Page::Voices);
    Widget* speed = r.Find(T(strings::id::kWindowVoiceSpeed), Role::Slider);
    REQUIRE(speed);
    const AccessBounds b = BoundsOf(r.S(), *speed);
    CHECK_FALSE(b.offscreen);
    CHECK(b.rect.w > 0);
    CHECK(b.rect.x >= 0);
    CHECK(b.rect.Right() <= 800);
}

// K11: the read button's name follows the host's reading state, and the tracker reports it (UIA Name event).
TEST_CASE("access: the read button's name changes when its card is read, and the tracker says so") {
    Rig r;
    r.Show(Page::Settings);
    Log log;
    AccessTracker tracker(r.S());
    tracker.SetObserver(&log);
    tracker.Update();  // the first call only looks
    const std::string card = T(strings::id::kWindowKeysRead);
    const std::string idle = strings::Format("en", strings::id::kWindowKeysReadAloud, {{"action", card}});
    const std::string stop = strings::Format("en", strings::id::kWindowKeysStopReading, {{"action", card}});
    Widget* button = r.Find(idle, Role::Button);
    REQUIRE(button);

    UiState s = SampleState();
    s.readingCard = "read";
    r.settings.SetState(s);
    tracker.Update();
    bool renamed = false;
    for (const Log::Prop& p : log.props) {
        if (p.id == button->Id()) renamed = (p.changes & kChangeName) && p.before.name == idle && p.after.name == stop;
    }
    CHECK(renamed);
    CHECK(r.Find(stop, Role::Button) == button);
    CHECK(r.Find(idle, Role::Button) == nullptr);

    // It stays a button that can be invoked: the stop press is an ordinary Invoke.
    r.sent.clear();
    CHECK(PerformAccess(r.S(), button->Id(), AccessRequest{AccessAction::Invoke}) == AccessResult::Done);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::ReadKeyCard);
}

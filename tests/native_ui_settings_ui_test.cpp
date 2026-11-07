// SettingsUi (K.2): the frame, the Home, Help, Report and About pages of the native Settings window. Driven
// through the Screen with a FakeCanvas, like the toolkit tests: what a click or a key sends to the host
// (PageMessage), what the pages draw from a state, and that every word on a page comes from strings/.
#include "native_ui/pages/about_page.h"
#include "native_ui/pages/home_page.h"
#include "native_ui/pages/layout.h"
#include "native_ui/pages/report_page.h"
#include "native_ui/settings_ui.h"
#include "native_ui_test_util.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

#include <doctest/doctest.h>

#include <cctype>
#include <set>
#include <string>
#include <vector>

using namespace nui;
using namespace nui::test;

namespace {

// A SettingsUi in a Screen, with a sink that keeps the messages it is sent.
struct Rig {
    Ui ui;
    SettingsUi settings{ui.screen, "en"};
    std::vector<PageMessage> sent;

    Rig() {
        ui.screen.SetMessageSink([this](const PageMessage& m) { sent.push_back(m); });
        ui.Frame();  // the first draw lays everything out
    }
    UiState State(bool on) const {
        UiState s;
        s.on = on;
        s.version = "0.5.0";
        s.masterVolume = {50, 0, 100, 100, 5};
        return s;
    }
    // Sends a state and settles the transitions, so the canvas shows the end look.
    void Push(const UiState& s) {
        settings.SetState(s);
        ui.Advance(0.5);
    }
    void ClickOn(const Widget& w) {
        const PointF c = Centre(w);
        ui.Click(c.x, c.y);
    }
    // The widgets of the tree, counted (a state must not add widgets).
    static size_t Count(const Widget& w) {
        size_t n = 1;
        for (const auto& c : w.Children()) n += Count(*c);
        return n;
    }
    size_t Widgets() { return Count(ui.screen.Root()); }
};

}  // namespace

// -------------------------------------------------------------------------------- frame ---

TEST_CASE("the frame's links switch the page, locally, and the current link is bold") {
    Rig r;
    CHECK(r.settings.CurrentPage() == Page::Home);
    CHECK(r.settings.GetFrame().LinkFor(Page::Home)->Current());

    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::Voices));
    CHECK(r.settings.CurrentPage() == Page::Voices);
    CHECK(r.settings.Current()->Kind() == Page::Voices);
    CHECK(r.settings.GetFrame().LinkFor(Page::Voices)->Current());
    CHECK_FALSE(r.settings.GetFrame().LinkFor(Page::Home)->Current());

    r.ui.Frame();
    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::About));  // a bottom link
    CHECK(r.settings.CurrentPage() == Page::About);
    r.ui.Frame();
    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::Settings));
    CHECK(r.settings.CurrentPage() == Page::Settings);
    r.ui.Frame();
    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::Report));
    CHECK(r.settings.CurrentPage() == Page::Report);
    r.ui.Frame();
    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::Help));
    CHECK(r.settings.CurrentPage() == Page::Help);
    r.ui.Frame();
    r.ClickOn(*r.settings.GetFrame().LinkFor(Page::Home));
    CHECK(r.settings.CurrentPage() == Page::Home);

    CHECK(r.sent.empty());  // which page shows is the UI's own: nothing goes to the host
}

TEST_CASE("ShowPage builds the page of the name, and the same page again changes nothing") {
    Rig r;
    Page p = Page::Home;
    CHECK(ParsePageName("report", p));
    CHECK(p == Page::Report);
    CHECK_FALSE(ParsePageName("nope", p));
    CHECK(p == Page::Report);  // untouched by a bad name
    r.settings.ShowPage(p);
    CHECK(r.settings.Current()->Kind() == Page::Report);
    PageView* before = r.settings.Current();
    r.settings.ShowPage(Page::Report);
    CHECK(r.settings.Current() == before);
    for (Page page : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
        r.settings.ShowPage(page);
        CHECK(r.settings.Current()->Kind() == page);
        CHECK(std::string(PageName(page)).size() > 3);
    }
}

TEST_CASE("the links sit in tab order before the page: top links, minimise, close, bottom links, volume, then the page") {
    Rig r;
    r.Push(r.State(false));
    const std::vector<Widget*> order = r.ui.screen.FocusOrder();
    REQUIRE(order.size() >= 10);
    CHECK(order[0] == r.settings.GetFrame().LinkFor(Page::Home));
    CHECK(order[1] == r.settings.GetFrame().LinkFor(Page::Voices));
    CHECK(order[2] == r.settings.GetFrame().LinkFor(Page::Settings));
    CHECK(order[3] == r.settings.GetFrame().MinimiseButton());  // K10: right after the top links
    CHECK(order[4] == r.settings.GetFrame().CloseButton());
    CHECK(order[5] == r.settings.GetFrame().LinkFor(Page::Report));
    CHECK(order[8] == r.settings.GetFrame().Volume());
    CHECK(order[9] == static_cast<HomePage*>(r.settings.Current())->Power());
}

TEST_CASE("K10: minimise and close are in the frame of every page, where the mockup puts them") {
    Rig r;
    for (Page page : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
        CAPTURE(PageName(page));
        r.settings.ShowPage(page);
        r.Push(r.State(false));
        Frame& frame = r.settings.GetFrame();
        REQUIRE(frame.MinimiseButton());
        REQUIRE(frame.CloseButton());
        // The icon box (24 x 24) is the hit area shrunk by 2; the boxes are at (726, 10) and (760, 10).
        const RectF m = frame.MinimiseButton()->Abs(), c = frame.CloseButton()->Abs();
        CHECK(m.x + 2 == doctest::Approx(layout::kMinimiseX));
        CHECK(m.y + 2 == doctest::Approx(layout::kWindowButtonsY));
        CHECK(c.x + 2 == doctest::Approx(layout::kCloseX));
        CHECK(c.y + 2 == doctest::Approx(layout::kWindowButtonsY));
        CHECK(m.w == doctest::Approx(28));
        CHECK(m.Right() < c.x);  // the two hit areas do not overlap
        CHECK(c.Right() <= layout::kWindowW);
        CHECK(frame.MinimiseButton()->Access().name == strings::Text("en", strings::id::kWindowMinimise));
        CHECK(frame.CloseButton()->Access().name == strings::Text("en", strings::id::kWindowClose));
        CHECK(frame.MinimiseButton()->Tooltip() == strings::Text("en", strings::id::kWindowMinimise));
        CHECK(frame.CloseButton()->Tooltip() == strings::Text("en", strings::id::kWindowClose));
    }
}

TEST_CASE("K10: a click on minimise or close asks the window, and sends nothing to the host") {
    Rig r;
    r.Push(r.State(false));
    std::vector<WindowCommand> asked;
    r.settings.SetOnWindowCommand([&](WindowCommand c) { asked.push_back(c); });
    r.ClickOn(*r.settings.GetFrame().MinimiseButton());
    REQUIRE(asked.size() == 1);
    CHECK(asked[0] == WindowCommand::Minimise);
    r.ClickOn(*r.settings.GetFrame().CloseButton());
    REQUIRE(asked.size() == 2);
    CHECK(asked[1] == WindowCommand::Close);
    // The keyboard does the same: Tab to the button, Enter.
    r.settings.GetFrame().CloseButton()->Focus();
    r.ui.Key(Key::Enter);
    CHECK(asked.size() == 3);
    CHECK(r.sent.empty());  // a PageMessage is for the App; these are the window's own
    // Without a handler (the picture tool, a test) a click does nothing and does not fail.
    r.settings.SetOnWindowCommand(nullptr);
    r.ClickOn(*r.settings.GetFrame().CloseButton());
    CHECK(r.sent.empty());
}

TEST_CASE("K10: the window buttons are quiet when idle and take the accent colour on hover") {
    Rig r;
    r.Push(r.State(false));
    auto iconColour = [&](const char* icon) {
        r.ui.canvas.Clear();
        r.ui.Frame();
        for (const Call* c : r.ui.canvas.Of(Call::Icon)) {
            if (c->text == icon) return c->colour.rgb;
        }
        return 0xFFFFFFFFu;
    };
    CHECK(iconColour("dash") == r.ui.pal.windowIcon.rgb);
    CHECK(iconColour("x-02") == r.ui.pal.windowIcon.rgb);
    const PointF c = Centre(*r.settings.GetFrame().CloseButton());
    r.ui.Move(c.x, c.y);
    r.ui.Advance(0.5);
    CHECK(iconColour("x-02") == r.ui.pal.accent.rgb);
    CHECK(iconColour("dash") == r.ui.pal.windowIcon.rgb);  // the other one stays quiet
}

TEST_CASE("Master volume: a press asks for the value under the pointer, on the grid of 5") {
    Rig r;
    // Before the first state the slider has no range: disabled, no message.
    CHECK_FALSE(r.settings.GetFrame().Volume()->HasRange());
    r.ClickOn(*r.settings.GetFrame().Volume());
    CHECK(r.sent.empty());

    r.Push(r.State(true));
    CHECK(r.settings.GetFrame().Volume()->HasRange());
    const RectF box = r.settings.GetFrame().Volume()->Abs();
    CHECK(box.x == doctest::Approx(576));
    CHECK(box.w == doctest::Approx(100));
    r.ui.Click(box.x + 33.0f, box.y + 12);  // 33 % -> 35
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::SetMasterVolume);
    CHECK(r.sent[0].value == doctest::Approx(35));

    // The slider shows the state, not the click: still at 50 until the next state says otherwise.
    CHECK(r.settings.GetFrame().Volume()->Range().value == doctest::Approx(50));
    UiState s = r.State(true);
    s.masterVolume.value = 35;
    r.Push(s);
    CHECK(r.settings.GetFrame().Volume()->Range().value == doctest::Approx(35));

    // Keys: the arrow moves one step of 5.
    r.sent.clear();
    r.settings.GetFrame().Volume()->Focus();
    r.ui.Key(Key::Right);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].value == doctest::Approx(40));
    // The tooltip and the value text say the percentage.
    CHECK(r.settings.GetFrame().Volume()->Tooltip() == "35%");
}

// ---------------------------------------------------------------------------------- Home ---

TEST_CASE("Home: both buttons are disabled until the first state") {
    Rig r;
    auto* home = static_cast<HomePage*>(r.settings.Current());
    CHECK_FALSE(home->Power()->Enabled());
    CHECK_FALSE(home->Enable()->Enabled());
    r.ClickOn(*home->Power());
    r.ClickOn(*home->Enable());
    CHECK(r.sent.empty());
    r.ui.Frame();
    CHECK(r.ui.canvas.Count(Call::Picture) >= 2);  // the bee and the flute: the page is drawn

    r.Push(r.State(false));
    CHECK(home->Power()->Enabled());
    CHECK(home->Enable()->Enabled());
}

TEST_CASE("Home: the power circle and the Enable button send setOn with the opposite of the state") {
    Rig r;
    auto* home = static_cast<HomePage*>(r.settings.Current());

    r.Push(r.State(false));  // Off
    CHECK(home->Enable()->Label() == strings::Text("en", strings::id::kWindowHomeEnable));
    r.ClickOn(*home->Power());
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].type == PageMessageType::SetOn);
    CHECK(r.sent[0].on == true);
    r.ClickOn(*home->Enable());
    REQUIRE(r.sent.size() == 2);
    CHECK(r.sent[1].on == true);
    CHECK(home->Power()->On() == false);  // nothing flips by itself: only a state does

    r.Push(r.State(true));  // On
    CHECK(home->Power()->On());
    CHECK(home->Enable()->Label() == strings::Text("en", strings::id::kWindowHomeDisable));
    r.sent.clear();
    r.ClickOn(*home->Power());
    r.ClickOn(*home->Enable());
    REQUIRE(r.sent.size() == 2);
    CHECK(r.sent[0].on == false);
    CHECK(r.sent[1].on == false);

    // The keyboard does the same: Space on the focused power button.
    r.sent.clear();
    home->Power()->Focus();
    r.ui.Key(Key::Space);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.sent[0].on == false);
}

TEST_CASE("Home: the bee's tint and the circle follow the state") {
    Rig r;
    const Palette& p = r.ui.pal;
    r.Push(r.State(false));
    r.ui.canvas.Clear();
    r.ui.Frame();
    bool beeOff = false;
    for (const Call* c : r.ui.canvas.Of(Call::Picture)) beeOff |= c->text == "bee" && c->colour.rgb == p.tintBeeOff.rgb;
    CHECK(beeOff);
    // The circle: paper when Off.
    const Call* circle = r.ui.canvas.Of(Call::FillEllipse).empty() ? nullptr : r.ui.canvas.Of(Call::FillEllipse)[0];
    REQUIRE(circle);
    CHECK(circle->colour.rgb == p.paper.rgb);
    CHECK(circle->rect.w == 124);

    r.Push(r.State(true));
    bool beeOn = false;
    for (const Call* c : r.ui.canvas.Of(Call::Picture)) beeOn |= c->text == "bee" && c->colour.rgb == p.tintBeeOn.rgb;
    CHECK(beeOn);
    circle = r.ui.canvas.Of(Call::FillEllipse)[0];
    CHECK(circle->colour.rgb == p.accent.rgb);
    // The glyph: an arc and the stem.
    CHECK(r.ui.canvas.Count(Call::Arc) == 1);
}

TEST_CASE("a state again does not add widgets, and the page keeps showing the last state") {
    Rig r;
    r.Push(r.State(false));
    const size_t n = r.Widgets();
    for (int i = 0; i < 3; ++i) r.Push(r.State(i % 2 == 0));
    CHECK(r.Widgets() == n);
    for (Page page : {Page::Report, Page::About, Page::Help, Page::Voices, Page::Settings}) {
        r.settings.ShowPage(page);
        const size_t m = r.Widgets();
        r.Push(r.State(true));
        r.Push(r.State(false));
        CHECK(r.Widgets() == m);
    }
    // A page that is built later gets the state that has come already.
    r.settings.ShowPage(Page::Home);
    CHECK(static_cast<HomePage*>(r.settings.Current())->Power()->Enabled());
}

// ---------------------------------------------------------------------------- Report, About ---

TEST_CASE("Report: the links send openLink with their LinkId") {
    Rig r;
    r.settings.ShowPage(Page::Report);
    r.Push(r.State(false));
    auto* report = static_cast<ReportPage*>(r.settings.Current());
    r.ClickOn(*report->MailLink());
    r.ClickOn(*report->DiscordLink());
    r.ClickOn(*report->GithubLink());
    REQUIRE(r.sent.size() == 3);
    for (const PageMessage& m : r.sent) CHECK(m.type == PageMessageType::OpenLink);
    CHECK(r.sent[0].link == LinkId::Mail);
    CHECK(r.sent[1].link == LinkId::Discord);
    CHECK(r.sent[2].link == LinkId::Github);
}

TEST_CASE("About: itch and GitHub send openLink, the notices link sends openNotices") {
    Rig r;
    r.settings.ShowPage(Page::About);
    r.Push(r.State(false));
    auto* about = static_cast<AboutPage*>(r.settings.Current());
    r.ClickOn(*about->ItchButton());
    r.ClickOn(*about->GithubButton());
    r.ClickOn(*about->NoticesLink());
    REQUIRE(r.sent.size() == 3);
    CHECK(r.sent[0].type == PageMessageType::OpenLink);
    CHECK(r.sent[0].link == LinkId::Itch);
    CHECK(r.sent[1].type == PageMessageType::OpenLink);
    CHECK(r.sent[1].link == LinkId::Github);
    CHECK(r.sent[2].type == PageMessageType::OpenNotices);
    // The inline link's accessible name is the long one, its drawn word the short one.
    CHECK(about->NoticesLink()->Access().name == strings::Text("en", strings::id::kWindowAboutNoticesLabel));
    CHECK(about->ItchButton()->Tooltip() == strings::Text("en", strings::id::kWindowAboutItch));
}

TEST_CASE("About: the head shows the name and the version of the state") {
    Rig r;
    r.settings.ShowPage(Page::About);
    UiState s = r.State(false);
    s.version = "1.2.3";
    r.Push(s);
    r.ui.canvas.Clear();
    r.ui.Frame();
    CHECK(r.ui.canvas.FindText("Lexia"));
    CHECK(r.ui.canvas.FindText("| ver. 1.2.3"));
}

TEST_CASE("About: the text that goes round the wolf starts its first lines in by the indents") {
    Rig r;
    r.settings.ShowPage(Page::About);
    r.Push(r.State(false));
    // The paragraph's lines at y >= 275 are the wolf-wrapped text; its first line is indented by 27.
    float firstX = -1;
    for (const Call* c : r.ui.canvas.Of(Call::Text)) {
        if (c->rect.y >= 274 && c->rect.y < 280 && firstX < 0) firstX = c->rect.x;
    }
    CHECK(firstX == doctest::Approx(283 + 27));
}

// ------------------------------------------------------------------------------- strings ---

namespace {

// Letters and digits only (and any byte of a UTF-8 letter), no no-break spaces: so a piece of a wrapped line,
// a word, or a bold phrase compares with the text it came from.
std::string Core(std::string s) {
    for (size_t pos = s.find("\xC2\xA0"); pos != std::string::npos; pos = s.find("\xC2\xA0", pos)) s.erase(pos, 2);
    std::string out;
    for (unsigned char ch : s) {
        if (std::isalnum(ch) || ch >= 0x80) out.push_back(static_cast<char>(ch));
    }
    return out;
}

}  // namespace

TEST_CASE("every word a page draws comes from strings/ (or is the version or the percentage)") {
    // The corpus: every English text, and the two filled-in forms the code builds with Format.
    std::vector<std::string> corpus;
    for (const char* id : strings::AllIds()) corpus.push_back(Core(strings::Text("en", id)));
    corpus.push_back(Core(strings::Format("en", strings::id::kWindowAboutVersion, {{"version", "0.5.0"}})));
    corpus.push_back(Core(strings::Format("en", strings::id::kWindowVolumeValue, {{"value", "50"}})));

    Rig r;
    for (bool on : {false, true}) {
        for (Page page : {Page::Home, Page::Voices, Page::Settings, Page::Report, Page::Help, Page::About}) {
            r.settings.ShowPage(page);
            r.Push(r.State(on));
            r.ui.canvas.Clear();
            r.ui.Frame();
            int texts = 0;
            for (const Call* c : r.ui.canvas.Of(Call::Text)) {
                const std::string core = Core(c->text);
                ++texts;
                bool known = core.empty();  // the "|" of the head
                for (const std::string& entry : corpus) known = known || entry.find(core) != std::string::npos;
                INFO("page " << PageName(page) << " draws \"" << c->text << "\"");
                CHECK(known);
            }
            CHECK(texts >= 6);  // at least the six links of the frame
        }
    }
}

TEST_CASE("the nav links, the volume and the buttons have accessible names from strings/") {
    Rig r;
    r.Push(r.State(false));
    CHECK(r.settings.GetFrame().LinkFor(Page::Home)->Access().name == strings::Text("en", strings::id::kWindowNavHome));
    CHECK(r.settings.GetFrame().Volume()->Access().name == strings::Text("en", strings::id::kWindowVolume));
    auto* home = static_cast<HomePage*>(r.settings.Current());
    CHECK(home->Power()->Access().name == strings::Text("en", strings::id::kWindowHomePower));
    CHECK(home->Power()->Access().toggle == Toggle::Off);
    CHECK(r.settings.GetFrame().LinkFor(Page::Home)->Access().selected);
    CHECK_FALSE(r.settings.GetFrame().LinkFor(Page::About)->Access().selected);
}

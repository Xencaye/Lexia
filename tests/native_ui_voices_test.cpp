// The Voices page and its sheets (K.3): what each control sends to the host, when Delete and the reset buttons
// are on, how a control's value reads, the Delete voice / Voice download / Result sheets (modal, Esc, focus in
// and back), the download list's states, one download at a time, the scroll position and the focus kept
// across updates, and the VoiceLibraryState that the typed Voice events change. Driven through the Screen with a
// FakeCanvas, like the other native_ui tests.
#include "native_ui/pages/voice_sheets.h"
#include "native_ui/pages/voice_text.h"
#include "native_ui/pages/voices_page.h"
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

constexpr const char* kLjspeech = "en_US-ljspeech-medium";
constexpr const char* kAlba = "en_GB-alba-medium";
constexpr uint64_t kMiB = 1024 * 1024;

std::string T(const char* id) { return strings::Text("en", id); }

// The first widget under `w` with this accessible name (and role, if given).
Widget* Find(Widget& w, const std::string& name, Role role = Role::None) {
    const AccessInfo a = w.Access();
    if (a.name == name && (role == Role::None || a.role == role)) return &w;
    for (const auto& c : w.Children()) {
        if (Widget* hit = Find(*c, name, role)) return hit;
    }
    return nullptr;
}

// Every widget under `w` of a role, in tree order.
void Collect(Widget& w, Role role, std::vector<Widget*>& out) {
    if (w.Access().role == role) out.push_back(&w);
    for (const auto& c : w.Children()) Collect(*c, role, out);
}

UiState VoiceState(const std::string& active = kLjspeech, double pitch = 0, bool installed = true) {
    UiState s;
    s.version = "0.6.0";
    s.masterVolume = {50, 0, 100, 100, 5};
    UiVoices v;
    if (installed) {
        v.installed = {kLjspeech, kAlba};
        v.active = active;
        v.removable = {kAlba};
        s.controls["speed"] = {1.0, 0.5, 2.0, 1.0, 0.05};
        s.controls["pitch"] = {pitch, -6, 6, 0, 0.5};
        s.controls["volume"] = {1.0, 0, 2.0, 1.0, 0.05};
    }
    s.voices["eng"] = v;
    return s;
}

std::vector<CatalogueEntry> Catalogue(int extra = 0) {
    std::vector<CatalogueEntry> list = {{"en_GB-alba-medium", "eng", 63 * kMiB, true},
                                        {"en_GB-jenny_dioco-medium", "eng", 63 * kMiB, false},
                                        {"en_US-amy-medium", "eng", 63 * kMiB, false},
                                        {"en_US-ljspeech-medium", "eng", 63 * kMiB, true}};
    for (int i = 0; i < extra; ++i) list.push_back({"en_US-extra" + std::to_string(i) + "-medium", "eng", 20 * kMiB, false});
    return list;
}

// A SettingsUi on the Voices page in a Screen, with a sink that keeps the messages the page sends.
struct Rig {
    Ui ui;
    SettingsUi settings{ui.screen, "en"};
    std::vector<PageMessage> sent;

    explicit Rig(const UiState& state = VoiceState()) {
        ui.screen.SetMessageSink([this](const PageMessage& m) { sent.push_back(m); });
        settings.ShowPage(Page::Voices);
        settings.SetState(state);
        ui.Advance(0.5);
    }
    VoicesPage& Page_() { return *static_cast<VoicesPage*>(settings.Current()); }
    void Push(const UiState& s) {
        settings.SetState(s);
        ui.Advance(0.5);
    }
    void Click(const Widget& w) {
        const PointF c = Centre(w);
        ui.Click(c.x, c.y);
        ui.Frame();
    }
    VoiceEventSink& Events() { return settings.VoiceEvents(); }
    Sheet* Top() { return ui.screen.TopSheet(); }
    const PageMessage& Last() const { return sent.back(); }
};

}  // namespace

// ------------------------------------------------------------------------------ the page ---

TEST_CASE("voices page: the Voice choice and the icon buttons send their messages") {
    Rig r;
    VoicesPage& p = r.Page_();
    CHECK(p.VoiceSelect()->Items() == std::vector<std::string>{kLjspeech, kAlba});
    CHECK(p.VoiceSelect()->Selected() == 0);
    CHECK(p.VoiceSelect()->Enabled());

    p.VoiceSelect()->Choose(1);  // the person picks the second Voice
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::SetVoice);
    CHECK(r.Last().language == "eng");
    CHECK(r.Last().voice == kAlba);
    CHECK(p.VoiceSelect()->Selected() == 0);  // the page keeps no copy: it moves with the next state

    r.Push(VoiceState(kAlba));
    CHECK(p.VoiceSelect()->Selected() == 1);

    r.Click(*p.AddFileButton());
    CHECK(r.Last().type == PageMessageType::AddFromFile);
    r.Click(*p.SavePresetLink());
    CHECK(r.Last().type == PageMessageType::SavePresetFile);
    r.Click(*p.LoadPresetLink());
    CHECK(r.Last().type == PageMessageType::LoadPresetFile);

    // Download: the host is asked for the catalogue and the sheet opens at once, loading.
    r.sent.clear();
    r.Click(*p.DownloadButton());
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::OpenCatalogue);
    CHECK(r.ui.screen.SheetCount() == 1);
}

TEST_CASE("voices page: a slider asks for a snapped value, a reset button for the default") {
    Rig r(VoiceState(kLjspeech, 3.0));
    VoicesPage& p = r.Page_();

    Slider* pitch = p.SliderFor("pitch");
    r.ui.Click(pitch->Abs().x + pitch->Abs().w * 0.25f, Centre(*pitch).y);  // a quarter along: -6 + 3 = -3
    REQUIRE(!r.sent.empty());
    CHECK(r.Last().type == PageMessageType::SetControl);
    CHECK(r.Last().name == "pitch");
    CHECK(r.Last().value == doctest::Approx(-3.0));
    r.ui.Up(Centre(*pitch).x, Centre(*pitch).y);

    r.sent.clear();
    r.ui.screen.SetFocus(p.SliderFor("speed"));
    r.ui.Key(Key::Right);  // one step of 0.05 above 1.0
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().name == "speed");
    CHECK(r.Last().value == doctest::Approx(1.05));

    r.sent.clear();
    REQUIRE(p.ResetFor("pitch")->Visible());
    r.Click(*p.ResetFor("pitch"));
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::ResetControl);
    CHECK(r.Last().name == "pitch");
}

TEST_CASE("voices page: a reset button shows only beside a value that is not the default") {
    Rig r;
    VoicesPage& p = r.Page_();
    for (const char* name : {"speed", "pitch", "volume"}) CHECK_FALSE(p.ResetFor(name)->Visible());

    r.Push(VoiceState(kLjspeech, 1.5));
    CHECK(p.ResetFor("pitch")->Visible());
    CHECK_FALSE(p.ResetFor("speed")->Visible());
    CHECK_FALSE(p.ResetFor("volume")->Visible());
    CHECK(p.ResetFor("pitch")->Access().name == "Reset pitch");

    r.Push(VoiceState(kLjspeech, 0));
    CHECK_FALSE(p.ResetFor("pitch")->Visible());
}

TEST_CASE("voices page: Delete is on only for a Voice the person added") {
    Rig r;  // ljspeech comes with Lexia
    VoicesPage& p = r.Page_();
    CHECK_FALSE(p.DeleteButton()->Enabled());
    CHECK(p.DeleteButton()->Tooltip() == T(strings::id::kWindowVoiceDeleteShipped));
    CHECK(p.DeleteButton()->FocusableNow());  // it explains itself: it keeps the focus
    r.Click(*p.DeleteButton());
    CHECK(r.ui.screen.SheetCount() == 0);
    CHECK(r.sent.empty());

    r.Push(VoiceState(kAlba));
    CHECK(p.DeleteButton()->Enabled());
    CHECK(p.DeleteButton()->Tooltip() == T(strings::id::kWindowVoiceDelete));
}

TEST_CASE("voices page: with no Voice the note shows, the controls are off and the presets cannot be used") {
    Rig r(VoiceState(kLjspeech, 0, /*installed=*/false));
    VoicesPage& p = r.Page_();
    CHECK_FALSE(p.VoiceSelect()->Enabled());
    CHECK(p.VoiceSelect()->Selected() == -1);
    CHECK(p.NoneNote()->Visible());
    CHECK(p.NoneNote()->PlainText() == "No voice is installed for English. Download one, or add one from a file.");
    for (const char* name : {"speed", "pitch", "volume"}) {
        CHECK_FALSE(p.SliderFor(name)->HasRange());
        CHECK_FALSE(p.ResetFor(name)->Visible());
    }
    CHECK_FALSE(p.SavePresetLink()->Enabled());
    CHECK_FALSE(p.LoadPresetLink()->Enabled());
    CHECK_FALSE(p.DeleteButton()->Enabled());

    r.Push(VoiceState());
    CHECK_FALSE(p.NoneNote()->Visible());
    CHECK(p.SliderFor("speed")->HasRange());
    CHECK(p.SavePresetLink()->Enabled());
}

TEST_CASE("voices page: a slider's value reads as the old page wrote it") {
    Rig r(VoiceState(kLjspeech, 1.5));
    VoicesPage& p = r.Page_();
    CHECK(p.SliderFor("speed")->Tooltip() == "1.0\xC3\x97");
    CHECK(p.SliderFor("pitch")->Tooltip() == "+1.5 semitones");
    CHECK(p.SliderFor("volume")->Tooltip() == "100%");
    CHECK(p.SliderFor("speed")->Range().log);  // Speed on a log scale: 1.0 in the middle
    CHECK_FALSE(p.SliderFor("pitch")->Range().log);

    const std::string en = "en";
    CHECK(VoiceControlText(en, "speed", 0.85) == "0.85\xC3\x97");
    CHECK(VoiceControlText(en, "speed", 1.5) == "1.5\xC3\x97");
    CHECK(VoiceControlText(en, "speed", 2) == "2.0\xC3\x97");
    CHECK(VoiceControlText(en, "pitch", 0) == "0 semitones");
    CHECK(VoiceControlText(en, "pitch", 1) == "+1 semitone");
    CHECK(VoiceControlText(en, "pitch", -1) == "\xE2\x88\x92" "1 semitone");  // U+2212, not a hyphen
    CHECK(VoiceControlText(en, "pitch", -2.5) == "\xE2\x88\x92" "2.5 semitones");
    CHECK(VoiceControlText(en, "volume", 1.2) == "120%");
    CHECK(VoiceControlText(en, "volume", 0) == "0%");
}

TEST_CASE("download size and progress read as the old sheet wrote them") {
    CHECK(DownloadSizeText("en", 63 * kMiB) == "63 MB");
    CHECK(DownloadSizeText("en", 5 * kMiB + kMiB / 2) == "5.5 MB");  // one decimal under 10 MB
    CHECK(DownloadProgressText("en", 12 * kMiB, 63 * kMiB) == "12 of 63 MB");
    CHECK(DownloadProgressText("en", 12 * kMiB + kMiB / 2, 63 * kMiB) == "12 of 63 MB");  // rounded down
    CHECK(DownloadProgressText("en", 70 * kMiB, 63 * kMiB) == "63 of 63 MB");              // never more than the total
    CHECK(DownloadProgressText("en", 2 * kMiB, 5 * kMiB) == "2.0 of 5.0 MB");
}

// ------------------------------------------------------------------------- Delete voice sheet ---

TEST_CASE("Delete voice: a modal sheet, the focus starts on Keep it, Delete sends deleteVoice") {
    Rig r(VoiceState(kAlba));
    VoicesPage& p = r.Page_();
    p.DeleteButton()->Focus();
    r.Click(*p.DeleteButton());
    REQUIRE(r.ui.screen.SheetCount() == 1);
    Sheet* s = r.Top();
    CHECK(s->Access().name == "Delete en_GB-alba-medium?");
    Widget* keep = Find(*s, "Keep it", Role::Button);
    Widget* del = Find(*s, "Delete", Role::Button);
    REQUIRE(keep);
    REQUIRE(del);
    CHECK(r.ui.screen.Focused() == keep);

    r.sent.clear();
    r.Click(*p.SavePresetLink());  // behind the scrim: nothing happens
    CHECK(r.sent.empty());

    r.Click(*del);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::DeleteVoice);
    CHECK(r.Last().voice == kAlba);
    CHECK(r.ui.screen.SheetCount() == 0);
}

TEST_CASE("Delete voice: Esc and Keep it close it and send nothing; the focus goes back to the button") {
    Rig r(VoiceState(kAlba));
    VoicesPage& p = r.Page_();
    p.DeleteButton()->Focus();
    r.Click(*p.DeleteButton());
    REQUIRE(r.ui.screen.SheetCount() == 1);
    r.ui.Key(Key::Escape);
    CHECK(r.ui.screen.SheetCount() == 0);
    CHECK(r.sent.empty());
    CHECK(r.ui.screen.Focused() == p.DeleteButton());

    r.Click(*p.DeleteButton());
    REQUIRE(r.ui.screen.SheetCount() == 1);
    r.Click(*Find(*r.Top(), "Keep it", Role::Button));
    CHECK(r.ui.screen.SheetCount() == 0);
    CHECK(r.sent.empty());
}

TEST_CASE("Delete voice: a long Voice name wraps in the title instead of running out of the card") {
    Rig r(VoiceState(kAlba));
    r.Page_().DeleteButton();
    OpenDeleteVoiceSheet(r.ui.screen, PageEnv{"en"}, "en_GB-northern_english_male_with_a_long-medium-name");
    r.ui.Frame();
    Sheet* s = r.Top();
    REQUIRE(s);
    // 25 + two title lines of 26.4 + 14 + a line of body (FakeCanvas: 21.6 each) + 22 + 34 + 25: taller than one line
    CHECK(s->Bounds().h > 25 + 26.4f + 14 + 21.6f + 22 + 34 + 25 + 20);
}

// --------------------------------------------------------------------------- Voice download sheet ---

TEST_CASE("Voice download: loading, then the list; Esc closes it and the focus goes back") {
    Rig r;
    VoicesPage& p = r.Page_();
    p.DownloadButton()->Focus();
    r.Click(*p.DownloadButton());
    REQUIRE(r.ui.screen.SheetCount() == 1);
    Sheet* s = r.Top();
    CHECK(s->Access().name == "Download a voice");
    CHECK(Find(*s, "Getting the list of voices"));
    CHECK(r.ui.screen.Focused() == p.Downloads()->CloseButton());  // the focus starts on Close

    r.Events().CatalogueShown(Catalogue());
    r.ui.Frame();
    CHECK_FALSE(Find(*s, "Getting the list of voices"));
    CHECK(Find(*s, "Download en_GB-jenny_dioco-medium", Role::Button));
    CHECK(Find(*s, "Download en_US-amy-medium", Role::Button));
    CHECK_FALSE(Find(*s, "Download en_GB-alba-medium", Role::Button));  // installed: a check, no button
    CHECK(r.ui.canvas.FindText("Installed"));
    CHECK(r.ui.canvas.FindText("63 MB"));

    r.ui.Key(Key::Escape);
    CHECK(r.ui.screen.SheetCount() == 0);
    CHECK(r.ui.screen.Focused() == p.DownloadButton());
}

TEST_CASE("Voice download: the catalogue that fails shows a notice and Try again, which asks again") {
    Rig r;
    VoicesPage& p = r.Page_();
    r.Click(*p.DownloadButton());
    r.Events().CatalogueFailed(VoiceProblem::Network);
    r.ui.Frame();
    Sheet* s = r.Top();
    REQUIRE(s);
    Widget* retry = Find(*s, "Try again", Role::Button);
    REQUIRE(retry);
    std::vector<Widget*> notices;
    Collect(*s, Role::Text, notices);
    bool found = false;
    for (Widget* n : notices) found = found || n->Access().value == T(strings::id::kWindowProblemCatalogueNetwork);
    CHECK(found);

    r.sent.clear();
    r.Click(*retry);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::OpenCatalogue);
    CHECK(Find(*s, "Getting the list of voices"));  // loading again
    CHECK_FALSE(Find(*s, "Try again", Role::Button));
}

TEST_CASE("Voice download: an empty catalogue says so") {
    Rig r;
    r.Click(*r.Page_().DownloadButton());
    r.Events().CatalogueShown({});
    r.ui.Frame();
    CHECK(Find(*r.Top(), "There is no voice to download."));
}

TEST_CASE("Voice download: one download at a time, progress moves the bar and rebuilds nothing") {
    Rig r;
    VoicesPage& p = r.Page_();
    r.Click(*p.DownloadButton());
    r.Events().CatalogueShown(Catalogue());
    r.ui.Frame();
    Sheet* s = r.Top();

    r.sent.clear();
    r.Click(*Find(*s, "Download en_US-amy-medium", Role::Button));
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::Download);
    CHECK(r.Last().name == "en_US-amy-medium");

    // The row shows Cancel and a bar; the other Voice's button is off (one at a time).
    Widget* cancel = Find(*s, "Cancel download of en_US-amy-medium", Role::Button);
    REQUIRE(cancel);
    Widget* other = Find(*s, "Download en_GB-jenny_dioco-medium", Role::Button);
    REQUIRE(other);
    CHECK_FALSE(other->Enabled());
    r.Click(*other);
    CHECK(r.sent.size() == 1);  // nothing was sent for it

    r.Events().DownloadProgress("en_US-amy-medium", 12 * kMiB, 63 * kMiB);
    r.ui.Frame();
    CHECK(r.ui.canvas.FindText("12 of 63 MB"));
    CHECK(Find(*s, "Cancel download of en_US-amy-medium", Role::Button) == cancel);  // the same widget: no rebuild

    // Cancel: asked once, the button says Cancelling and is off.
    r.sent.clear();
    r.Click(*cancel);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::CancelDownload);
    Widget* cancelling = Find(*s, "Cancel download of en_US-amy-medium", Role::Button);
    REQUIRE(cancelling);
    CHECK_FALSE(cancelling->Enabled());
    CHECK(r.ui.canvas.FindText("Cancelling"));
    r.Events().DownloadProgress("en_US-amy-medium", 14 * kMiB, 63 * kMiB);  // a late progress does not undo it
    r.ui.Frame();
    CHECK_FALSE(Find(*s, "Cancel download of en_US-amy-medium", Role::Button)->Enabled());

    // The cancel is answered: a plain note, no error, and Download is on again.
    r.Events().DownloadFailed("en_US-amy-medium", ProblemFacts{VoiceProblem::Cancelled, {}, {}});
    r.ui.Frame();
    CHECK(p.Downloads()->NoteNotice()->Visible());
    CHECK(p.Downloads()->NoteNotice()->Access().value == "The download is cancelled.");
    CHECK(Find(*s, "Download en_US-amy-medium", Role::Button)->Enabled());
    CHECK(Find(*s, "Download en_GB-jenny_dioco-medium", Role::Button)->Enabled());
}

TEST_CASE("Voice download: a finished download is a check and a Done note; a failed one a notice and Try again") {
    Rig r;
    r.Click(*r.Page_().DownloadButton());
    r.Events().CatalogueShown(Catalogue());
    r.ui.Frame();
    Sheet* s = r.Top();

    r.Click(*Find(*s, "Download en_US-amy-medium", Role::Button));
    r.Events().DownloadFailed("en_US-amy-medium", ProblemFacts{VoiceProblem::BadDownload, {}, {}});
    r.ui.Frame();
    CHECK(Find(*s, "Try en_US-amy-medium again", Role::Button));
    std::vector<Widget*> texts;
    Collect(*s, Role::Text, texts);
    bool sawError = false;
    for (Widget* n : texts) sawError = sawError || n->Access().value == T(strings::id::kWindowProblemBadDownload);
    CHECK(sawError);

    r.sent.clear();
    r.Click(*Find(*s, "Try en_US-amy-medium again", Role::Button));
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::Download);

    r.Events().DownloadDone("en_US-amy-medium");
    r.ui.Frame();
    CHECK_FALSE(Find(*s, "Download en_US-amy-medium", Role::Button));
    CHECK_FALSE(Find(*s, "Try en_US-amy-medium again", Role::Button));
    CHECK(Find(*s, "Download en_GB-jenny_dioco-medium", Role::Button)->Enabled());
    texts.clear();
    Collect(*s, Role::Text, texts);
    bool sawDone = false;
    for (Widget* n : texts)
        sawDone = sawDone || n->Access().value == "en_US-amy-medium is installed. You can choose it on the voices page.";
    CHECK(sawDone);
}

TEST_CASE("Voice download: the scroll position and the focus are kept when the list is drawn again") {
    Rig r;
    r.Click(*r.Page_().DownloadButton());
    r.Events().CatalogueShown(Catalogue(12));  // longer than the card: the list scrolls
    r.ui.Frame();
    Sheet* s = r.Top();
    CatalogueBody* body = r.Page_().Downloads()->Body();
    REQUIRE(body->CanScroll());
    CHECK(s->Bounds().h <= 520.5f);  // the sheet's maximum

    body->ScrollTo(120);
    r.ui.Frame();
    REQUIRE(body->Offset() == doctest::Approx(120));

    // The focus is on a row's button; the click starts a download and the rows are built again.
    Widget* get = Find(*s, "Download en_US-extra3-medium", Role::Button);
    REQUIRE(get);
    r.ui.screen.SetFocus(get);
    r.ui.Key(Key::Enter);
    r.ui.Frame();
    CHECK(body->Offset() == doctest::Approx(120));
    Widget* cancel = Find(*s, "Cancel download of en_US-extra3-medium", Role::Button);
    REQUIRE(cancel);
    CHECK(r.ui.screen.Focused() == cancel);  // Download -> Cancel: the focus follows the Voice

    // Progress does not move the list either.
    r.Events().DownloadProgress("en_US-extra3-medium", 5 * kMiB, 20 * kMiB);
    r.ui.Frame();
    CHECK(body->Offset() == doctest::Approx(120));
    CHECK(r.ui.screen.Focused() == cancel);

    // A finished download scrolls its row into view.
    body->ScrollTo(0);
    r.Events().DownloadDone("en_US-extra3-medium");
    r.ui.Frame();
    r.ui.Frame();
    CHECK(body->Offset() > 0);
}

TEST_CASE("Voice download: closing the sheet does not stop a download, and the sheet shows it again") {
    Rig r;
    VoicesPage& p = r.Page_();
    r.Click(*p.DownloadButton());
    r.Events().CatalogueShown(Catalogue());
    r.ui.Frame();
    r.Click(*Find(*r.Top(), "Download en_US-amy-medium", Role::Button));
    r.ui.Key(Key::Escape);
    CHECK(r.ui.screen.SheetCount() == 0);
    r.Events().DownloadProgress("en_US-amy-medium", 3 * kMiB, 63 * kMiB);  // still going; nothing to draw into
    CHECK(r.ui.screen.SheetCount() == 0);
    CHECK(r.settings.Library().CurrentDownload().has_value());

    // The catalogue comes again (a new sheet): the download shows on its row.
    r.settings.Library().ClearNote();
    r.Events().CatalogueShown(Catalogue());
    r.ui.Frame();
    REQUIRE(r.ui.screen.SheetCount() == 1);
    CHECK(Find(*r.Top(), "Cancel download of en_US-amy-medium", Role::Button));
}

// ------------------------------------------------------------------------------- Result sheet ---

TEST_CASE("Result sheet: Voice added, with the Language's name and Close") {
    Rig r;
    r.Events().VoiceAdded({"en_US-amy-medium", "eng"});
    r.ui.Frame();
    REQUIRE(r.ui.screen.SheetCount() == 1);
    Sheet* s = r.Top();
    CHECK(s->Access().name == "Voice added");
    CHECK(Find(*s, "Close", Role::Button));
    CHECK_FALSE(Find(*s, "Choose another file", Role::Button)->Visible());
    std::vector<Widget*> texts;
    Collect(*s, Role::Text, texts);
    REQUIRE_FALSE(texts.empty());
    CHECK(texts.front()->Access().name == "Done");
    CHECK(texts.front()->Access().value == "en_US-amy-medium is added. It speaks English. You can choose it on the voices page.");
    CHECK(r.ui.screen.Focused() == Find(*s, "Close", Role::Button));
}

TEST_CASE("Result sheet: a refused file says why and offers another file, which asks the host again") {
    Rig r;
    r.Events().VoiceRefused(ProblemFacts{VoiceProblem::UnsupportedLanguage, "German", "low"});
    r.ui.Frame();
    Sheet* s = r.Top();
    REQUIRE(s);
    CHECK(s->Access().name == "Voice not added");
    std::vector<Widget*> texts;
    Collect(*s, Role::Text, texts);
    REQUIRE_FALSE(texts.empty());
    CHECK(texts.front()->Access().name == "Problem");
    CHECK(texts.front()->Access().value ==
          "Lexia can't read German yet. Choose a voice for a language that Lexia reads.");
    Widget* again = Find(*s, "Choose another file", Role::Button);
    REQUIRE(again);
    CHECK(again->Visible());

    r.sent.clear();
    r.Click(*again);
    REQUIRE(r.sent.size() == 1);
    CHECK(r.Last().type == PageMessageType::AddFromFile);
    CHECK(r.ui.screen.SheetCount() == 0);
}

TEST_CASE("Result sheet: delete and Preset results; a second result replaces the first in the open sheet") {
    Rig r;
    r.Events().VoiceDeleteFailed(kAlba, VoiceProblem::CannotWrite);
    r.ui.Frame();
    CHECK(r.Top()->Access().name == "Voice not deleted");
    r.Events().PresetSaved("Slow reading.json");
    r.ui.Frame();
    REQUIRE(r.ui.screen.SheetCount() == 1);  // one sheet, new words
    CHECK(r.Top()->Access().name == "Preset saved");
    r.ui.Key(Key::Escape);
    CHECK(r.ui.screen.SheetCount() == 0);

    r.Events().PresetLoaded("Slow reading.json");
    r.ui.Frame();
    CHECK(r.Top()->Access().name == "Preset loaded");
    r.ui.Key(Key::Escape);
    r.Events().PresetFailed(PresetProblem::NoVoice);
    r.ui.Frame();
    CHECK(r.Top()->Access().name == "Preset not used");
}

TEST_CASE("a result that comes while another page shows opens when the Voices page does") {
    Rig r;
    r.settings.ShowPage(Page::Home);
    r.Events().VoiceAdded({"en_US-amy-medium", "eng"});
    r.ui.Frame();
    CHECK(r.ui.screen.SheetCount() == 0);
    r.settings.ShowPage(Page::Voices);
    r.ui.Frame();
    CHECK(r.ui.screen.SheetCount() == 1);
    CHECK(r.Top()->Access().name == "Voice added");
}

TEST_CASE("a page switch from outside closes the sheet that belongs to the page") {
    Rig r;
    r.Click(*r.Page_().DownloadButton());
    REQUIRE(r.ui.screen.SheetCount() == 1);
    r.settings.ShowPage(Page::Help);
    CHECK(r.ui.screen.SheetCount() == 0);
    r.ui.Frame();  // nothing calls back into the page that went
}

// ------------------------------------------------------------------------- the library state ---

TEST_CASE("VoiceLibraryState: the catalogue, one download at a time, the cancel flag") {
    VoiceLibraryState lib;
    std::vector<VoiceLibraryState::Change> changes;
    lib.SetOnChange([&](VoiceLibraryState::Change c) { changes.push_back(c); });

    lib.BeginSheet();
    CHECK(lib.Loading());
    CHECK_FALSE(lib.HaveVoices());
    lib.CatalogueShown(Catalogue());
    CHECK_FALSE(lib.Loading());
    CHECK(lib.HaveVoices());
    CHECK(changes.back() == VoiceLibraryState::Change::Catalogue);

    const CatalogueEntry amy = Catalogue()[2];
    const CatalogueEntry jenny = Catalogue()[1];
    CHECK(lib.StartDownload(amy));
    CHECK_FALSE(lib.StartDownload(jenny));  // one at a time
    CHECK(lib.RequestCancel());
    CHECK_FALSE(lib.RequestCancel());       // once
    lib.DownloadProgress(amy.name, 1, 2);
    CHECK(lib.CurrentDownload()->cancelling);  // the host's progress never clears it
    CHECK(changes.back() == VoiceLibraryState::Change::Progress);

    lib.DownloadFailed(amy.name, ProblemFacts{VoiceProblem::Cancelled, {}, {}});
    CHECK_FALSE(lib.CurrentDownload().has_value());
    CHECK_FALSE(lib.Failed().has_value());
    CHECK(lib.NoteKind() == VoiceLibraryState::Note::Cancelled);

    CHECK(lib.StartDownload(amy));
    lib.DownloadDone(amy.name);
    CHECK(lib.IsInstalled(amy));
    CHECK(lib.NoteKind() == VoiceLibraryState::Note::Done);
    CHECK(lib.TakeReveal() == amy.name);
    CHECK(lib.TakeReveal().empty());  // read once

    lib.BeginSheet();  // a new sheet forgets what was installed since the catalogue came
    CHECK_FALSE(lib.IsInstalled(amy));
    CHECK(lib.NoteKind() == VoiceLibraryState::Note::None);
}

TEST_CASE("VoiceLibraryState: a progress message the sheet did not start is a change of rows") {
    VoiceLibraryState lib;
    VoiceLibraryState::Change last = VoiceLibraryState::Change::Catalogue;
    lib.SetOnChange([&](VoiceLibraryState::Change c) { last = c; });
    lib.DownloadProgress("en_US-amy-medium", 1, 10);
    CHECK(last == VoiceLibraryState::Change::Rows);
    lib.DownloadProgress("en_US-amy-medium", 2, 10);
    CHECK(last == VoiceLibraryState::Change::Progress);
}

TEST_CASE("VoiceLibraryState: results wait until a sheet takes them") {
    VoiceLibraryState lib;
    lib.PresetSaved("a.json");
    REQUIRE(lib.HasResult());
    const std::optional<ResultOutcome> r = lib.TakeResult();
    REQUIRE(r);
    CHECK(r->kind == ResultKind::PresetSaved);
    CHECK_FALSE(lib.HasResult());
    const ResultTexts t = DescribeResult("en", *r);
    CHECK(t.ok);
    CHECK(t.text == "The speed, pitch and volume are saved in a.json.");
}

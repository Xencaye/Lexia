#include "settingsshot_pages.h"

#include <memory>
#include <string>

#include "native_ui/controls.h"
#include "native_ui/display.h"
#include "native_ui/select.h"
#include "native_ui/pages/about_page.h"
#include "native_ui/pages/home_page.h"
#include "native_ui/pages/report_page.h"
#include "native_ui/pages/voices_page.h"
#include "native_ui/settings_ui.h"
#include "native_ui/slider.h"
#include "settingsshot_sample.h"

// The sample texts below are test data of a developer tool (like lexia_barshot's), not text the app shows:
// they stay literals, outside strings/.
namespace shot {

using namespace nui;

void ForcePress(Screen& screen, Widget& w) {
    const RectF r = w.Abs();
    MouseEvent move;
    move.kind = MouseKind::Move;
    move.pos = {r.x + r.w / 2, r.y + r.h / 2};
    screen.HandleMouse(move);
    MouseEvent down = move;
    down.kind = MouseKind::Down;
    down.button = MouseButton::Left;
    screen.HandleMouse(down);
}

void ForceKeyboardFocus(Screen& screen, Widget& w) {
    screen.SetFocus(&w);
    // Any key turns the focus-visible flag on (the ring and the grown slider thumb). A key the toolkit
    // does not name does nothing else.
    KeyEvent key;
    key.key = Key::Other;
    key.code = 0;
    screen.HandleKey(key);
}

namespace {

// A 1 DIP dotted rule across its bounds (the design's `border: 1px dotted`, here a widget of its own
// because the pages draw their rules with it).
class DottedRule : public Widget {
public:
    void Paint(Canvas& canvas, const Palette& p) override {
        const RectF r = Abs();
        canvas.DottedLine({r.x, r.y}, {r.Right(), r.y}, p.line);
    }
};

// Places widgets left to right on a row, `gap` apart, each at the size its Measure gives.
class Flow {
public:
    Flow(Widget& parent, Canvas& canvas, float x, float y, float gap = 16)
        : parent_(parent), canvas_(canvas), x0_(x), x_(x), y_(y), gap_(gap) {}

    // Adds a widget on the row, vertically centred on a row of height `rowH` (0 = top aligned).
    template <class T, class... A>
    T* Put(float rowH, A&&... args) {
        T* w = parent_.Add<T>(std::forward<A>(args)...);
        const SizeF s = w->Measure(canvas_, 10000);
        w->SetBounds({x_, y_ + (rowH > s.h ? (rowH - s.h) / 2 : 0.0f), s.w, s.h});
        x_ += s.w + gap_;
        return w;
    }
    // A widget of a given width (sliders, selects, paragraphs, notices): the height follows Measure.
    template <class T, class... A>
    T* PutWide(float width, A&&... args) {
        T* w = parent_.Add<T>(std::forward<A>(args)...);
        const SizeF s = w->Measure(canvas_, width);
        w->SetBounds({x_, y_, width, s.h});
        x_ += width + gap_;
        return w;
    }
    // A new row `dy` lower, back at the left edge.
    void Down(float dy) {
        y_ += dy;
        x_ = x0_;
    }
    float Y() const { return y_; }

private:
    Widget& parent_;
    Canvas& canvas_;
    float x0_, x_, y_, gap_;
};

// A small caption over a group (12 px, ink).
void Caption(Widget& parent, Canvas& canvas, float x, float y, const std::string& text) {
    Label* l = parent.Add<Label>(text);
    l->SetStyle(12);
    const SizeF s = l->Measure(canvas, 10000);
    l->SetBounds({x, y, s.w, s.h});
}

void BuildLeftColumn(Screen& screen, Canvas& canvas) {
    Widget& root = screen.Root();
    constexpr float x = 24;

    // ---- icon buttons: idle, hover, pressed (SetPressed), disabled, and a real mouse press
    Caption(root, canvas, x, 12, "Icon buttons: idle, hover, pressed, disabled, mouse down");
    Flow icons(root, canvas, x, 30, 12);
    icons.Put<IconButton>(28, "home-05");
    ForceHover(*icons.Put<IconButton>(28, "settings"));
    icons.Put<IconButton>(28, "pause-03")->SetPressed(true);
    icons.Put<IconButton>(28, "trash-02")->SetEnabled(false);
    IconButton* down = icons.Put<IconButton>(28, "play-03");

    // ---- links
    Caption(root, canvas, x, 68, "Links: icon, hover, current (bold), small");
    Flow links(root, canvas, x, 86, 22);
    links.Put<TextLink>(22, "Voices", "volume-05");
    ForceHover(*links.Put<TextLink>(22, "Settings", "settings"));
    links.Put<NavLink>(22, "Home")->SetCurrent(true);
    links.Put<TextLink>(22, "Privacy")->SetSmall(true);

    // ---- buttons
    Caption(root, canvas, x, 118, "Buttons: normal, hover, quiet; small, small quiet, disabled");
    Flow b1(root, canvas, x, 136, 14);
    b1.Put<Button>(34, "Save");
    ForceHover(*b1.Put<Button>(34, "Download"));
    b1.Put<Button>(34, "Cancel")->SetQuiet(true);
    Flow b2(root, canvas, x, 178, 14);
    b2.Put<Button>(34, "Delete")->SetSmall(true);
    {
        Button* q = b2.Put<Button>(34, "Close");
        q->SetSmall(true);
        q->SetQuiet(true);
    }
    b2.Put<Button>(34, "Please wait")->SetEnabled(false);

    // ---- select
    Caption(root, canvas, x, 222, "Select: normal, disabled");
    Flow sel(root, canvas, x, 240, 16);
    Select* s1 = sel.PutWide<Select>(170);
    s1->SetItems({"Amy", "Anna", "Dimitar"});
    s1->SetSelected(1);
    Select* s2 = sel.PutWide<Select>(170);
    s2->SetItems({"Amy", "Anna", "Dimitar"});
    s2->SetSelected(0);
    s2->SetEnabled(false);

    // ---- sliders
    Caption(root, canvas, x, 276, "Sliders: linear, log2 (Speed, 1.0 in the middle), keyboard focus, disabled");
    Flow sl(root, canvas, x, 294, 0);
    sl.PutWide<Slider>(340)->SetRange({0.5, 1.5, 1.0, 0.05, false});
    sl.Down(30);
    sl.PutWide<Slider>(340)->SetRange({0.5, 2.0, 1.0, 0.05, true});
    sl.Down(30);
    Slider* focused = sl.PutWide<Slider>(340);
    focused->SetRange({0.0, 1.0, 0.7, 0.05, false});
    sl.Down(30);
    sl.PutWide<Slider>(340)->Clear();

    // ---- switch rows
    Caption(root, canvas, x, 424, "Switch rows: off, on, hover, disabled");
    Flow sw(root, canvas, x, 442, 12);
    sw.PutWide<SwitchRow>(170, "Run at startup");
    sw.PutWide<SwitchRow>(170, "Start minimised")->SetChecked(true);
    sw.Down(30);
    ForceHover(*sw.PutWide<SwitchRow>(170, "Left-handed"));
    SwitchRow* dis = sw.PutWide<SwitchRow>(170, "Run as admin");
    dis->SetChecked(true);
    dis->SetEnabled(false);

    // ---- key chips
    Caption(root, canvas, x, 508, "Key chips: set, none, capturing");
    Flow chips(root, canvas, x, 526, 14);
    chips.Put<KeyChip>(24, "F8")->SetState(KeyChip::State::Set);
    chips.Put<KeyChip>(24, "None");  // State::None: the quiet look
    chips.Put<KeyChip>(24, "Press a key")->SetState(KeyChip::State::Capturing);

    // The one real press and the one keyboard focus (they are a single pointer and a single focus).
    ForcePress(screen, *down);
    ForceKeyboardFocus(screen, *focused);
}

void BuildRightColumn(Screen& screen, Canvas& canvas) {
    Widget& root = screen.Root();
    constexpr float x = 412, w = 364;

    // ---- type
    Caption(root, canvas, x, 12, "Type: 22 bold, 16, 16 bold, 13");
    // Each label gets its style before it is measured, so the next one starts after its real width.
    float lx = x;
    auto label = [&](const char* text, float size, FontWeight weight) {
        Label* l = root.Add<Label>(text);
        l->SetStyle(size, weight);
        const SizeF s = l->Measure(canvas, 10000);
        l->SetBounds({lx, 30, s.w, s.h});
        lx += s.w + 16;
    };
    label("Voices", 22, FontWeight::Bold);
    label("Atkinson Hyperlegible", 16, FontWeight::Regular);
    label("Next", 16, FontWeight::Bold);
    label("13 px", 13, FontWeight::Regular);

    // ---- notices
    Caption(root, canvas, x, 70, "Notices: good, bad");
    Flow n(root, canvas, x, 88, 0);
    Notice* ok = n.PutWide<Notice>(w);
    ok->SetKind(Notice::Kind::Ok);
    ok->SetLead("Done.");
    ok->SetText("The Voice was added.");
    ok->SetBounds({x, 88, w, ok->Measure(canvas, w).h});
    n.Down(ok->Bounds().h + 10);
    Notice* bad = n.PutWide<Notice>(w);
    bad->SetKind(Notice::Kind::Bad);
    bad->SetLead("Failed.");
    bad->SetText("That file is not a Voice Lexia can use. Choose a Medium Voice of a Language Lexia reads.");
    bad->SetBounds({x, n.Y(), w, bad->Measure(canvas, w).h});
    const float afterNotices = n.Y() + bad->Bounds().h;

    // ---- progress
    Caption(root, canvas, x, afterNotices + 12, "Progress bar: 40 %, full");
    Flow p(root, canvas, x, afterNotices + 30, 0);
    p.PutWide<ProgressBar>(w)->SetFraction(0.4);
    p.Down(14);
    p.PutWide<ProgressBar>(w)->SetFraction(1.0);
    const float afterProgress = p.Y() + 6;

    // ---- paragraph with bold runs and indents
    Caption(root, canvas, x, afterProgress + 12, "Paragraph: bold {name} runs, per-line indents (27, 23, 15, 0)");
    Flow para(root, canvas, x, afterProgress + 30, 0);
    Paragraph* pa = para.PutWide<Paragraph>(w);
    pa->SetText("Press {key} over any text and {app} reads it aloud. Drag to read a part, or click to read a block.",
                {{"key", "F8"}, {"app", "Lexia"}});
    pa->SetIndents({27, 23, 15});
    pa->SetBounds({x, para.Y(), w, pa->Measure(canvas, w).h});
    const float afterPara = para.Y() + pa->Bounds().h;

    // ---- dotted rule
    Caption(root, canvas, x, afterPara + 10, "Dotted rule");
    DottedRule* rule = root.Add<DottedRule>();
    rule->SetBounds({x, afterPara + 32, w, 1});

    // ---- icons and engravings
    Caption(root, canvas, x, afterPara + 42, "Icons in accent; engravings in their tints");
    float ix = x;
    for (const char* name : {"volume-05", "settings", "heart", "github", "discord", "mail-01"}) {
        IconWidget* ic = root.Add<IconWidget>(name, ColourRole::Accent);
        ic->SetBounds({ix, afterPara + 60, 24, 24});
        ix += 36;
    }
    const float py = afterPara + 94;
    struct Pic {
        const char* name;
        PictureTint tint;
        float w, h, opacity;
    };
    // Sizes keep the pictures' own proportions (the old CSS aspect-ratio) and the old opacities.
    const Pic pics[] = {{"bee", PictureTint::BeeOff, 69, 60, 0.9f},
                        {"bee", PictureTint::BeeOn, 69, 60, 0.9f},
                        {"flute", PictureTint::Flute, 40, 60, 1.0f},
                        {"dog", PictureTint::Dog, 61, 60, 0.66f}};
    float px = x;
    for (const Pic& pic : pics) {
        PictureWidget* pw = root.Add<PictureWidget>(pic.name, pic.tint);
        pw->SetOpacity(pic.opacity);
        pw->SetBounds({px, py, pic.w, pic.h});
        px += pic.w + 12;
    }
    PictureWidget* wine = root.Add<PictureWidget>("wine", PictureTint::Wine);
    wine->SetBounds({x, py + 70, 106, 58});
    PictureWidget* otter = root.Add<PictureWidget>("otter", PictureTint::Otter);
    otter->SetOpacity(0.55f);
    otter->SetBounds({x + 130, py + 70, 123, 58});
}

// Opens the sheet `name` of the Voices page, with the data of the old mock host's ?sheet= states. The Voice
// events go through the same sink the window uses (VoiceTab calls it), so the sheets draw what they would draw
// in the app. False for a name that is none of them.
bool ApplySheet(const std::string& name, SettingsUi& ui, Screen& screen) {
    if (ui.CurrentPage() != Page::Voices) return false;
    VoicesPage& page = *static_cast<VoicesPage*>(ui.Current());
    VoiceEventSink& events = ui.VoiceEvents();
    constexpr uint64_t kMiB = 1024 * 1024;
    const std::string amy = "en_US-amy-medium";

    if (name == "delete") {
        page.OpenDeleteSheet();
    } else if (name == "catalogue" || name == "offline" || name == "downloading" || name == "done" || name == "failed" ||
               name == "failed-baddownload") {
        page.OpenDownloadSheet();
        if (name == "offline") {
            events.CatalogueFailed(VoiceProblem::Network);
        } else {
            events.CatalogueShown(SampleCatalogue());
            if (name == "downloading") {
                for (const CatalogueEntry& e : SampleCatalogue()) {
                    if (e.name == amy) ui.Library().StartDownload(e);
                }
                events.DownloadProgress(amy, 12 * kMiB, 63 * kMiB);
            } else if (name == "done") {
                events.DownloadDone(amy);
            } else if (name == "failed") {
                events.DownloadFailed(amy, ProblemFacts{VoiceProblem::Network, {}, {}});
            } else if (name == "failed-baddownload") {
                events.DownloadFailed(amy, ProblemFacts{VoiceProblem::BadDownload, {}, {}});
            }
        }
    } else if (name == "added") {
        events.VoiceAdded({amy, "eng"});
    } else if (name == "refused") {
        events.VoiceRefused(ProblemFacts{VoiceProblem::UnsupportedLanguage, "German", "low"});
    } else if (name == "refused-notmedium") {
        events.VoiceRefused(ProblemFacts{VoiceProblem::NotMedium, "German", "low"});
    } else if (name == "deleteFailed") {
        events.VoiceDeleteFailed("en_GB-alba-medium", VoiceProblem::CannotWrite);
    } else if (name == "presetSaved") {
        events.PresetSaved("Slow reading.json");
    } else if (name == "presetLoaded") {
        events.PresetLoaded("en_US-amy-medium", true);
    } else if (name == "presetFailed") {
        events.PresetFailed(PresetProblem::BadFile, std::string());
    } else if (name == "presetLoadedOther") {
        events.PresetLoaded("en_GB-alba-medium", false);  // a Voice that is not the active one
    } else if (name == "presetNotInstalled") {
        events.PresetFailed(PresetProblem::VoiceNotInstalled, "insert voice");
    } else {
        return false;
    }
    // The reference pictures show the sheet's focus ring (the old shots were taken after a key): any key turns
    // the focus-visible flag on.
    KeyEvent key;
    key.key = Key::Other;
    screen.HandleKey(key);
    return true;
}

// The real SettingsUi on a sample state. The SettingsUi is kept for the life of the tool: the Voices page's
// sheets call back into it (the Voice library state is its), and the picture is drawn after this returns.
bool BuildRealPage(Page page, Screen& screen, bool dark, const ShotOptions& options) {
    static std::unique_ptr<SettingsUi> keep;
    keep = std::make_unique<SettingsUi>(screen, "en");
    SettingsUi& ui = *keep;
    // The Delete sheet is for a Voice the person added: its picture needs that one active.
    const std::string voices = options.voices.empty() && options.sheet == "delete" ? "user" : options.voices;
    UiState state = SampleState(options.on, dark, voices);
    if (options.switches) {  // the owner's reference shot of the switches (start minimised is on by default)
        state.runAtStartup = true;
        state.runAsAdmin = true;
        state.leftHanded = true;
        state.keepWordMark = true;
    }
    ui.SetState(state);
    // The Settings page opens on a Key and may start a capture, as the old ?select= / ?capture= did.
    ShowOptions show;
    show.selectAction = options.select;
    show.capture = options.capture;
    ui.ShowPage(page, show);
    if (!options.sheet.empty() && !ApplySheet(options.sheet, ui, screen)) return false;
    const std::string& h = options.hover;
    if (h.empty()) return true;
    Widget* target = nullptr;
    Page linkPage = Page::Home;
    if (h == "power" && page == Page::Home) target = static_cast<HomePage*>(ui.Current())->Power();
    else if (h == "enable" && page == Page::Home) target = static_cast<HomePage*>(ui.Current())->Enable();
    else if (h == "volume") target = ui.GetFrame().Volume();
    else if (ParsePageName(h, linkPage)) target = ui.GetFrame().LinkFor(linkPage);
    else if (page == Page::Report && h == "mail") target = static_cast<ReportPage*>(ui.Current())->MailLink();
    else if (page == Page::Report && h == "discord") target = static_cast<ReportPage*>(ui.Current())->DiscordLink();
    else if (page == Page::Report && h == "github") target = static_cast<ReportPage*>(ui.Current())->GithubLink();
    else if (page == Page::About && h == "itch") target = static_cast<AboutPage*>(ui.Current())->ItchButton();
    else if (page == Page::About && h == "notices") target = static_cast<AboutPage*>(ui.Current())->NoticesLink();
    else if (page == Page::Voices) {
        VoicesPage* v = static_cast<VoicesPage*>(ui.Current());
        if (h == "delete") target = v->DeleteButton();
        else if (h == "download") target = v->DownloadButton();
        else if (h == "addfile") target = v->AddFileButton();
        else if (h == "speed" || h == "pitch" || h == "volume") target = v->SliderFor(h);
        else if (h == "reset-pitch") target = v->ResetFor("pitch");
        else if (h == "save") target = v->SavePresetLink();
        else if (h == "load") target = v->LoadPresetLink();
    }
    if (!target) return false;  // a hover that this page does not have
    ForceHover(*target);
    return true;
}

}  // namespace

bool BuildShotPage(std::string_view page, Screen& screen, Canvas& canvas, bool dark, const ShotOptions& options) {
    if (page == "gallery") {
        BuildLeftColumn(screen, canvas);
        BuildRightColumn(screen, canvas);
        return true;
    }
    Page real = Page::Home;
    if (ParsePageName(page, real)) return BuildRealPage(real, screen, dark, options);
    return false;
}

}  // namespace shot

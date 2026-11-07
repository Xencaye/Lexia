#include "native_ui/pages/voices_page.h"

#include <algorithm>
#include <cmath>

#include "native_ui/pages/layout.h"
#include "native_ui/pages/voice_text.h"
#include "native_ui/screen.h"
#include "strings/string_ids.h"

namespace nui {

namespace {

namespace sid = strings::id;

constexpr float kIconButtonSize = 28;   // the icon buttons of the rows (components.css `.icon-btn`)
constexpr float kSliderH = 28;          // the slider's box
constexpr float kSelectTopOffset = 1;   // a 28 tall control in a 30 tall row

// The Language's name in a sentence ("English"), in the window's Interface language.
std::string LanguageNameOf(const std::string& interfaceLanguage, const std::string& languageId) {
    return strings::LanguageName(interfaceLanguage, languageId);
}

}  // namespace

VoicesPage::VoicesPage(const PageEnv& env, VoiceLibraryState& library) : PageView(Page::Voices, env), library_(library) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(sid::kWindowNavVoices));

    // ---- the Voice row: label, the choice, three icon buttons
    voiceIcon_ = Add<IconWidget>("user-profile-03", ColourRole::Accent);
    voiceLabel_ = Add<Label>(T(sid::kWindowVoiceVoice));
    voiceLabel_->SetStyle(layout::kVoiceLabelFont);
    voiceLabel_->SetAccessHidden(true);  // the Select below says the same word

    voice_ = Add<Select>();
    voice_->SetName(T(sid::kWindowVoiceVoice));
    voice_->SetAutomationId("voices.voice");
    voice_->SetBounds({layout::kVoiceControlX, layout::kVoiceRowTop + kSelectTopOffset, layout::kVoiceControlW,
                       kIconButtonSize});
    voice_->SetEnabled(false);  // until the first state
    // The choice is asked for; the select keeps showing the old one until the next state moves it.
    voice_->SetOnChoose([this](int index) {
        if (index < 0 || static_cast<size_t>(index) >= installed_.size()) return;
        PageMessage m = MakeMessage(PageMessageType::SetVoice, "setVoice");
        m.language = language_;
        m.voice = installed_[static_cast<size_t>(index)];
        Emit(m);
    });

    float x = layout::kVoiceSideX;
    auto sideButton = [&](const char* icon, const char* textId) {
        IconButton* b = Add<IconButton>(icon);
        b->SetBounds({x, layout::kVoiceRowTop + kSelectTopOffset, kIconButtonSize, kIconButtonSize});
        b->SetName(T(textId));
        b->SetTooltip(T(textId));
        x += kIconButtonSize + layout::kVoiceSideGap;
        return b;
    };
    download_ = sideButton("download", sid::kWindowVoiceDownload);
    download_->SetAutomationId("voices.download");
    download_->SetOnActivate([this] { OpenDownloadSheet(); });
    addFile_ = sideButton("upload-04", sid::kWindowVoiceAddFromFile);
    addFile_->SetAutomationId("voices.addFile");
    addFile_->SetOnActivate([this] { Emit(MakeMessage(PageMessageType::AddFromFile, "addFromFile")); });
    delete_ = sideButton("trash-02", sid::kWindowVoiceDelete);
    delete_->SetAutomationId("voices.delete");
    delete_->SetFocusWhenDisabled(true);  // aria-disabled: it explains itself with its tooltip, so it keeps the focus
    delete_->SetOnActivate([this] { OpenDeleteSheet(); });

    // ---- the note that takes the place of the short rule while no Voice is installed
    none_ = Add<Paragraph>();
    none_->SetStyle(layout::kVoiceNoneFont);
    none_->SetLineHeight(layout::kVoiceNoneLine);
    none_->SetBounds(layout::kVoiceNone);
    none_->SetVisible(false);
    rule_ = Add<DottedRule>();
    rule_->SetBounds(layout::kVoiceRule);

    // ---- the three controls
    rows_.push_back(AddControl("speed", layout::kSpeedRowTop, "fast-forward", sid::kWindowVoiceSpeed));
    rows_.push_back(AddControl("pitch", layout::kPitchRowTop, "waveform", sid::kWindowVoicePitch));
    rows_.push_back(AddControl("volume", layout::kVolumeRowTop, "volume-06", sid::kWindowVoiceVolume));

    // ---- the otter and the preset links
    otter_ = Add<PictureWidget>("otter", PictureTint::Otter);
    otter_->SetOpacity(layout::kOtterOpacity);
    otter_->SetBounds(layout::kOtter);

    save_ = Add<TextLink>(T(sid::kWindowVoiceSavePreset), "save-02");
    load_ = Add<TextLink>(T(sid::kWindowVoiceLoadPreset), "tag");
    for (TextLink* l : {save_, load_}) {
        l->SetFontSize(layout::kPresetsFont);
        l->SetIconSize(layout::kPresetsIcon);
        l->SetReserveBold(false);  // a plain `.text-link` is never bold
        l->SetEnabled(false);      // until a Voice is there (the first state)
    }
    save_->SetAutomationId("voices.savePreset");
    load_->SetAutomationId("voices.loadPreset");
    save_->SetOnActivate([this] { Emit(MakeMessage(PageMessageType::SavePresetFile, "savePresetFile")); });
    load_->SetOnActivate([this] { Emit(MakeMessage(PageMessageType::LoadPresetFile, "loadPresetFile")); });
}

VoicesPage::~VoicesPage() = default;

VoicesPage::ControlRow VoicesPage::AddControl(const char* name, float top, const char* icon, const char* textId) {
    ControlRow r;
    r.name = name;
    r.top = top;
    r.icon = Add<IconWidget>(icon, ColourRole::Accent);
    r.label = Add<Label>(T(textId));
    r.label->SetStyle(layout::kVoiceLabelFont);
    r.label->SetAccessHidden(true);  // the slider carries the same word as its name
    r.slider = Add<Slider>();
    r.slider->SetBounds({layout::kVoiceControlX, top + (layout::kVoiceRowH - kSliderH) / 2, layout::kVoiceControlW, kSliderH});
    r.slider->SetName(T(textId));
    r.slider->SetAutomationId(std::string("voices.") + name);
    r.slider->Clear();  // disabled until a state has a Voice
    // A drag or key asks for a value on the grid of the state; the host answers with a new state.
    r.slider->SetOnValue([this, n = std::string(name)](double v) {
        PageMessage m = MakeMessage(PageMessageType::SetControl, "setControl");
        m.name = n;
        m.value = v;
        Emit(m);
    });
    r.reset = Add<IconButton>("refresh-01");
    r.reset->SetBounds({layout::kVoiceSideX, top + (layout::kVoiceRowH - kIconButtonSize) / 2, kIconButtonSize, kIconButtonSize});
    r.reset->SetAutomationId(std::string("voices.reset.") + name);
    r.reset->SetVisible(false);  // only beside a value that is not the default
    r.reset->SetOnActivate([this, n = std::string(name)] {
        PageMessage m = MakeMessage(PageMessageType::ResetControl, "resetControl");
        m.name = n;
        Emit(m);
    });
    return r;
}

VoicesPage::ControlRow* VoicesPage::Row(const std::string& name) {
    for (ControlRow& r : rows_) {
        if (r.name == name) return &r;
    }
    return nullptr;
}

const VoicesPage::ControlRow* VoicesPage::Row(const std::string& name) const {
    for (const ControlRow& r : rows_) {
        if (r.name == name) return &r;
    }
    return nullptr;
}

Slider* VoicesPage::SliderFor(const std::string& name) const {
    const ControlRow* r = Row(name);
    return r ? r->slider : nullptr;
}

IconButton* VoicesPage::ResetFor(const std::string& name) const {
    const ControlRow* r = Row(name);
    return r ? r->reset : nullptr;
}

// ------------------------------------------------------------------------------------ layout ---

// The labels end at x = 252 with their icon 8 to the left, each centred on its row. Their width is the
// word's, so this needs the canvas.
void VoicesPage::Relayout(Canvas& canvas) {
    auto place = [&](IconWidget* icon, Label* label, float top) {
        const SizeF s = label->Measure(canvas, 10000);
        const float cy = top + layout::kVoiceRowH / 2;
        const float lx = layout::kVoiceLabelRight - s.w;
        label->SetBounds({lx, cy - s.h / 2, s.w, s.h});
        icon->SetBounds({lx - layout::kVoiceLabelGap - layout::kVoiceLabelIcon, cy - layout::kVoiceLabelIcon / 2,
                         layout::kVoiceLabelIcon, layout::kVoiceLabelIcon});
    };
    place(voiceIcon_, voiceLabel_, layout::kVoiceRowTop);
    for (ControlRow& r : rows_) place(r.icon, r.label, r.top);

    // The preset links in a row, 18 apart, centred on the 24 tall row.
    float x = layout::kPresetsX;
    for (TextLink* l : {save_, load_}) {
        const SizeF s = l->Measure(canvas, 10000);
        l->SetBounds({x, layout::kPresetsY + (layout::kPresetsH - s.h) / 2, s.w, s.h});
        x += s.w + layout::kPresetsGap;
    }
    // The note under the select is as tall as its lines.
    const float noteH = none_->Measure(canvas, layout::kVoiceNone.w).h;
    none_->SetBounds({layout::kVoiceNone.x, layout::kVoiceNone.y, layout::kVoiceNone.w, noteH});
}

// ------------------------------------------------------------------------------------ state ---

bool VoicesPage::CanDelete() const {
    return hasVoice_ && std::find(removable_.begin(), removable_.end(), active_) != removable_.end();
}

void VoicesPage::SetState(const UiState& state) {
    language_ = state.language;
    installed_.clear();
    removable_.clear();
    active_.clear();
    const auto it = state.voices.find(state.language);
    if (it != state.voices.end()) {
        installed_ = it->second.installed;
        removable_ = it->second.removable;
        active_ = it->second.active;
    }
    const auto activeIt = std::find(installed_.begin(), installed_.end(), active_);
    hasVoice_ = !installed_.empty() && activeIt != installed_.end();

    // The choice. The items are set again only when the list changed, so an open list is not disturbed.
    if (voice_->Items() != installed_) voice_->SetItems(installed_);
    voice_->SetSelected(hasVoice_ ? static_cast<int>(activeIt - installed_.begin()) : -1);
    voice_->SetEnabled(!installed_.empty());

    // No Voice: a note takes the place of the short rule.
    const bool none = installed_.empty();
    const std::string noteText =
        F(sid::kWindowVoiceNone, {{"language", LanguageNameOf(env_.interfaceLanguage, state.language)}});
    none_->SetText(noteText);
    if (none_->Visible() != none) none_->SetVisible(none);
    rule_->SetVisible(!none);
    MarkLayout();  // the note's height follows its text

    // Delete: only a Voice that is not shipped; the tooltip says why it cannot be used for one that is.
    const bool may = CanDelete();
    delete_->SetEnabled(may);
    const std::string deleteText = T(hasVoice_ && !may ? sid::kWindowVoiceDeleteShipped : sid::kWindowVoiceDelete);
    delete_->SetTooltip(deleteText);
    delete_->SetHelp(deleteText);
    save_->SetEnabled(hasVoice_);
    load_->SetEnabled(hasVoice_);

    for (ControlRow& r : rows_) {
        const auto cit = state.controls.find(r.name);
        const bool usable = hasVoice_ && cit != state.controls.end() && std::isfinite(cit->second.value);
        const std::string label = r.label->Text();
        if (!usable) {
            r.slider->Clear();
            r.slider->SetTooltip(std::string());
            r.slider->SetValueText(std::string());
            r.reset->SetVisible(false);
            continue;
        }
        const UiControl& c = cit->second;
        r.slider->SetRange({c.min, c.max, c.value, c.step, r.name == "speed"});  // Speed on a log scale: 1.0 in the middle
        const std::string shown = VoiceControlText(env_.interfaceLanguage, r.name, c.value);
        r.slider->SetTooltip(shown);
        r.slider->SetValueText(shown);
        // Reset shows only beside a value that is not the default.
        r.reset->SetVisible(std::isfinite(c.def) && c.value != c.def);
        const std::string resetText = F(sid::kWindowVoiceResetFor, {{"control", label}});
        r.reset->SetName(resetText);
        r.reset->SetTooltip(resetText);
    }
}

// ------------------------------------------------------------------------------------ sheets ---

DownloadSheet& VoicesPage::EnsureDownloads() {
    // Made on first use: the page is not in a window (no Screen) when it is built.
    if (!downloads_) downloads_ = std::make_unique<DownloadSheet>(*GetScreen(), env_, library_);
    return *downloads_;
}

ResultSheet& VoicesPage::EnsureResults() {
    if (!results_) results_ = std::make_unique<ResultSheet>(*GetScreen(), env_);
    return *results_;
}

void VoicesPage::OpenDownloadSheet() {
    if (!GetScreen()) return;
    library_.BeginSheet();  // a new sheet: loading, no old note, no old catalogue
    Emit(MakeMessage(PageMessageType::OpenCatalogue, "openCatalogue"));  // the host gets the list (ADR-0005: only now)
    EnsureDownloads().Open();
}

void VoicesPage::OpenDeleteSheet() {
    if (!CanDelete() || !GetScreen()) return;  // the button is disabled then; this is the keyboard's guard
    OpenDeleteVoiceSheet(*GetScreen(), env_, active_);
}

void VoicesPage::OnLibraryChange(VoiceLibraryState::Change change) {
    if (!GetScreen()) return;
    if (change == VoiceLibraryState::Change::Result) {
        ShowPendingResult();
        return;
    }
    EnsureDownloads().OnChange(change);
}

void VoicesPage::ShowPendingResult() {
    if (!GetScreen()) return;
    if (std::optional<ResultOutcome> r = library_.TakeResult()) EnsureResults().Show(*r);
}

}  // namespace nui

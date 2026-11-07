#include "native_ui/pages/frame.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "native_ui/display.h"
#include "native_ui/screen.h"
#include "native_ui/pages/layout.h"
#include "strings/string_ids.h"

namespace nui {

namespace {

// Lays `links` out left to right, `gap` apart, in a group whose box is as tall as the tallest link. The group
// starts at x0 (or is centred on the window when centreOnWindow) and its vertical centre is centreY. Links
// are positioned relative to the group.
void PlaceRow(Canvas& canvas, Panel& group, const std::vector<TextLink*>& links, float gap, float x0,
              bool centreOnWindow, float centreY) {
    std::vector<SizeF> sizes;
    float total = 0, maxH = 0;
    for (TextLink* l : links) {
        sizes.push_back(l->Measure(canvas, 10000));
        total += sizes.back().w;
        maxH = std::max(maxH, sizes.back().h);
    }
    if (!links.empty()) total += gap * static_cast<float>(links.size() - 1);
    if (centreOnWindow) x0 = (layout::kWindowW - total) / 2;
    group.SetBounds({x0, centreY - maxH / 2, total, maxH});
    float x = 0;
    for (size_t i = 0; i < links.size(); ++i) {
        links[i]->SetBounds({x, (maxH - sizes[i].h) / 2, sizes[i].w, sizes[i].h});
        x += sizes[i].w + gap;
    }
}

}  // namespace

Frame::Frame(const PageEnv& env) : env_(env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});

    // Painted first, so a page is over it (nothing overlaps today).
    PictureWidget* flute = Add<PictureWidget>("flute", PictureTint::Flute);
    flute->SetBounds(layout::kFlute);

    topNav_ = Add<Panel>();
    topNav_->SetName(T(strings::id::kWindowNavMain));
    AddLink(*topNav_, Page::Home, strings::id::kWindowNavHome, "home-05");
    AddLink(*topNav_, Page::Voices, strings::id::kWindowNavVoices, "user-profile-group");
    AddLink(*topNav_, Page::Settings, strings::id::kWindowNavSettings, "settings");
    for (Page p : {Page::Home, Page::Voices, Page::Settings}) {
        TextLink* l = links_[static_cast<int>(p)];
        l->SetFontSize(layout::kTopNavFont);
        l->SetIconSize(layout::kTopNavIcon);
        l->SetGap(layout::kTopNavIconGap);
    }
    DottedRule* ruleTop = Add<DottedRule>();
    ruleTop->SetBounds(layout::kRuleTop);

    footNav_ = Add<Panel>();
    footNav_->SetName(T(strings::id::kWindowNavMore));
    AddLink(*footNav_, Page::Report, strings::id::kWindowNavReport, "Spider");
    AddLink(*footNav_, Page::Help, strings::id::kWindowNavHelp, "book-04");
    AddLink(*footNav_, Page::About, strings::id::kWindowNavAbout, "heart");
    for (Page p : {Page::Report, Page::Help, Page::About}) {
        TextLink* l = links_[static_cast<int>(p)];
        l->SetFontSize(layout::kFootNavFont);
        l->SetIconSize(layout::kFootNavIcon);
        l->SetGap(layout::kFootNavIconGap);
    }
    DottedRule* ruleBottom = Add<DottedRule>();
    ruleBottom->SetBounds(layout::kRuleBottom);

    // The Master volume: the speaker icon and a compact slider. Disabled until the first state.
    volumeGroup_ = Add<Panel>();
    volumeGroup_->SetBounds({layout::kVolumeIconX, layout::kVolumeBoxTop,
                             layout::kVolumeSliderX + layout::kVolumeSliderW - layout::kVolumeIconX,
                             layout::kVolumeBoxH});
    IconWidget* icon = volumeGroup_->Add<IconWidget>("volume-05", ColourRole::Accent);
    icon->SetBounds({0, (layout::kVolumeBoxH - layout::kVolumeIconSize) / 2, layout::kVolumeIconSize,
                     layout::kVolumeIconSize});
    volume_ = volumeGroup_->Add<Slider>();
    volume_->SetCompact(true);
    volume_->SetBounds({layout::kVolumeSliderX - layout::kVolumeIconX, 0, layout::kVolumeSliderW,
                        layout::kVolumeBoxH});
    volume_->SetName(T(strings::id::kWindowVolume));
    volume_->Clear();
    // A drag or key asks for a value on the grid of the state; the host answers with a new state.
    volume_->SetOnValue([this](double v) {
        PageMessage m = MakeMessage(PageMessageType::SetMasterVolume, "setMasterVolume");
        m.value = v;
        if (Screen* s = GetScreen()) s->Emit(m);
    });

    SetCurrent(Page::Home);
}

TextLink* Frame::AddLink(Panel& group, Page page, const char* textId, const char* icon) {
    TextLink* link = group.Add<TextLink>(T(textId), icon);
    links_[static_cast<int>(page)] = link;
    // The page changes locally. The link is not inside the page that goes, but the Screen runs the
    // switch after the event anyway, like every callback that removes widgets.
    link->SetOnActivate([this, page] {
        if (!onNavigate_) return;
        if (Screen* s = GetScreen()) s->Defer([this, page] { if (onNavigate_) onNavigate_(page); });
        else onNavigate_(page);
    });
    return link;
}

void Frame::SetCurrent(Page page) {
    for (int i = 0; i < 6; ++i) {
        if (links_[i]) links_[i]->SetCurrent(i == static_cast<int>(page));
    }
}

void Frame::SetState(const UiState& state) {
    const UiControl& mv = state.masterVolume;
    volume_->SetRange({mv.min, mv.max, mv.value, mv.step, false});
    // "50%": the tooltip and what a screen reader says as the value.
    const std::string text = strings::Format(env_.interfaceLanguage, strings::id::kWindowVolumeValue,
                                             {{"value", std::to_string(std::lround(mv.value))}});
    volume_->SetValueText(text);
    volume_->SetTooltip(text);
}

void Frame::Relayout(Canvas& canvas) {
    PlaceRow(canvas, *topNav_, {links_[0], links_[1], links_[2]}, layout::kTopNavGap, 0, true,
             layout::kTopNavCentreY);
    PlaceRow(canvas, *footNav_, {links_[3], links_[4], links_[5]}, layout::kFootNavGap, layout::kFootNavX, false,
             layout::kFootNavCentreY);
}

}  // namespace nui

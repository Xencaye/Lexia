#pragma once
// The frame of the Settings window (K.2, ADR-0009): what is on every page. Pure, Win32-free.
//
//   the top links (home, voices, settings), a dotted rule under them,
//   the bottom-left links (report, help, about), a dotted rule over them,
//   the Master volume (a speaker icon and a slider, Settings::speechVolume),
//   the flute engraving.
//
// Tab order is the order of the children: top links, bottom links, Master volume, then the page (the
// SettingsUi puts its page host after the frame). The frame draws the links with the current page in bold;
// a click on a link calls onNavigate(page) (the SettingsUi switches the page and nothing is sent to the host:
// which page shows is the one thing the page owns, as app.js did).
#include <functional>

#include "native_ui/controls.h"
#include "native_ui/pages/page.h"
#include "native_ui/slider.h"

namespace nui {

class Frame : public Panel {
public:
    explicit Frame(const PageEnv& env);

    void SetCurrent(Page page);  // the bold link, and aria-current for accessibility
    void SetOnNavigate(std::function<void(Page)> fn) { onNavigate_ = std::move(fn); }
    // The Master volume from the state. Until the first state the slider is disabled.
    void SetState(const UiState& state);

    // For the tests and the picture tool: the widgets the frame owns.
    TextLink* LinkFor(Page page) const { return links_[static_cast<int>(page)]; }
    Slider* Volume() const { return volume_; }
    Widget* VolumeGroup() const { return volumeGroup_; }

protected:
    void Relayout(Canvas& canvas) override;

private:
    std::string T(const char* id) const { return strings::Text(env_.interfaceLanguage, id); }
    TextLink* AddLink(Panel& group, Page page, const char* textId, const char* icon);

    const PageEnv env_;
    Panel* topNav_ = nullptr;
    Panel* footNav_ = nullptr;
    Panel* volumeGroup_ = nullptr;
    TextLink* links_[6] = {};
    Slider* volume_ = nullptr;
    std::function<void(Page)> onNavigate_;
};

}  // namespace nui

#pragma once
// The Settings page. K.2 only gives it its place: an empty page. K.4 fills it: the headings, the key list with
// its chips, the key card (wine, play button), the capture hint, and the five switches (Start minimised is
// disabled unless Run at startup is on). Key capture comes from the window's WM_KEYDOWN through the Screen
// (a KeyChip that captures gets every key).
#include "native_ui/pages/page.h"

namespace nui {

class SettingsPage : public PageView {
public:
    explicit SettingsPage(const PageEnv& env);
    void SetState(const UiState& state) override;  // TODO(K.4): keys and switches from the state
};

}  // namespace nui

#pragma once
// The Voices page. K.2 only gives it its place: an empty page, so the frame, the navigation and the window can
// be built and tested first. K.3 fills it: the Voice row (select, download / add / delete), the Speed / Pitch /
// Volume sliders with their reset buttons, the otter, the preset links, the "no voice" note, and the three
// sheets (Delete voice, Voice download, Result), with typed events from VoiceTab instead of JSON.
#include "native_ui/pages/page.h"

namespace nui {

class VoicesPage : public PageView {
public:
    explicit VoicesPage(const PageEnv& env);
    void SetState(const UiState& state) override;  // TODO(K.3): the Voice row and the controls from state.voices / state.controls
};

}  // namespace nui

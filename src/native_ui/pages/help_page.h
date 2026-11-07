#pragma once
// The Help page (K.2): a centred title and one line ("Coming soon!"). Ported from old ui/css/pages.css `.help`.
#include "native_ui/pages/page.h"

namespace nui {

class HelpPage : public PageView {
public:
    explicit HelpPage(const PageEnv& env);
    void SetState(const UiState&) override {}  // nothing on it depends on a setting
};

}  // namespace nui

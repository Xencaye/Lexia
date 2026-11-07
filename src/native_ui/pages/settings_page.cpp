#include "native_ui/pages/settings_page.h"

#include "native_ui/pages/layout.h"
#include "strings/string_ids.h"

namespace nui {

SettingsPage::SettingsPage(const PageEnv& env) : PageView(Page::Settings, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavSettings));
    // TODO(K.4): build the Settings page here (positions go into layout.h next to the others).
}

void SettingsPage::SetState(const UiState&) {
    // TODO(K.4): show the keys and the switches from the state.
}

}  // namespace nui

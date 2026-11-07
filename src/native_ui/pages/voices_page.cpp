#include "native_ui/pages/voices_page.h"

#include "native_ui/pages/layout.h"
#include "strings/string_ids.h"

namespace nui {

VoicesPage::VoicesPage(const PageEnv& env) : PageView(Page::Voices, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavVoices));
    // TODO(K.3): build the Voice page here (positions go into layout.h next to the others).
}

void VoicesPage::SetState(const UiState&) {
    // TODO(K.3): show the Voice choice and the three controls from the state.
}

}  // namespace nui

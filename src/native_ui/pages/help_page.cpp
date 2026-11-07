#include "native_ui/pages/help_page.h"

#include "native_ui/pages/layout.h"
#include "strings/string_ids.h"

namespace nui {

HelpPage::HelpPage(const PageEnv& env) : PageView(Page::Help, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavHelp));

    // `.help .big`: 24 bold, line height 30, centred over the whole window width.
    Label* title = Add<Label>(T(strings::id::kWindowHelpTitle));
    title->SetStyle(layout::kBigFont, FontWeight::Bold);
    title->SetAlign(TextAlign::Centre);
    title->SetLetterSpacing(layout::kBigLetterSpacing);
    title->SetBounds({0, layout::kHelpTitleY, layout::kWindowW, layout::kBigLine});

    Label* text = Add<Label>(T(strings::id::kWindowHelpSoon));
    text->SetAlign(TextAlign::Centre);
    text->SetBounds({0, layout::kHelpTextY, layout::kWindowW, layout::kBodyLine});
}

}  // namespace nui

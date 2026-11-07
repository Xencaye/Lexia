#pragma once
// The About page (K.2): the wolf, "Lexia | ver. x", the intro with two bold words, the text that goes round
// the wolf, the itch.io and GitHub buttons, and the licence and notices lines. Ported from `.wolf`
// `.about-*` in ui/css/pages.css and ui/js/pages.js. The version comes from UiState.version.
#include <string>

#include "native_ui/controls.h"
#include "native_ui/pages/page.h"

namespace nui {

class AboutPage : public PageView {
public:
    explicit AboutPage(const PageEnv& env);
    void SetState(const UiState& state) override;

    IconButton* ItchButton() const { return itch_; }
    IconButton* GithubButton() const { return github_; }
    TextLink* NoticesLink() const { return notices_; }

protected:
    void Relayout(Canvas& canvas) override;

private:
    IconButton* AddIconLink(const char* icon, const char* nameId, LinkId id);

    Label* name_ = nullptr;           // "Lexia" in bold accent
    Label* version_ = nullptr;        // "| ver. 0.5.0"
    Label* legal_ = nullptr;          // the licence line
    Label* noticesBefore_ = nullptr;  // "View third-party notices "
    TextLink* notices_ = nullptr;     // "here"
    Label* noticesAfter_ = nullptr;   // "."
    IconButton* itch_ = nullptr;
    IconButton* github_ = nullptr;
    std::string versionText_;
    std::string noticesTemplateBefore_, noticesTemplateAfter_;
};

}  // namespace nui

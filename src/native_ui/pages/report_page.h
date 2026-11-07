#pragma once
// The Report page (K.2): the dog, the invitation and three links (mail, Discord, GitHub). A link sends
// openLink with a LinkId; the host owns the URL (ADR-0005, `LinkUrl`). Ported from `.dog` `.big`
// `.report-text` `.links` in old ui/css/pages.css and old ui/js/pages.js.
#include "native_ui/controls.h"
#include "native_ui/pages/page.h"

namespace nui {

class ReportPage : public PageView {
public:
    explicit ReportPage(const PageEnv& env);
    void SetState(const UiState&) override {}
    TextLink* MailLink() const { return mail_; }
    TextLink* DiscordLink() const { return discord_; }
    TextLink* GithubLink() const { return github_; }

protected:
    void Relayout(Canvas& canvas) override;

private:
    TextLink* AddLink(const char* textId, const char* icon, LinkId id);
    TextLink *mail_ = nullptr, *discord_ = nullptr, *github_ = nullptr;
};

}  // namespace nui

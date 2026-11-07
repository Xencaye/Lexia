#include "native_ui/settings_ui.h"

#include "native_ui/pages/about_page.h"
#include "native_ui/pages/help_page.h"
#include "native_ui/pages/home_page.h"
#include "native_ui/pages/layout.h"
#include "native_ui/pages/report_page.h"
#include "native_ui/pages/settings_page.h"
#include "native_ui/pages/voices_page.h"

namespace nui {

SettingsUi::SettingsUi(Screen& screen, std::string interfaceLanguage) : screen_(screen) {
    env_.interfaceLanguage = std::move(interfaceLanguage);
    screen_.SetSize({layout::kWindowW, layout::kWindowH});

    // The frame first, then the page host: Tab goes through the frame's links and the Master volume, then
    // the page, as the old page's tab order did.
    Widget& root = screen_.Root();
    root.SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    auto frame = std::make_unique<Frame>(env_);
    frame_ = frame.get();
    root.AddChild(std::move(frame));
    auto host = std::make_unique<Panel>();
    host_ = host.get();
    host_->SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    root.AddChild(std::move(host));

    // The Voice library tells the page what changed while the Voices page shows. Another page has nothing to
    // draw: the state waits in library_ (a download goes on, a result is kept) until the Voices page is built.
    library_.SetOnChange([this](VoiceLibraryState::Change change) {
        if (page_ == Page::Voices) static_cast<VoicesPage*>(view_)->OnLibraryChange(change);
    });

    frame_->SetOnNavigate([this](Page page) { ShowPage(page); });

    auto view = MakePage(Page::Home);
    view_ = view.get();
    host_->AddChild(std::move(view));
    frame_->SetCurrent(Page::Home);
}

SettingsUi::~SettingsUi() = default;

std::unique_ptr<PageView> SettingsUi::MakePage(Page page) {
    switch (page) {
        case Page::Home: return std::make_unique<HomePage>(env_);
        case Page::Voices: return std::make_unique<VoicesPage>(env_, library_);
        case Page::Settings: return std::make_unique<SettingsPage>(env_);
        case Page::Report: return std::make_unique<ReportPage>(env_);
        case Page::Help: return std::make_unique<HelpPage>(env_);
        case Page::About: return std::make_unique<AboutPage>(env_);
    }
    return std::make_unique<HomePage>(env_);
}

void SettingsUi::ShowPage(Page page) {
    if (page == page_) return;
    // Not before the first state is drawn and not animated: a page that was just built must not fade in from
    // its idle look (Screen rule). The old page goes (the Screen forgets focus and hover that pointed into it).
    screen_.SkipAnimations();
    // A sheet that is open belongs to the page that opened it (its buttons call back into it): close them
    // all before the page goes. (A page switch with a sheet open comes from outside, e.g. Help from the
    // Control bar; a click on a frame link cannot reach behind a sheet.)
    for (size_t n = screen_.SheetCount(); n > 0; --n) screen_.CloseTopSheet();
    host_->ClearChildren();
    auto view = MakePage(page);
    view_ = view.get();
    host_->AddChild(std::move(view));
    page_ = page;
    frame_->SetCurrent(page);
    if (hasState_) view_->SetState(state_);
    // A result that came while another page showed (an Add from file that ended later) opens now.
    if (page == Page::Voices) static_cast<VoicesPage*>(view_)->ShowPendingResult();
    screen_.Invalidate();
}

void SettingsUi::ShowPage(Page page, const ShowOptions& options) {
    ShowPage(page);  // builds the page when it is not the one that shows
    view_->OnShow(options);
    screen_.Invalidate();
}

void SettingsUi::SetState(const UiState& state) {
    state_ = state;
    hasState_ = true;
    SetDark(state.dark);
    frame_->SetState(state_);
    view_->SetState(state_);
    screen_.Invalidate();
}

void SettingsUi::SetDark(bool dark) {
    if (dark == dark_) return;
    dark_ = dark;
    screen_.Invalidate();
}

}  // namespace nui

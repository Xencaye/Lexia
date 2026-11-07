#pragma once
// The pieces every page of the native Settings window is built from (K.2, ADR-0009). Pure, Win32-free.
//
//   Page       the six pages: Home, Voices, Settings (the top links), Report, Help, About (the bottom links).
//   Panel      a widget that only holds others (no look of its own). It is not a hit target itself (a press
//              on empty paper is nobody's), and it lays its children out once with a Canvas to measure
//              text: Relayout(canvas) runs before the first paint and again after MarkLayout().
//   PageView   a Panel that is one page. The window's SettingsUi makes one at a time (MakePage) and feeds it
//              every UiState (SetState). A page keeps no copy of a setting: it draws the last state.
//
// How a later task adds to a page (the frame does not change):
//   K.3  VoicesPage (voices_page.*): its widgets and sheets are built in the constructor / Relayout; Voice
//        library events arrive as typed calls on the page (SettingsUi::Current() gives it, K.3 adds the
//        accessor to its own type), and a user action is a PageMessage through Screen::Emit.
//   K.4  SettingsPage (settings_page.*): the same. K.4 also adds a ShowPage(Page, ShowOptions) overload to
//        SettingsUi and a PageView::OnShow(const ShowOptions&) hook (select an action, start a capture) that
//        it calls after the page is built; the one-argument ShowPage stays.
#include <string>
#include <string_view>

#include "native_ui/display.h"
#include "native_ui/widget.h"
#include "strings/strings.h"
#include "ui_host.h"

namespace nui {

enum class Page { Home, Voices, Settings, Report, Help, About };

// "home" | "voices" | "settings" | "report" | "help" | "about" (the names App::OpenSettingsWindow takes).
const char* PageName(Page page);
// The page of a name; false (and `out` untouched) for a name that is none of them.
bool ParsePageName(std::string_view name, Page& out);

// What a page needs from outside the tree: the words' language.
struct PageEnv {
    std::string interfaceLanguage = "en";
};

// A widget that holds others and does not draw or take the pointer itself.
class Panel : public Widget {
public:
    Widget* HitTest(PointF p) override;  // a child, or nothing (not the panel)
    void PrepareLayout(Canvas& canvas) override;

protected:
    // Places the children. Called before painting when MarkLayout() was called (and at the first paint),
    // with the canvas that measures text. Default: nothing to place.
    virtual void Relayout(Canvas& canvas);
    void MarkLayout() { layoutDirty_ = true; Invalidate(); }

private:
    bool layoutDirty_ = true;
};

class PageView : public Panel {
public:
    PageView(Page kind, const PageEnv& env);
    Page Kind() const { return kind_; }
    // Shows what `state` says. Called once when the page is built (if a state has come) and after every state.
    virtual void SetState(const UiState& state) = 0;

protected:
    // The texts of the page: by string id, in the window's Interface language.
    std::string T(const char* id) const { return strings::Text(env_.interfaceLanguage, id); }
    std::string F(const char* id, const strings::Args& args) const {
        return strings::Format(env_.interfaceLanguage, id, args);
    }
    // Sends a message to the host (Screen::Emit); dropped when the page is not in a window yet.
    void Emit(const PageMessage& message) const;

    const PageEnv env_;

private:
    Page kind_;
};

// A 1 DIP dotted rule across the top of its bounds (the design's `border-top: 1px dotted`), in the `line` colour.
class DottedRule : public Widget {
public:
    void Paint(Canvas& canvas, const Palette& palette) override;
};

// A message of a type and a log name ("setOn"), as the page sends it.
PageMessage MakeMessage(PageMessageType type, const char* typeText);

}  // namespace nui

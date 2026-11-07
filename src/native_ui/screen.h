#pragma once
// Screen: the root of the native Settings window's widget tree (K1.b, ADR-0009). It owns the tree and
// everything that belongs to the window as a whole:
//   - the mouse: hover, press, capture while dragging, leave, the wheel (which goes up to a ScrollArea);
//   - the keys: to the focused widget, Tab / Shift+Tab in tree order, Esc closes the top sheet;
//   - the layers on top of the page: modal Sheets (a scrim behind each, focus moves in and returns to
//     the opener) and one popup (the list of an open Select);
//   - the tooltip request (the hover delay counts in time the caller passes in), the cursor hint,
//     the "needs repaint" flag, the focus-visible flag and the transitions' clock.
// There is no clock and no window in here: the window calls Tick(now) with seconds and passes events.
// Pure and Win32-free, so tests/native_ui_*_test.cpp drive it with a FakeCanvas.
#include "native_ui/widget.h"
#include "ui_host.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nui {

class Sheet;

// "Show this tooltip": the text and the rectangle of the widget it belongs to (the window places it
// below or above the anchor).
struct TooltipRequest {
    std::string text;
    RectF anchor;
};

constexpr double kTooltipDelaySeconds = 0.5;

class Screen {
public:
    Screen();
    ~Screen();
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    // The page. Its bounds are the whole client area; the page fills it with widgets.
    Widget& Root() { return *root_; }
    void SetSize(SizeF size);  // the client area in DIPs (800 x 600)
    SizeF Size() const { return size_; }

    // ---- time and drawing
    // The caller passes the time (seconds) before it passes events and before it paints. Transitions
    // and the tooltip delay use it. A Tick that finds a transition running asks for a repaint.
    void Tick(double now);
    double Now() const { return now_; }
    bool NeedsTick() const;  // a transition runs or a tooltip is counting down: keep a timer going
    void SetReducedMotion(bool reduced);
    bool ReducedMotion() const { return reduced_; }
    // Transitions are on once the first state has been drawn (a page that was just built must not
    // slide in) and while reduced motion is off. SkipAnimations() starts the "not yet drawn" phase
    // again (a page switch).
    bool AnimationsOn() const { return drawn_ && !reduced_; }
    void SkipAnimations() { drawn_ = false; }
    void NoteAnimationUntil(double endTime);  // widgets: "tick until this time"

    void Invalidate() { dirty_ = true; }
    bool NeedsRepaint() const { return dirty_; }
    // Lays the layers out (needs the canvas to measure text); Paint calls it, tests may call it alone.
    void Layout(Canvas& canvas);
    void Paint(Canvas& canvas, const Palette& palette);

    // ---- input. Both return true when the event was used.
    bool HandleMouse(const MouseEvent& ev);
    bool HandleKey(const KeyEvent& ev);
    CursorHint Cursor() const;
    std::optional<TooltipRequest> Tooltip() const;  // after the hover delay; none when nothing to show

    // ---- focus
    Widget* Focused() const { return focused_; }
    void SetFocus(Widget* w);  // null clears. Keeps the focus-visible flag as it is.
    // Focus-visible, like CSS :focus-visible: the ring shows only after the keyboard was used. A mouse
    // press turns it off, any key turns it on.
    bool FocusVisible() const { return focusVisible_; }
    // The widgets Tab visits now, in order (the top layer's, else the page's).
    std::vector<Widget*> FocusOrder() const;

    // ---- layers
    Sheet* OpenSheet(std::unique_ptr<Sheet> sheet);
    void CloseSheet(Sheet* sheet);  // runs its onClose; focus goes back to what had it. Deferred.
    void CloseTopSheet();
    Sheet* TopSheet() const;
    size_t SheetCount() const;
    // A popup (the open list of a Select): not modal, closed by Esc, Tab, a press outside, or
    // ClosePopup. `owner` is told with OnPopupClosed. Opening one closes the one that is open.
    Widget* OpenPopup(std::unique_ptr<Widget> popup, Widget* owner);
    void ClosePopup();
    Widget* Popup() const;

    // ---- commands out
    // Page code builds PageMessages (src/ui_host.h) in its widget callbacks and sends them with
    // Emit; the window sets the sink (App::HandlePageMessage). Without a sink they are dropped.
    void SetMessageSink(std::function<void(const PageMessage&)> sink) { sink_ = std::move(sink); }
    void Emit(const PageMessage& message) const {
        if (sink_) sink_(message);
    }

    // Runs `fn` after the event that is being handled, so a callback can remove widgets (the sheet it
    // sits in, its own page) without pulling them from under its own feet. Outside an event: at once.
    void Defer(std::function<void()> fn);

    // Widget removal: drop every pointer that points into `subtree`.
    void Forget(Widget* subtree);

private:
    struct Layer {
        Widget* widget = nullptr;  // owned by overlay_
        bool modal = false;        // a sheet
        Widget* opener = nullptr;  // the focus before it opened (sheet)
        Widget* owner = nullptr;   // the Select of a popup
    };

    static bool Inside(const Widget* w, const Widget* tree);
    Widget* ScopeRoot() const;  // the top layer's widget, or the page
    Widget* HitTarget(PointF p, bool& outsideModal) const;
    void SetHover(Widget* w);
    void SetFocusVisible(bool v);
    void MoveFocus(bool backwards);
    void RunDeferred();
    void RemoveLayer(size_t index);
    void ClosePopupNow();  // ClosePopup without the deferral, for the Screen's own event handling
    void CollectFocus(Widget* w, std::vector<Widget*>& out) const;
    bool DispatchWheel(Widget* target, const MouseEvent& ev);

    std::unique_ptr<Widget> root_;
    std::unique_ptr<Widget> overlay_;  // the parent of the layers' widgets
    std::vector<Layer> layers_;
    SizeF size_{800, 600};
    double now_ = 0;
    double animUntil_ = 0;
    bool animating_ = false;  // a transition was running at the last Tick
    bool reduced_ = false;
    bool drawn_ = false;
    bool dirty_ = true;
    bool focusVisible_ = false;
    Widget* hover_ = nullptr;
    double hoverSince_ = 0;
    bool tooltipHidden_ = false;  // a press hides the tooltip until the pointer moves to another widget
    Widget* focused_ = nullptr;
    Widget* capture_ = nullptr;
    int dispatchDepth_ = 0;
    std::vector<std::function<void()>> deferred_;
    std::function<void(const PageMessage&)> sink_;
    double tooltipDelay_ = kTooltipDelaySeconds;

    friend class Widget;
};

}  // namespace nui

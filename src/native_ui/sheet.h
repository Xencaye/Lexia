#pragma once
// Sheet: a modal card over the page (K1.b; ui/css/components.css `.sheet`, the old <dialog>). The Voice
// download, the result sheet and Delete voice are Sheets. Screen::OpenSheet puts it on the layer stack:
// a scrim is drawn behind it, the focus moves into it and goes back to the opener on close, Tab stays
// inside, Esc closes the top one. A press on the scrim does nothing (as with the old dialog).
//
// Layout (DIPs): width 420 (SetWidth changes it), padding 24, 1 border line, radius 6, card fill. The
// title is 22 bold with line height 1.2, 14 below it. The body widgets are stacked with 6 between
// them, each as wide as the inside of the card and as tall as its Measure says. The action buttons
// follow 22 below, in a row with 12 between (they wrap to a new row when the row is full). The sheet
// is centred in the Screen's size, and lays itself out again at every Paint (PrepareLayout), so a body
// that changes needs no extra call.
#include "native_ui/controls.h"
#include "native_ui/widget.h"

#include <functional>
#include <string>

namespace nui {

constexpr float kSheetWidth = 420, kSheetPad = 24, kSheetTitleSize = 22, kSheetTitleGap = 14,
                kSheetLineGap = 6, kSheetActionsTop = 22, kSheetActionGap = 12;

class Sheet : public Widget {
public:
    explicit Sheet(float width = kSheetWidth);

    void SetWidth(float w) { width_ = w; }
    void SetTitle(std::string title);

    // A line of the body: it is laid out by the sheet (bounds are replaced).
    Widget* AddBody(std::unique_ptr<Widget> w);
    template <class T, class... Args>
    T* AddBodyOf(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        AddBody(std::move(owned));
        return raw;
    }
    // An action button, after the body. Hidden buttons take no room (the old `.btn[hidden]`).
    Button* AddAction(std::unique_ptr<Button> b);

    // Runs when the sheet closes by Esc, by Close() or by Screen::CloseSheet (not when the whole
    // Screen goes away).
    void SetOnClose(std::function<void()> fn) { onClose_ = std::move(fn); }
    void NotifyClosed() {
        if (onClose_) onClose_();
    }
    void Close();  // = Screen::CloseSheet(this)

    // The widget that gets the focus when the sheet opens; default the first focusable one.
    void SetInitialFocus(Widget* w) { initialFocus_ = w; }
    Widget* InitialFocus() const { return initialFocus_; }

    void PrepareLayout(Canvas& canvas) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    float width_;
    std::string title_;
    RectF titleBox_;  // where the title is drawn (window DIPs, set by the layout)
    std::vector<Widget*> body_;
    std::vector<Button*> actions_;
    Widget* initialFocus_ = nullptr;
    std::function<void()> onClose_;
};

}  // namespace nui

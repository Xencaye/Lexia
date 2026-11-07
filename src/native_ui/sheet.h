#pragma once
// Sheet: a modal card over the page (K1.b; old ui/css/components.css `.sheet`, the old <dialog>). The Voice
// download, the result sheet and Delete voice are Sheets. Screen::OpenSheet puts it on the layer stack:
// a scrim is drawn behind it, the focus moves into it and goes back to the opener on close, Tab stays
// inside, Esc closes the top one. A press on the scrim does nothing (as with the old dialog).
//
// Layout (DIPs): width 420 (SetWidth changes it), padding 24 inside a 1 border line (content starts 25 in), radius 6, card fill. The
// title is 22 bold with line height 1.2, 14 below it (SetTitleGap: the wide download sheet has 6); a title
// wider than the card wraps at its spaces (a long Voice name in "Delete {voice}?"). The body widgets are
// stacked with 6 between them (AddBody can say another gap after one), each as wide as the inside of the card
// and as tall as its Measure says. The action buttons follow 22 below, in a row with 12 between (they wrap
// to a new row when the row is full). The sheet is centred in the Screen's size, and lays itself out again
// at every Paint (PrepareLayout), so a body that changes needs no extra call.
//
// A sheet with a list (the download sheet) has a maximum height (SetMaxHeight, the old `max-height: 520px`)
// and one flexible body widget (SetFlexBody, the old `flex: 1; min-height: 96px`): when the sheet would be
// taller than the maximum, the flexible one is made shorter, down to its minimum, and scrolls.
#include "native_ui/controls.h"
#include "native_ui/text_layout.h"
#include "native_ui/widget.h"

#include <functional>
#include <string>

namespace nui {

constexpr float kSheetWidth = 420, kSheetPad = 24, kSheetTitleSize = 22, kSheetTitleGap = 14,
                kSheetLineGap = 6, kSheetActionsTop = 22, kSheetActionGap = 12, kSheetFlexMin = 96;

class Sheet : public Widget {
public:
    explicit Sheet(float width = kSheetWidth);

    void SetWidth(float w) { width_ = w; }
    void SetTitle(std::string title);
    void SetTitleGap(float gap) { titleGap_ = gap; }
    // The most the sheet is tall (0 = no limit). Needs a flexible body to have an effect.
    void SetMaxHeight(float h) { maxHeight_ = h; }
    // The body widget that gives way when the sheet is too tall (a body of this sheet).
    void SetFlexBody(Widget* w) { flexBody_ = w; }

    // A line of the body: it is laid out by the sheet (bounds are replaced). `gapAfter` is the space under it.
    Widget* AddBody(std::unique_ptr<Widget> w, float gapAfter = kSheetLineGap);
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
    struct BodyLine {
        Widget* widget = nullptr;
        float gapAfter = kSheetLineGap;
    };
    float width_;
    float titleGap_ = kSheetTitleGap;
    float maxHeight_ = 0;
    std::string title_;
    std::vector<WrappedLine> titleLines_;  // the title as wrapped at the last layout
    RectF titleBox_;  // where the title is drawn (window DIPs, set by the layout): all its lines
    float titleLineH_ = 0;
    std::vector<BodyLine> body_;
    std::vector<Button*> actions_;
    Widget* initialFocus_ = nullptr;
    Widget* flexBody_ = nullptr;
    std::function<void()> onClose_;
};

}  // namespace nui

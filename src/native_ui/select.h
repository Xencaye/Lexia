#pragma once
// Select: a drop-down of text choices (the Voice choice, the theme). 28 tall, padding 0 30 0 9, a 1 DIP
// `line` border, radius 1, card fill, 16 px ink text and a 16 px ink chevron 9 from the right (K1.b;
// old ui/css/components.css `.select`). Disabled: opacity 0.7, not focusable.
//
// It opens an in-window popup list under it (or over it when there is no room below), as a layer of
// the Screen. Closed: Down / Up / Enter / Space (or a press) open it. Open: Down / Up / Home / End move
// the highlight, Enter / Space choose it, Esc, Tab or a press outside close it; the mouse moves the
// highlight and a release on a row chooses it. A choice calls onChoose(index) and closes the list; the
// select keeps showing the old choice until the page calls SetSelected from the next state.
#include "native_ui/widget.h"

#include <functional>
#include <string>
#include <vector>

namespace nui {

constexpr float kSelectH = 28, kSelectRowH = 28;

class Select : public Widget {
public:
    Select();
    void SetItems(std::vector<std::string> items);
    const std::vector<std::string>& Items() const { return items_; }
    // One tooltip per item, shown on its row of the open list (the Voice choice: the file name behind the
    // short name). Fewer tips than items, or an empty one, give a row without a tooltip.
    void SetItemTooltips(std::vector<std::string> tips) { itemTips_ = std::move(tips); }
    const std::vector<std::string>& ItemTooltips() const { return itemTips_; }
    void SetSelected(int index);  // -1 = none
    int Selected() const { return selected_; }
    void SetOnChoose(std::function<void(int)> fn) { onChoose_ = std::move(fn); }
    bool IsOpen() const { return open_; }
    void Open();
    // Called by the open list when a row is chosen.
    void Choose(int index);

    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    bool OnMouse(const MouseEvent& ev) override;
    bool OnKey(const KeyEvent& ev) override;
    void OnPopupClosed() override;
    CursorHint Cursor() const override { return enabled_ ? CursorHint::Hand : CursorHint::Arrow; }
    AccessInfo Access() const override;
    // UI Automation ExpandCollapse (K.5): opens or closes the list as Enter or Esc would.
    bool AccExpand(bool expand) override;

private:
    std::vector<std::string> items_;
    std::vector<std::string> itemTips_;  // parallel to items_; may be shorter
    int selected_ = -1;
    bool open_ = false;
    std::function<void(int)> onChoose_;
};

}  // namespace nui

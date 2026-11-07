#include "native_ui/select.h"

#include "native_ui/screen.h"

#include <algorithm>

namespace nui {

namespace {

constexpr float kPadLeft = 9, kPadRight = 30, kBorder = 1, kRadius = 1, kChevron = 16, kChevronRight = 9;

class SelectList;

// One row of the open list. It forwards the pointer to the list: the highlight follows the pointer and
// a release on the row chooses it.
class ListRow : public Widget {
public:
    ListRow(int index, std::string text) : index_(index), text_(std::move(text)) {
        SetRole(Role::ListItem);
        SetName(text_);
    }
    int Index() const { return index_; }
    void Paint(Canvas& canvas, const Palette& palette) override;
    bool OnMouse(const MouseEvent& ev) override;
    AccessInfo Access() const override;
    bool AccSelect() override;
    CursorHint Cursor() const override { return CursorHint::Hand; }

private:
    SelectList* List() const;
    int index_;
    std::string text_;
};

// The popup: a card with a line border and one ListRow per item. It owns the highlight.
class SelectList : public Widget {
public:
    SelectList(Select* select, const std::vector<std::string>& items, int selected)
        : select_(select), selected_(selected), highlight_(std::max(0, selected)) {
        SetRole(Role::List);
        int i = 0;
        for (const std::string& t : items) Add<ListRow>(i++, t);
    }
    int Highlight() const { return highlight_; }
    int SelectedIndex() const { return selected_; }
    void SetHighlight(int i) {
        const int n = static_cast<int>(Children().size());
        i = std::max(0, std::min(n - 1, i));
        if (i != highlight_) {
            highlight_ = i;
            Invalidate();
        }
    }
    void Choose(int i) {
        select_->Choose(i);
        // The list closes after the event that chose: this is called from the list's own handlers.
        if (Screen* s = GetScreen()) s->ClosePopup();
    }
    // Rows are laid out inside the border, one under the other.
    void LayoutRows() {
        float y = kBorder;
        for (auto& c : Children()) {
            c->SetBounds({kBorder, y, Bounds().w - 2 * kBorder, kSelectRowH});
            y += kSelectRowH;
        }
    }
    void Paint(Canvas& canvas, const Palette& p) override {
        const RectF r = Abs();
        canvas.FillRect(r, p.card, kRadius);
        canvas.StrokeRect({r.x + kBorder / 2, r.y + kBorder / 2, r.w - kBorder, r.h - kBorder}, p.line,
                          kBorder, kRadius);
    }
    bool OnKey(const KeyEvent& ev) override {
        const int n = static_cast<int>(Children().size());
        switch (ev.key) {
            case Key::Down: SetHighlight(highlight_ + 1); return true;
            case Key::Up: SetHighlight(highlight_ - 1); return true;
            case Key::Home: SetHighlight(0); return true;
            case Key::End: SetHighlight(n - 1); return true;
            case Key::Enter:
            case Key::Space:
                if (n > 0) Choose(highlight_);
                return true;
            default: return false;  // Esc and Tab are the Screen's: they close the list
        }
    }
    AccessInfo Access() const override {
        AccessInfo a = BaseAccess();
        a.role = Role::List;
        return a;
    }

private:
    Select* select_;
    int selected_;
    int highlight_;
};

SelectList* ListRow::List() const { return static_cast<SelectList*>(Parent()); }

void ListRow::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    SelectList* list = List();
    const bool highlighted = list && list->Highlight() == index_;
    const bool current = list && list->SelectedIndex() == index_;
    if (highlighted) canvas.FillRect(r, p.line);
    TextStyle st;
    st.weight = current ? FontWeight::Bold : FontWeight::Regular;  // the current choice stands out
    canvas.Text({r.x + kPadLeft - kBorder, r.y, r.w - kPadLeft, r.h}, text_, st, p.ink);
}

bool ListRow::OnMouse(const MouseEvent& ev) {
    SelectList* list = List();
    if (!list) return false;
    if (ev.kind == MouseKind::Move) {
        list->SetHighlight(index_);
        return true;
    }
    if (ev.button != MouseButton::Left) return false;
    if (ev.kind == MouseKind::Down) return true;
    if (ev.kind == MouseKind::Up) {
        if (Abs().Contains(ev.pos)) list->Choose(index_);
        return true;
    }
    return false;
}

AccessInfo ListRow::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::ListItem;
    const SelectList* list = List();
    a.selected = list && list->SelectedIndex() == index_;
    a.selectable = true;
    // The row the arrow keys are on has the keyboard focus for a screen reader: the focus itself stays on the
    // Select (the Screen routes the keys to the list), so the row says it, and AccessFocus finds it.
    a.focusable = true;
    a.focused = list && list->Highlight() == index_;
    return a;
}

bool ListRow::AccSelect() {
    SelectList* list = List();
    if (!list) return false;
    list->Choose(index_);  // as a release on the row: onChoose, then the list closes
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------------- Select ---

Select::Select() {
    SetRole(Role::ComboBox);
    SetFocusable(true);
    SetSize(0, kSelectH);
}

void Select::SetItems(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_ >= static_cast<int>(items_.size())) selected_ = -1;
    Invalidate();
}

void Select::SetSelected(int index) {
    selected_ = (index >= 0 && index < static_cast<int>(items_.size())) ? index : -1;
    Invalidate();
}

SizeF Select::Measure(Canvas&, float maxWidth) { return {maxWidth, kSelectH}; }

void Select::Open() {
    Screen* s = GetScreen();
    if (!s || !enabled_ || open_ || items_.empty()) return;
    auto list = std::make_unique<SelectList>(this, items_, selected_);
    const RectF r = Abs();
    const float h = static_cast<float>(items_.size()) * kSelectRowH + 2 * kBorder;
    // Under the select; over it when the list would run off the window and there is room above.
    float y = r.Bottom();
    if (y + h > s->Size().h && r.y - h >= 0) y = r.y - h;
    list->SetBounds({r.x, y, r.w, h});
    list->SetName(name_);  // the list is called what the select is called (a screen reader reads "Voice, list")
    list->LayoutRows();
    open_ = true;
    s->OpenPopup(std::move(list), this);
    Invalidate();
}

void Select::OnPopupClosed() {
    open_ = false;
    Invalidate();
}

void Select::Choose(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) return;
    if (onChoose_) onChoose_(index);  // the shown choice changes with the next state, not now
}

bool Select::OnMouse(const MouseEvent& ev) {
    if (ev.button != MouseButton::Left) return false;
    if (ev.kind == MouseKind::Down) {
        Open();  // like a native select: the list opens on the press
        return true;
    }
    return ev.kind == MouseKind::Up;  // the release of that press is ours too
}

bool Select::OnKey(const KeyEvent& ev) {
    if (!enabled_) return false;
    if (ev.key == Key::Down || ev.key == Key::Up || ev.key == Key::Enter || ev.key == Key::Space) {
        Open();
        return true;
    }
    return false;
}

void Select::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    {
        OpacityScope fade(canvas, enabled_ ? 1.0f : 0.7f);
        canvas.FillRect(r, p.card, kRadius);
        canvas.StrokeRect({r.x + kBorder / 2, r.y + kBorder / 2, r.w - kBorder, r.h - kBorder}, p.line,
                          kBorder, kRadius);
        if (selected_ >= 0) {
            // The text is clipped before the chevron instead of running under it.
            canvas.PushClip({r.x + kBorder, r.y, r.w - kBorder - kPadRight, r.h});
            canvas.Text({r.x + kPadLeft, r.y, r.w - kPadLeft, r.h}, items_[static_cast<size_t>(selected_)],
                        TextStyle{}, p.ink);
            canvas.PopClip();
        }
        // The chevron of the design (viewBox 14: M2.5 5 l4.5 4.5 L11.5 5, stroke 2.2), scaled to 16.
        const float k = kChevron / 14.0f;
        const float ox = r.Right() - kChevronRight - kChevron;
        const float oy = r.y + (r.h - kChevron) / 2;
        canvas.Polyline({{ox + 2.5f * k, oy + 5.0f * k}, {ox + 7.0f * k, oy + 9.5f * k},
                         {ox + 11.5f * k, oy + 5.0f * k}},
                        p.ink, 2.2f * k);
    }
    if (RingVisible()) DrawFocusRing(canvas, p, r, kRadius);
}

bool Select::AccExpand(bool expand) {
    Screen* s = GetScreen();
    if (!s || !enabled_) return false;
    if (expand) {
        if (items_.empty()) return false;
        Open();  // nothing to do when it is open already
    } else if (open_) {
        s->ClosePopup();
    }
    return true;
}

AccessInfo Select::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::ComboBox;
    if (selected_ >= 0) a.value = items_[static_cast<size_t>(selected_)];
    a.expanded = open_ ? Tri::Yes : Tri::No;
    return a;
}

}  // namespace nui

#include "native_ui/sheet.h"

#include "native_ui/screen.h"

#include <algorithm>
#include <cmath>

namespace nui {

namespace {
constexpr float kRadius = 6, kBorder = 1, kTitleLineHeight = 1.2f;
}  // namespace

Sheet::Sheet(float width) : width_(width) { SetRole(Role::Dialog); }

void Sheet::SetTitle(std::string title) {
    title_ = std::move(title);
    SetName(title_);
    Invalidate();
}

Widget* Sheet::AddBody(std::unique_ptr<Widget> w) {
    Widget* raw = AddChild(std::move(w));
    body_.push_back(raw);
    return raw;
}

Button* Sheet::AddAction(std::unique_ptr<Button> b) {
    Button* raw = static_cast<Button*>(AddChild(std::move(b)));
    actions_.push_back(raw);
    return raw;
}

void Sheet::Close() {
    if (Screen* s = GetScreen()) s->CloseSheet(this);
}

void Sheet::PrepareLayout(Canvas& canvas) {
    const float innerW = width_ - 2 * kSheetPad;
    float y = kSheetPad;

    const float titleH = kSheetTitleSize * kTitleLineHeight;  // CSS line-height 1.2
    titleBox_ = {kSheetPad, y, innerW, titleH};
    y += titleH + kSheetTitleGap;

    // The body lines, one under the other, each as tall as it says for this width.
    bool anyBody = false;
    for (Widget* w : body_) {
        if (!w->Visible()) continue;
        const SizeF s = w->Measure(canvas, innerW);
        w->SetBounds({kSheetPad, y, innerW, s.h});
        y += s.h + kSheetLineGap;
        anyBody = true;
    }
    if (anyBody) y -= kSheetLineGap;  // no gap after the last line

    // The actions: a row, 12 apart; a full row wraps. A hidden button takes no room.
    bool anyAction = false;
    float x = kSheetPad, rowH = 0;
    for (Button* b : actions_) {
        if (!b->Visible()) continue;
        if (!anyAction) {
            y += kSheetActionsTop;
            anyAction = true;
        }
        const SizeF s = b->Measure(canvas, innerW);
        if (x > kSheetPad && x + s.w > kSheetPad + innerW) {  // does not fit: next row
            x = kSheetPad;
            y += rowH + kSheetActionGap;
            rowH = 0;
        }
        b->SetBounds({x, y, s.w, s.h});
        x += s.w + kSheetActionGap;
        rowH = std::max(rowH, s.h);
    }
    y += rowH;

    const float h = y + kSheetPad;
    // Centred in the window, on whole DIPs so the 1 DIP border is crisp.
    float sx = 0, sy = 0;
    if (const Screen* screen = GetScreen()) {
        sx = std::floor((screen->Size().w - width_) / 2);
        sy = std::floor((screen->Size().h - h) / 2);
    }
    SetBounds({sx, sy, width_, h});

    Widget::PrepareLayout(canvas);  // anything nested in the body
}

void Sheet::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    canvas.FillRect(r, p.card, kRadius);
    canvas.StrokeRect({r.x + kBorder / 2, r.y + kBorder / 2, r.w - kBorder, r.h - kBorder}, p.line, kBorder,
                      kRadius - kBorder / 2);
    TextStyle st;
    st.size = kSheetTitleSize;
    st.weight = FontWeight::Bold;
    canvas.Text({r.x + titleBox_.x, r.y + titleBox_.y, titleBox_.w, titleBox_.h}, title_, st, p.ink);
}

AccessInfo Sheet::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Dialog;
    a.name = title_;
    return a;
}

}  // namespace nui

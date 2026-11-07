#include "native_ui/sheet.h"

#include "native_ui/screen.h"

#include <algorithm>
#include <cmath>

namespace nui {

namespace {
constexpr float kRadius = 6, kBorder = 1, kTitleLineHeight = 1.2f;

TextStyle TitleStyle() {
    TextStyle st;
    st.size = kSheetTitleSize;
    st.weight = FontWeight::Bold;
    return st;
}
}  // namespace

Sheet::Sheet(float width) : width_(width) { SetRole(Role::Dialog); }

void Sheet::SetTitle(std::string title) {
    title_ = std::move(title);
    SetName(title_);
    Invalidate();
}

Widget* Sheet::AddBody(std::unique_ptr<Widget> w, float gapAfter) {
    Widget* raw = AddChild(std::move(w));
    body_.push_back({raw, gapAfter});
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
    // The content starts inside the 1 DIP border and the 24 DIP padding (CSS: border 1px, padding 24px).
    const float inset = kSheetPad + kBorder;
    const float innerW = width_ - 2 * inset;

    // The title: one line, or more when it is wider than the card (a long Voice name). The line height is the
    // CSS `line-height: 1.2` of the size, not what the canvas measures, so every sheet has the same rhythm.
    titleLineH_ = kSheetTitleSize * kTitleLineHeight;
    titleLines_ = WrapRuns(canvas, {TextRun{title_, true}}, TitleStyle(), innerW);
    const float titleH = titleLineH_ * static_cast<float>(std::max<size_t>(1, titleLines_.size()));
    titleBox_ = {inset, inset, innerW, titleH};

    // First pass: how tall is each body line for this width.
    std::vector<float> bodyH(body_.size(), 0.0f);
    float bodyTotal = 0;
    bool anyBody = false;
    for (size_t i = 0; i < body_.size(); ++i) {
        if (!body_[i].widget->Visible()) continue;
        bodyH[i] = body_[i].widget->Measure(canvas, innerW).h;
        bodyTotal += bodyH[i] + body_[i].gapAfter;
        anyBody = true;
    }
    float lastGap = 0;
    for (size_t i = body_.size(); i-- > 0;) {
        if (body_[i].widget->Visible()) {
            lastGap = body_[i].gapAfter;
            break;
        }
    }
    if (anyBody) bodyTotal -= lastGap;  // no gap after the last line

    // The actions: rows, 12 apart; a full row wraps. A hidden button takes no room. Placed relative to the
    // top of the actions block, so the total height is known before the body is placed.
    struct Placed {
        Button* button;
        RectF rect;
    };
    std::vector<Placed> placed;
    float actionsH = 0;
    {
        float x = inset, y = 0, rowH = 0;
        for (Button* b : actions_) {
            if (!b->Visible()) continue;
            const SizeF s = b->Measure(canvas, innerW);
            if (x > inset && x + s.w > inset + innerW) {  // does not fit: next row
                x = inset;
                y += rowH + kSheetActionGap;
                rowH = 0;
            }
            placed.push_back({b, {x, y, s.w, s.h}});
            x += s.w + kSheetActionGap;
            rowH = std::max(rowH, s.h);
        }
        if (!placed.empty()) actionsH = y + rowH;
    }
    const float actionsBlock = placed.empty() ? 0.0f : kSheetActionsTop + actionsH;

    // The flexible body gives way when the whole card would be taller than its maximum.
    float total = inset + titleH + titleGap_ + bodyTotal + actionsBlock + inset;
    if (flexBody_ && maxHeight_ > 0 && total > maxHeight_) {
        for (size_t i = 0; i < body_.size(); ++i) {
            if (body_[i].widget != flexBody_ || !body_[i].widget->Visible()) continue;
            const float want = std::max(std::min(bodyH[i], kSheetFlexMin), bodyH[i] - (total - maxHeight_));
            total -= bodyH[i] - want;
            bodyH[i] = want;
        }
    }

    float y = inset + titleH + titleGap_;
    for (size_t i = 0; i < body_.size(); ++i) {
        if (!body_[i].widget->Visible()) continue;
        body_[i].widget->SetBounds({inset, y, innerW, bodyH[i]});
        y += bodyH[i] + body_[i].gapAfter;
    }
    if (anyBody) y -= lastGap;
    if (!placed.empty()) {
        y += kSheetActionsTop;
        for (const Placed& p : placed) p.button->SetBounds({p.rect.x, y + p.rect.y, p.rect.w, p.rect.h});
        y += actionsH;
    }

    const float h = y + inset;
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
    const TextStyle st = TitleStyle();
    float y = r.y + titleBox_.y;
    if (titleLines_.size() <= 1) {  // one line: one call, so the text engine sets it as a whole
        canvas.Text({r.x + titleBox_.x, y, titleBox_.w, titleLineH_}, title_, st, p.ink);
        return;
    }
    for (const WrappedLine& line : titleLines_) {
        for (const LinePiece& piece : line.pieces) {
            canvas.Text({r.x + titleBox_.x + piece.x, y, piece.width + 4, titleLineH_}, piece.text, st, p.ink);
        }
        y += titleLineH_;
    }
}

AccessInfo Sheet::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Dialog;
    a.name = title_;
    return a;
}

}  // namespace nui

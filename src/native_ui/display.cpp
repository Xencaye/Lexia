#include "native_ui/display.h"

#include <algorithm>

namespace nui {

namespace {

// Notice numbers (components.css `.notice`).
constexpr float kNoticePadX = 12, kNoticePadY = 10, kNoticeBorder = 3, kNoticeGap = 9, kMarkSize = 20,
                kMarkTop = 1;
constexpr float kBarH = 6;

// '\n' splits the text of a Paragraph into paragraphs; an empty line is kept out.
std::vector<std::string> SplitParagraphs(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const std::string part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) out.push_back(part);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

std::string JoinRuns(const std::vector<TextRun>& runs) {
    std::string s;
    for (const TextRun& r : runs) s += r.text;
    return s;
}

}  // namespace

// ------------------------------------------------------------------------------------ Label ---

Label::Label(std::string text) : text_(std::move(text)) {
    SetRole(Role::Text);
    SetName(text_);
}

void Label::SetText(std::string text) {
    text_ = std::move(text);
    SetName(text_);
    Invalidate();
}

void Label::SetStyle(float size, FontWeight weight) {
    style_.size = size;
    style_.weight = weight;
    Invalidate();
}

SizeF Label::Measure(Canvas& canvas, float) { return canvas.MeasureText(text_, style_); }

void Label::Paint(Canvas& canvas, const Palette& p) {
    canvas.Text(Abs(), text_, style_, Resolve(p, colour_));
}

AccessInfo Label::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Text;
    a.value = text_;
    return a;
}

// --------------------------------------------------------------------------------- Paragraph ---

Paragraph::Paragraph(std::string text) : tmpl_(std::move(text)) { SetRole(Role::Text); }

void Paragraph::SetText(std::string tmpl, std::map<std::string, std::string> bold) {
    tmpl_ = std::move(tmpl);
    bold_ = std::move(bold);
    Invalidate();
}

void Paragraph::SetStyle(float size, FontWeight weight) {
    style_.size = size;
    style_.weight = weight;
    Invalidate();
}

std::string Paragraph::PlainText() const {
    std::string out;
    for (const std::string& para : SplitParagraphs(tmpl_)) {
        if (!out.empty()) out += '\n';
        out += JoinRuns(ParseTemplate(para, bold_));
    }
    return out;
}

Paragraph::Layout Paragraph::LayoutFor(Canvas& canvas, float width) const {
    Layout out;
    out.lineHeight = lineHeight_ > 0 ? lineHeight_ : LineHeight(canvas, style_);
    float y = 0;
    size_t lineNo = 0;  // the indents count lines of the whole widget, not of one paragraph
    bool first = true;
    for (const std::string& para : SplitParagraphs(tmpl_)) {
        if (!first) y += paragraphGap_;
        first = false;
        // The indents of the lines this paragraph will get.
        std::vector<float> indents;
        if (lineNo < indents_.size()) indents.assign(indents_.begin() + static_cast<std::ptrdiff_t>(lineNo), indents_.end());
        std::vector<WrappedLine> lines = WrapRuns(canvas, ParseTemplate(para, bold_), style_, width, indents);
        for (WrappedLine& l : lines) {
            out.lines.push_back({std::move(l), y});
            y += out.lineHeight;
            ++lineNo;
        }
    }
    out.height = y;
    return out;
}

SizeF Paragraph::Measure(Canvas& canvas, float maxWidth) {
    return {maxWidth, LayoutFor(canvas, maxWidth).height};
}

void Paragraph::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const Layout layout = LayoutFor(canvas, r.w);
    for (const Placed& pl : layout.lines) {
        for (const LinePiece& piece : pl.line.pieces) {
            TextStyle st = style_;
            st.weight = piece.bold ? FontWeight::Bold : style_.weight;
            st.align = TextAlign::Left;
            // A little slack: the piece was measured, but a text engine may round up.
            canvas.Text({r.x + piece.x, r.y + pl.y, piece.width + 4, layout.lineHeight}, piece.text, st,
                        Resolve(p, colour_));
        }
    }
}

AccessInfo Paragraph::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Text;
    a.value = PlainText();
    return a;
}

// ------------------------------------------------------------------------------------ Notice ---

Notice::Notice() {
    SetRole(Role::Text);
    SetLiveRegion(true);  // a result that appears is announced
}

Notice::Geometry Notice::Place(Canvas& canvas, float width) const {
    Geometry g;
    TextStyle leadStyle;
    leadStyle.weight = FontWeight::Bold;
    TextStyle textStyle;
    g.lineHeight = LineHeight(canvas, textStyle);
    g.leadW = lead_.empty() ? 0.0f : canvas.MeasureText(lead_, leadStyle).w;
    g.textX = kNoticeBorder + kNoticePadX + kMarkSize + kNoticeGap + (lead_.empty() ? 0.0f : g.leadW + kNoticeGap);
    g.textW = std::max(0.0f, width - kNoticePadX - g.textX);
    g.lines = WrapRuns(canvas, std::vector<TextRun>{TextRun{text_, false}}, textStyle, g.textW);
    const float textH = static_cast<float>(g.lines.size()) * g.lineHeight;
    const float inner = std::max({kMarkTop + kMarkSize, textH, lead_.empty() ? 0.0f : g.lineHeight});
    g.height = 2 * kNoticePadY + inner;
    return g;
}

SizeF Notice::Measure(Canvas& canvas, float maxWidth) { return {maxWidth, Place(canvas, maxWidth).height}; }

void Notice::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const Geometry g = Place(canvas, r.w);
    const bool bad = kind_ == Kind::Bad;
    const Colour accent = bad ? p.danger : p.accent;
    const Colour textColour = bad ? p.danger : p.ink;

    canvas.FillRect(r, p.paper);
    canvas.FillRect({r.x, r.y, kNoticeBorder, r.h}, accent);

    // The mark: a ring with a check (ok) or an exclamation sign (bad), drawn with lines.
    const float mx = r.x + kNoticeBorder + kNoticePadX;
    const float my = r.y + kNoticePadY + kMarkTop;
    canvas.StrokeEllipse({mx + 1, my + 1, kMarkSize - 2, kMarkSize - 2}, accent, 1.75f);
    if (!bad) {
        canvas.Polyline({{mx + 5.5f, my + 10.5f}, {mx + 8.8f, my + 13.8f}, {mx + 14.5f, my + 7.0f}}, accent, 2.0f);
    } else {
        canvas.Line({mx + 10, my + 5.5f}, {mx + 10, my + 11.5f}, accent, 2.0f);
        canvas.FillEllipse({mx + 9, my + 13.5f, 2, 2}, accent);
    }

    float x = mx + kMarkSize + kNoticeGap;
    if (!lead_.empty()) {
        TextStyle leadStyle;
        leadStyle.weight = FontWeight::Bold;
        canvas.Text({x, r.y + kNoticePadY, g.leadW + 4, g.lineHeight}, lead_, leadStyle, accent);
    }
    float y = r.y + kNoticePadY;
    for (const WrappedLine& line : g.lines) {
        for (const LinePiece& piece : line.pieces) {
            canvas.Text({r.x + g.textX + piece.x, y, piece.width + 4, g.lineHeight}, piece.text, TextStyle{},
                        textColour);
        }
        y += g.lineHeight;
    }
}

AccessInfo Notice::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Text;
    a.name = lead_;
    a.value = text_;
    a.liveRegion = true;
    return a;
}

// ------------------------------------------------------------------------------- ProgressBar ---

ProgressBar::ProgressBar() {
    SetRole(Role::ProgressBar);
    SetSize(0, kBarH);
}

void ProgressBar::SetFraction(double f) {
    fraction_ = std::min(1.0, std::max(0.0, f));
    Invalidate();
}

SizeF ProgressBar::Measure(Canvas&, float maxWidth) { return {maxWidth, kBarH}; }

void ProgressBar::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    canvas.FillRect(r, p.line, kBarH / 2);
    const float w = r.w * static_cast<float>(fraction_);
    if (w > 0.5f) canvas.FillRect({r.x, r.y, w, r.h}, p.accent, std::min(kBarH / 2, w / 2));
}

AccessInfo ProgressBar::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::ProgressBar;
    a.hasRange = true;
    a.min = 0;
    a.max = 1;
    a.current = fraction_;
    a.step = 0;
    return a;
}

// ------------------------------------------------------------------- IconWidget, PictureWidget ---

IconWidget::IconWidget(std::string icon, ColourRole colour) : icon_(std::move(icon)), colour_(colour) {
    SetRole(Role::Image);
}

void IconWidget::Paint(Canvas& canvas, const Palette& p) { canvas.Icon(Abs(), icon_, Resolve(p, colour_)); }

PictureWidget::PictureWidget(std::string picture, PictureTint tint)
    : picture_(std::move(picture)), tint_(tint) {
    SetRole(Role::Image);
}

void PictureWidget::Paint(Canvas& canvas, const Palette& p) {
    Colour tint = p.tintFlute;
    switch (tint_) {
        case PictureTint::Flute: tint = p.tintFlute; break;
        case PictureTint::BeeOff: tint = p.tintBeeOff; break;
        case PictureTint::BeeOn: tint = p.tintBeeOn; break;
        case PictureTint::Otter: tint = p.tintOtter; break;
        case PictureTint::Wine: tint = p.tintWine; break;
        case PictureTint::Dog: tint = p.tintDog; break;
    }
    canvas.Picture(Abs(), picture_, tint, opacity_);
}

}  // namespace nui

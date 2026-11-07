#pragma once
// The widgets that show something and do nothing (K1.b): Label (one line), Paragraph (wrapped text with
// bold `{name}` runs and optional per-line indents), Notice (a mark, a lead word and a text), ProgressBar,
// IconWidget and PictureWidget. Look from ui/css/components.css (`.notice`) and base.css; numbers in DIPs.
#include "native_ui/text_layout.h"
#include "native_ui/widget.h"

#include <map>
#include <string>
#include <vector>

namespace nui {

// ------------------------------------------------------------------------------------ Label ---
// One line of text in `bounds`, aligned, in a palette colour. It is not cut or wrapped.
class Label : public Widget {
public:
    explicit Label(std::string text = std::string());
    void SetText(std::string text);
    const std::string& Text() const { return text_; }
    void SetStyle(float size, FontWeight weight = FontWeight::Regular);
    void SetAlign(TextAlign a) { style_.align = a; Invalidate(); }
    void SetUnderline(bool u) { style_.underline = u; Invalidate(); }
    void SetLetterSpacing(float dips) { style_.letterSpacing = dips; Invalidate(); }
    void SetColour(ColourRole c) { colour_ = c; Invalidate(); }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    std::string text_;
    TextStyle style_;
    ColourRole colour_ = ColourRole::Ink;
};

// --------------------------------------------------------------------------------- Paragraph ---
// Wrapped text. The text may hold several paragraphs, one per '\n', `paragraphGap` apart. `{name}`
// placeholders become bold runs with the text given in `bold` (SetText). `indents[i]` is the left indent
// of the i-th line of the whole widget (lines past the list have none): the About page's text goes
// round an engraving with 27, 23, 15, 11, 7, 0. The height follows the width: Measure(canvas, w).
class Paragraph : public Widget {
public:
    explicit Paragraph(std::string text = std::string());
    void SetText(std::string tmpl, std::map<std::string, std::string> bold = {});
    void SetStyle(float size, FontWeight weight = FontWeight::Regular);
    void SetColour(ColourRole c) { colour_ = c; Invalidate(); }
    void SetIndents(std::vector<float> indents) { indents_ = std::move(indents); Invalidate(); }
    void SetParagraphGap(float gap) { paragraphGap_ = gap; Invalidate(); }
    // The height of a line in DIPs (the design's `line-height: 20px`); 0 = what the canvas measures.
    void SetLineHeight(float h) { lineHeight_ = h; Invalidate(); }
    void SetLetterSpacing(float dips) { style_.letterSpacing = dips; Invalidate(); }

    struct Placed {
        WrappedLine line;
        float y = 0;  // from the top of the widget
    };
    struct Layout {
        std::vector<Placed> lines;
        float height = 0;
        float lineHeight = 0;
    };
    Layout LayoutFor(Canvas& canvas, float width) const;
    std::string PlainText() const;  // the text with the placeholders filled, paragraphs joined by '\n'

    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    std::string tmpl_;
    std::map<std::string, std::string> bold_;
    TextStyle style_;
    ColourRole colour_ = ColourRole::Ink;
    std::vector<float> indents_;
    float paragraphGap_ = 8;
    float lineHeight_ = 0;
};

// ------------------------------------------------------------------------------------ Notice ---
// Padding 10 / 12, a 3 DIP left border in accent, the paper fill, a 20 DIP mark (a check in a circle, or
// "!" in a circle), a bold lead word and a text, 9 apart. Never colour alone: the mark and the lead say
// the kind too. Bad: the border, mark, lead and text are in the danger colour.
class Notice : public Widget {
public:
    enum class Kind { Ok, Bad };
    Notice();
    void SetKind(Kind k) { kind_ = k; Invalidate(); }
    Kind GetKind() const { return kind_; }
    void SetLead(std::string lead) { lead_ = std::move(lead); Invalidate(); }
    void SetText(std::string text) { text_ = std::move(text); Invalidate(); }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    struct Geometry {
        float textX = 0, textW = 0, leadW = 0, height = 0, lineHeight = 0;
        std::vector<WrappedLine> lines;
    };
    Geometry Place(Canvas& canvas, float width) const;
    Kind kind_ = Kind::Ok;
    std::string lead_, text_;
};

// ------------------------------------------------------------------------------- ProgressBar ---
// 6 tall, radius 3, a `line` track and an accent fill.
class ProgressBar : public Widget {
public:
    ProgressBar();
    void SetFraction(double f);  // 0..1
    double Fraction() const { return fraction_; }
    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    double fraction_ = 0;
};

// ------------------------------------------------------------------- IconWidget, PictureWidget ---
class IconWidget : public Widget {
public:
    explicit IconWidget(std::string icon = std::string(), ColourRole colour = ColourRole::Accent);
    void SetIcon(std::string icon) { icon_ = std::move(icon); Invalidate(); }
    void SetColour(ColourRole c) { colour_ = c; Invalidate(); }
    const std::string& Icon() const { return icon_; }
    void Paint(Canvas& canvas, const Palette& palette) override;

private:
    std::string icon_;
    ColourRole colour_;
};

// Which tint of the palette an engraving takes.
enum class PictureTint { Flute, BeeOff, BeeOn, Otter, Wine, Dog };

class PictureWidget : public Widget {
public:
    explicit PictureWidget(std::string picture = std::string(), PictureTint tint = PictureTint::Flute);
    void SetPicture(std::string picture) { picture_ = std::move(picture); Invalidate(); }
    void SetTint(PictureTint t) { tint_ = t; Invalidate(); }
    void SetOpacity(float o) { opacity_ = o; Invalidate(); }
    const std::string& Picture() const { return picture_; }
    void Paint(Canvas& canvas, const Palette& palette) override;

private:
    std::string picture_;
    PictureTint tint_;
    float opacity_ = 1;
};

}  // namespace nui

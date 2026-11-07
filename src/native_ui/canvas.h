#pragma once
// Canvas: what the widgets of the native Settings window draw on (K1, ADR-0009). Pure interface,
// Win32-free, in lexia_core. Two implementations:
//   - D2dCanvas (src/native_ui/d2d_canvas.*, lexia.exe): Direct2D + DirectWrite on the window.
//   - FakeCanvas (tests/fake_canvas.h): records the calls and measures text by character count, so
//     the widget tree is tested without a window.
//
// All numbers are DIPs (1/96 inch), with (0,0) at the top left of the 800 x 600 client area. The
// canvas scales to pixels itself. Text is UTF-8 (one line per call: the widgets wrap text themselves,
// so the About page can indent each line by hand). Icons and pictures are named by their file name
// without extension ("home-05", "bee"); the canvas finds them in the exe's resources.
#include "native_ui/palette.h"

#include <string_view>
#include <vector>

namespace nui {

struct PointF {
    float x = 0, y = 0;
};

struct SizeF {
    float w = 0, h = 0;
};

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    bool Contains(PointF p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
};

enum class FontWeight { Regular = 400, SemiBold = 600, Bold = 700 };

enum class TextAlign { Left, Centre, Right };

// One line of text in the one family (Atkinson Hyperlegible Next).
struct TextStyle {
    float size = 16;                         // font size in DIPs (CSS px)
    FontWeight weight = FontWeight::Regular;
    bool underline = false;
    TextAlign align = TextAlign::Left;       // inside the box given to Text
    float letterSpacing = 0;                 // DIPs added after every character (the design's `letter-spacing: 0.01em` on titles)
};

class Canvas {
public:
    virtual ~Canvas() = default;

    // A filled rectangle; radius > 0 rounds the corners.
    virtual void FillRect(const RectF& r, Colour c, float radius = 0) = 0;
    // The outline of a rectangle, the stroke centred on the edge.
    virtual void StrokeRect(const RectF& r, Colour c, float width, float radius = 0) = 0;
    virtual void FillEllipse(const RectF& bounds, Colour c) = 0;
    virtual void StrokeEllipse(const RectF& bounds, Colour c, float width) = 0;
    // A straight line with round caps (the power glyph, the check mark, the chevron).
    virtual void Line(PointF a, PointF b, Colour c, float width) = 0;
    // Connected lines with round joins and caps.
    virtual void Polyline(const std::vector<PointF>& points, Colour c, float width) = 0;
    // An arc of the circle (centre, radius) from startDeg, sweepDeg long, clockwise, 0 = 3 o'clock.
    virtual void Arc(PointF centre, float radius, float startDeg, float sweepDeg, Colour c, float width) = 0;
    // A 1 DIP dotted rule (the design's `1px dotted`).
    virtual void DottedLine(PointF a, PointF b, Colour c) = 0;

    // One line of text in `box`, vertically centred, aligned by style.align. Text longer than the box
    // is not cut (the layout gives room).
    virtual void Text(const RectF& box, std::string_view utf8, const TextStyle& style, Colour c) = 0;
    // The width and the line height of one line of text.
    virtual SizeF MeasureText(std::string_view utf8, const TextStyle& style) = 0;

    // An SVG icon of src/res/icons (now resources), drawn in one colour into `box` (square).
    virtual void Icon(const RectF& box, std::string_view name, Colour c) = 0;
    // An engraving (black lines on white JPEG) as a mask: alpha = 1 - luminance, colour = tint.
    virtual void Picture(const RectF& box, std::string_view name, Colour tint, float opacity) = 0;

    // Clip and opacity layers; each Push has one Pop.
    virtual void PushClip(const RectF& r) = 0;
    virtual void PopClip() = 0;
    virtual void PushOpacity(float opacity) = 0;
    virtual void PopOpacity() = 0;
};

}  // namespace nui

#pragma once
// Word wrapping for the native Settings window (K1.b). The Canvas draws one line of text per call, so
// the widgets wrap themselves, measuring with Canvas::MeasureText. This is the one wrapper, shared by
// Paragraph, Notice and the Sheet's lines. Pure, Win32-free.
//
// Text is a list of runs (regular or bold). Runs are cut into words at spaces; a word that continues
// across a run boundary ("{games}," : a bold word then a comma) is never split, like in HTML. A word
// wider than the line stands alone on its line (no break inside a word). Each line may have its own
// left indent (the About page's text goes round an engraving).
#include "native_ui/canvas.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace nui {

struct TextRun {
    std::string text;
    bool bold = false;
};

// One drawn piece: a part of a word in one weight, at x from the start of the line (indent included).
struct LinePiece {
    std::string text;
    bool bold = false;
    float x = 0;
    float width = 0;
};

struct WrappedLine {
    std::vector<LinePiece> pieces;
    float indent = 0;
    float width = 0;  // the pieces' extent, indent included
};

// "text with {name} placeholders" -> runs; a name in `bold` becomes a bold run with the given text, a
// name that is not there is left as it is ("{name}"). Braces without a name stay as text.
std::vector<TextRun> ParseTemplate(std::string_view tmpl, const std::map<std::string, std::string>& bold);

// Wraps `runs` to `maxWidth`. `base` gives size / underline; each run's weight comes from `bold`.
// indents[i] is the left indent of line i (lines past the list have none). Empty text gives no line.
std::vector<WrappedLine> WrapRuns(Canvas& canvas, const std::vector<TextRun>& runs, const TextStyle& base,
                                  float maxWidth, const std::vector<float>& indents = {});

// The width of a space in `style`. MeasureText of " " alone may be trimmed to nothing by a text engine,
// so it is measured as the difference of "x x" and "xx".
float SpaceWidth(Canvas& canvas, const TextStyle& style);

// The line height of `style` (the canvas's, from a line with an ascender and a descender).
float LineHeight(Canvas& canvas, const TextStyle& style);

}  // namespace nui

#pragma once
// Turns the loose pieces of text the OCR finds into reading-ordered blocks
// (paragraphs, headings, single names...). Pure functions: no Win32, no ONNX.
#include <string>
#include <vector>

#include "common.h"

// Puts items in reading order, in place: rows top to bottom, left to right inside a row.
// Defined for TextLine and TextBlock only (explicitly instantiated in layout.cpp).
template <typename T>  // T needs a Rect member called `box`
void SortReadingOrder(std::vector<T>& v);

// Tuning knobs of GroupBlocks. All distances are in line heights (the median Line height of the
// Shot), so they do not depend on the text size or the scale of the Shot. The defaults are the
// values tuned against the Fixtures (docs/notes/layout-baseline.md); settings.json can override them.
struct LayoutParams {
    double paragraphGap = 0.9;  // the first gap in a Block may be at most this
    double columnGap = 5.0;     // an empty vertical strip this wide separates two columns
    double listLine = 8.0;      // a Line this wide or wider may go on into the next one;
                                // shorter stacked Lines are a list, one Block per entry
};

// Groups lines into blocks using spacing and alignment, in reading order.
std::vector<TextBlock> GroupBlocks(std::vector<TextLine> lines,
                                   const LayoutParams& params = LayoutParams());

// All blocks joined into one string.
std::string BlocksToText(const std::vector<TextBlock>& blocks);

// Index of the smallest block under point `p`, or -1. `tolerance` px on every side of a block still
// count, to forgive a click that lands just beside the text (8 at 96 dpi; the caller scales it).
int FindBlockAt(const std::vector<TextBlock>& blocks, Point p, int tolerance = 8);

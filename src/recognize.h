#pragma once
// The one way to turn an area of a Shot into Blocks: OCR the area, then group the Lines.
// The Analysis (whole Shot), a Drag (a rectangle) and lexia_cli all go through here.
#include <vector>

#include <opencv2/core.hpp>

#include "common.h"
#include "layout.h"

class OcrEngine;

struct Recognition {
    std::vector<TextBlock> blocks;  // in reading order, in full-Shot coordinates
    long long ms = 0;               // how long recognising took
};

// Recognises `area` of the Shot's image. `isDrag`: the user dragged this area, so if nothing
// is detected the whole area is still read as one piece of text (see OcrEngine::ReadLines).
// `layout` tunes the grouping of the Lines into Blocks.
Recognition RecognizeBlocks(OcrEngine& ocr, const cv::Mat& bgr, Rect area, bool isDrag,
                            const LayoutParams& layout = LayoutParams());

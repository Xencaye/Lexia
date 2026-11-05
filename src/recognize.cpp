#include "recognize.h"

#include <chrono>

#include "layout.h"
#include "ocr.h"

// The one path from pixels to Blocks: OCR finds the Lines in `area` of the Shot, then layout groups
// them into Blocks in Reading order. The Analysis, a Drag and lexia_cli all call this, so the CLI and
// the Fixture score test the same pipeline as the app. `ms` is the time of both steps together.
Recognition RecognizeBlocks(OcrEngine& ocr, const cv::Mat& bgr, Rect area, bool isDrag,
                            const LayoutParams& layout) {
    const auto t0 = std::chrono::steady_clock::now();
    Recognition r;
    r.blocks = GroupBlocks(ocr.ReadLines(bgr, area, isDrag), layout);
    r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0)
               .count();
    return r;
}

#pragma once
// Neural OCR: a detector finds where text is, a recogniser reads each piece.
// Both are PaddleOCR models run locally with ONNX Runtime. The ONNX details are hidden
// behind this interface so the rest of the program doesn't need ONNX headers.
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "common.h"
#include "language.h"

class OcrEngine {
public:
    OcrEngine();
    ~OcrEngine();
    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;

    static std::string ModelsDir();

    // Look for the models in this folder (UTF-8) instead of beside the exe. Call before Init.
    // Only lexia_cli uses it, to add the dev models/ folder in a Release build.
    static void SetModelsDir(const std::string& utf8Dir);
    static bool ModelsPresent(const Language& lang);

    // Loads the detector (once) and the recogniser for `lang`. On failure the
    // previously loaded language keeps working.
    bool Init(const Language& lang);

    // Finds text inside `area` of the image. Returned boxes are in full-image coordinates.
    // `isDrag`: the user dragged this area, so if nothing is detected we still try
    // to read the whole area as one piece of text (handy for a single short word).
    std::vector<TextLine> ReadLines(const cv::Mat& bgr, Rect area, bool isDrag);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

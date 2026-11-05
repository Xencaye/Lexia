# PaddleOCR models via ONNX Runtime for text recognition

Lexia recognises text with PaddleOCR's detection and recognition models run through ONNX Runtime,
rather than Windows' built-in OCR (`Windows.Media.Ocr`) or Tesseract. Both alternatives were tried
and did poorly on stylised, rendered text (game fonts, text over busy backgrounds), which is
exactly the text Lexia exists to read.

## Consequences

- The models and ONNX Runtime ship with Lexia and are not in git (`scripts\fetch_deps.ps1`).
- Each Language needs its own recognition model and character dictionary.

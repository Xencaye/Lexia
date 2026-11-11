# PaddleOCR models via ONNX Runtime for text recognition

Lexia recognises text with PaddleOCR's detection and recognition models run through ONNX Runtime,
rather than Windows' built-in OCR (`Windows.Media.Ocr`) or Tesseract. Both alternatives were tried
and did poorly on stylised, rendered text (game fonts, text over busy backgrounds), which is
exactly the text Lexia exists to read.

## Consequences

- The models and ONNX Runtime ship with Lexia. They are downloads from pinned URLs (`scripts\fetch_deps.ps1`),
  except Lexia's own fine-tuned `rec_en.onnx` (task L, 2026-11-11), which is in git as `ocr_models/rec_en.onnx`
  and copied into `models\` by the same script.
- Each Language needs its own recognition model and character dictionary.

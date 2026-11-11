# ocr_models

`rec_en.onnx`: Lexia's English recogniser. `scripts\fetch_deps.ps1` copies it into `models\` after a SHA-256 check
(the hash is pinned in the script). It is the only model kept in git; the others (`det.onnx`, `dict_en.txt`, the
Cyrillic ones) are downloads from pinned URLs.

## Where it comes from

PaddleOCR's `en_PP-OCRv3_rec` (Apache-2.0, Copyright (c) PaddlePaddle Authors), **modified**: fine-tuned by the Lexia
project (task L) so that it reads pixel game fonts, from PaddleOCR's published training checkpoint. The architecture,
the input `[N, 3, 48, W]`, the output `[N, T, 97]` and the dictionary (`dict_en.txt`) are the same as the original,
so `src/ocr.cpp` and `src/ctc.*` read it unchanged. The notice is also in the model's metadata
(`training/rec_en/stamp_onnx.py`).

- Training scripts: `training/rec_en/` (steps in its `README.md`). The data is made again from the pinned fonts and
  books and a fixed seed; it is not in git.
- This file: round 2 of the training (scripts at commit `5eae7a3`), **epoch 5** (`iter_epoch_5`), chosen on the
  Fixtures with `training/rec_en/score_epochs.ps1`. SHA-256
  `A9CDCF89764D08C8F52C47C542E814C89263F09FB01959DCE00A6C29F2F23360`.
- Training fonts and their licences: `docs/notes/training-fonts.md`; credited in the package by
  `licenses/components/rec-en-training-fonts.txt`.

## Scores (2026-11-11)

| | PaddleOCR's model | This model |
| --- | --- | --- |
| Character error rate, synthetic pixel-font check lines | 12.76 % | 0.87 % |
| Character error rate, synthetic normal-font check lines | 3.61 % | 0.47 % |
| Fixtures: average distance (`score_layout.ps1`) | 0.131 | 0.110 |
| Fixtures: average Block-count error | 1.29 | 1.00 |

Known regressions: `unraveled_04` (0.018 -> 0.045, beyond the tolerance): a blurred lamp is read as "3" (the owner
accepted it, Nov 11); `lovebyte_02` (0.018 -> 0.036, inside the tolerance): "Version n Q.9.18" where a line crosses
the text, and an icon read as "e".

Known limits of this model (fixed in the scripts on Nov 11, for the next training): it never saw "_" in training
(it reads "angel_OS_blue" as "angel...OS... blue"), and its game-text templates were shaped after the Undertale
screen, so the `undertale_05` gain is likely higher than on games it never saw.

## Replacing it

A retrained model replaces this file in a new commit and the hash in `scripts\fetch_deps.ps1` in the same commit.
Each version adds about 9 MB to the history, so retrain only for a real gain (plan, task L6).

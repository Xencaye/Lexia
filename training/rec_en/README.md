# Training the English recogniser (task L)

Fine-tunes PaddleOCR's `en_PP-OCRv3_rec` so that Lexia reads pixel game fonts (Undertale and others) without
letter errors, and normal text no worse. The result is a new `rec_en.onnx` with the same input and output as the
old one, so `src/ocr.cpp` and `src/ctc.*` do not change. The plan is task L in
`docs/Lexia — Code Review & Agent Plan.md`; the fonts and their licences are in `docs/notes/training-fonts.md`.

Nothing here is built by CMake or shipped. The training data is **not** in git: the scripts download the fonts and
texts (pinned by SHA-256 in `sources.lock.json`) and draw the lines again from a fixed seed.

## What you need

- Windows 11, an NVIDIA GPU with 8 GB or more (made on an RTX 3080, 10 GB) and a current NVIDIA driver. No CUDA
  install: CUDA 11.8 and cuDNN 8.9 come as pip packages into the venv.
- Python 3.12 (`winget install Python.Python.3.12`; PaddlePaddle 2.6.2 has no build for 3.13) and git.
- About 15 GB free on the drive of the work folder.

The work folder is `%USERPROFILE%\LexiaTrain` (set `$env:LEXIA_TRAIN_DIR` for another). It must not be inside
OneDrive or the repo: it holds 400 000 images.

## Steps

From the repo root in PowerShell:

```powershell
.\training\rec_en\setup.ps1          # venv, PaddleOCR v2.9.1, the pretrained checkpoint, PaddleOCR's rec_en.onnx,
                                     # fonts, texts, 400 000 lines
.\training\rec_en\train.ps1          # about 4-5 hours on an RTX 3080 (10 epochs)
.\training\rec_en\score_epochs.ps1   # every epoch's Fixture score; pick the epoch with no Fixture worse
.\training\rec_en\export.ps1 -Checkpoint iter_epoch_5   # -> <work>\rec_en.onnx, the notice, then check_onnx.py
.\training\rec_en\try_model.ps1      # the Fixture score with the new model in models\, then the old one back
```

- `train.ps1 -Resume` goes on after a stop (Ctrl+C is safe; `latest` is saved every epoch, so at most one epoch,
  about 25 minutes, is lost).
- The log is `<work>\output\train.log`. Every 2000 steps it evaluates on the check lines: `acc` is the share of
  lines read exactly, `norm_edit_dis` 1 - the normalised edit distance. `best_accuracy` is the best so far.
- **Choose the epoch on the Fixtures, not by `best_accuracy`.** In the three runs of Nov 10-11 the best epoch on the
  synthetic check lines was 8-10, and the best on the real Fixtures was earlier (round 2: epoch 5, which ships).
  `score_epochs.ps1 -Run <folder> -Epochs 4,5,6` scores another run folder or other epochs; it does not touch
  `models\`.
- To ship a model: copy `<work>\rec_en.onnx` to `ocr_models\rec_en.onnx`, put its SHA-256 in
  `scripts\fetch_deps.ps1`, run `fetch_deps.ps1`, `ctest` and `score_layout.ps1 -SaveBaseline`, and update
  `ocr_models/README.md` (plan, task L6).
- `try_model.ps1 -Keep` leaves the new model in `models\`, to try Lexia itself with it (a Release build reads the
  `models\` folder beside `lexia.exe` in the build: copy it there too). To put the shipped model back, run
  `scripts\fetch_deps.ps1` (it copies `ocr_models\rec_en.onnx` again when the hash in `models\` is not the pinned
  one). `<work>\rec_en.original.onnx` is PaddleOCR's own model, only for `check_onnx.py`: never put it in `models\`.

## What `check_onnx.py` checks

The new model has one input `[N, 3, 48, W]` and one output `[N, T, 97]` of softmax scores; on the synthetic check
lines its character error rate (CER) on normal fonts is at most 0.5 points worse than PaddleOCR's own model
(`<work>\rec_en.original.onnx`, kept by `setup.ps1`; `models\` now holds Lexia's fine-tune), and on pixel fonts
better. `try_model.ps1` then runs `scripts\score_layout.ps1`: no Fixture may be worse than the baseline.

## Files

| File | What it does |
| --- | --- |
| `sources.json` | The fonts (pixel and normal) and the public-domain books the lines are made from |
| `sources.lock.json` | The SHA-256 of every download. `fetch_sources.py --pin` adds the hash of a new entry |
| `fetch_sources.py` | Downloads them, checks the hashes and each font's licence file, writes `<work>\sources\manifest.json` |
| `gen_lines.py` | Draws the lines: 40 % pixel fonts (no anti-aliasing, scaled like a game), 60 % normal fonts; resized to height 48 exactly as Lexia does |
| `dict_en.txt` | Lexia's `models\dict_en.txt` with LF line ends (the same 95 characters in the same order) |
| `lexia_rec_en.yml` | The PaddleOCR training config (the reasons for each change are at its top) |
| `requirements.txt` | The pinned Python packages |
| `setup.ps1`, `train.ps1`, `score_epochs.ps1`, `export.ps1`, `try_model.ps1`, `common.ps1` | The steps above |
| `check_onnx.py` | The model check |
| `stamp_onnx.py` | Writes the Apache-2.0 "modified" notice and the training commit into the model's metadata |
| `warm_files.py` | Reads every image once before training (a cold first epoch is 4x slower on Windows) |

## Security

All downloads are pinned (pip versions, the PaddleOCR commit, the checkpoint and every font and book by SHA-256);
the checkpoint's `.pdparams` are pickles and are loaded only from the pinned archive. Python runs with `-I`, so no
file in a download folder is imported. Archives are unpacked member by member, refusing `..`, absolute paths and
links. Nothing from the user's screen is used: the Fixtures are only the test, never training data, and
`gen_lines.py` refuses any label equal to a line of a Fixture's `expected.txt`.

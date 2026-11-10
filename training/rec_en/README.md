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
.\training\rec_en\setup.ps1        # venv, PaddleOCR v2.9.1, the pretrained checkpoint, fonts, texts, 400 000 lines
.\training\rec_en\train.ps1        # about 4-5 hours on an RTX 3080 (10 epochs)
.\training\rec_en\export.ps1       # -> <work>\rec_en.onnx, then check_onnx.py
.\training\rec_en\try_model.ps1    # the Fixture score (score_layout.ps1) with the new model, then the old one back
```

- `train.ps1 -Resume` goes on after a stop (Ctrl+C is safe; `latest` is saved every epoch).
- The log is `<work>\output\train.log`. Every 2000 steps it evaluates on the check lines: `acc` is the share of
  lines read exactly, `norm_edit_dis` 1 - the normalised edit distance. `best_accuracy` is the best so far.
- `export.ps1 -Checkpoint iter_epoch_6` exports another epoch. The best on the synthetic check lines is not always
  the best on the real Fixtures: try two or three epochs with `try_model.ps1` and keep the best.
- `try_model.ps1 -Keep` leaves the new model in `models\`, to try Lexia itself with it (a Release build reads the
  `models\` folder beside `lexia.exe` in the build: copy it there too). Put the old one back from
  `<work>\rec_en.original.onnx`.

## What `check_onnx.py` checks

The new model has one input `[N, 3, 48, W]` and one output `[N, T, 97]` of softmax scores; on the synthetic check
lines its character error rate (CER) on normal fonts is at most 0.5 points worse than the old model, and on pixel
fonts better. `try_model.ps1` then runs `scripts\score_layout.ps1`: no Fixture may be worse than the baseline.

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
| `setup.ps1`, `train.ps1`, `export.ps1`, `try_model.ps1`, `common.ps1` | The steps above |
| `check_onnx.py` | The model check |
| `warm_files.py` | Reads every image once before training (a cold first epoch is 4x slower on Windows) |

## Security

All downloads are pinned (pip versions, the PaddleOCR commit, the checkpoint and every font and book by SHA-256);
the checkpoint's `.pdparams` are pickles and are loaded only from the pinned archive. Python runs with `-I`, so no
file in a download folder is imported. Archives are unpacked member by member, refusing `..`, absolute paths and
links. Nothing from the user's screen is used: the Fixtures are only the test, never training data.

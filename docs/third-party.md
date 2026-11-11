# Third-party components

A plain list of what Lexia builds on and ships, with licences. All are GPL-3-compatible
(see `docs/adr/0003-gpl-3-licence.md`). The package gets the full text of every licence in
`THIRD-PARTY-NOTICES.txt` and Lexia's own in `LICENSE` (task H2; see "The package's notices" below).

| Component | Licence | Used for |
| --- | --- | --- |
| ONNX Runtime 1.20.1 | MIT | Runs the OCR models and the Piper Voices (`onnxruntime\`, fetched by `scripts\fetch_deps.ps1`). Its own list of bundled software ships as `licenses\onnxruntime-ThirdPartyNotices.txt` |
| OpenCV 4.14.0 | Apache-2.0 (and Berkeley SoftFloat, BSD-3-Clause style, inside opencv_core) | Image handling (`opencv_core`, `opencv_imgproc`, via vcpkg); `opencv_imgcodecs` in `lexia_cli` only |
| libpng | libpng-2.0 | PNG reading and writing for `lexia_cli` only (via vcpkg, OpenCV's `png` feature); not shipped |
| zlib | Zlib | Compression, used by OpenCV (z.dll is shipped) and libpng (via vcpkg) |
| nlohmann-json | MIT | Reading `settings.json` and writing `lexia_cli`'s JSON output (via vcpkg) |
| doctest | MIT | Unit tests only (`tests\`, via vcpkg); not shipped |
| PaddleOCR PP-OCR models | Apache-2.0 | Text detection and recognition (`models\`), as ONNX conversions from SWHL/RapidOCR and deepghs/paddleocr (the latter under its RDL licence, which defers to the original PaddleOCR licence). `rec_en.onnx` is **modified**: fine-tuned by the Lexia project for game fonts (task L), kept in `ocr_models\` (`ocr_models/README.md`), with the notice in the model's metadata and in `licenses/components/paddleocr-models.txt` |
| Training fonts of `rec_en.onnx` | SIL OFL 1.1, CC0, BSD-2-Clause, Apache-2.0, Ubuntu Font Licence | Only to draw the synthetic training lines; no font is shipped or in the model. Credited in `licenses/components/rec-en-training-fonts.txt`; the list and the licence checks are in `docs/notes/training-fonts.md` |
| SoundTouch 2.4.1 | LGPL-2.1-or-later | Shifts the pitch of synthesised speech without changing its length (Voice control Pitch, `src/tts/pitch_shift.cpp`). Via vcpkg (`soundtouch`), linked dynamically: `SoundTouch.dll` ships next to the exe, so a user can replace it with their own build as the LGPL asks. Source: https://codeberg.org/soundtouch/soundtouch |
| miniz 3.1.2 | MIT | Reads the zip when the user adds a Voice from a zip in the Voice library (`src/tts/voice_library.cpp`). Via vcpkg (`miniz`); `miniz.dll` ships next to the exe |
| Piper (libpiper, OHF-Voice/piper1-gpl v1.8.0) | GPL-3.0-or-later | Neural text to speech, in-process through Lexia's ONNX Runtime (`piper.dll`). Built from the pinned commit by `CMakeLists.txt` (`libpiper_external`); one line is patched for ONNX Runtime 1.20 (`cmake/patch_libpiper.cmake`). Bundles nlohmann/json (MIT) and uni-algo (Unlicense or MIT) as headers |
| espeak-ng (commit 212928b, 2025-03-22) | GPL-3.0-or-later (some data files Apache-2.0 / BSD-2-Clause / Unicode licence) | Turns text into phonemes for Piper; linked into `piper.dll`, and its `espeak-ng-data\` folder ships next to the exe. Cloned at the commit pinned in piper1-gpl's `libpiper\CMakeLists.txt` |
| ucd-tools (espeak-ng submodule) | GPL-3.0-or-later (Unicode data under the Unicode licence) | Unicode character data for espeak-ng, linked into `piper.dll` |
| Microsoft Visual C++ runtime | Microsoft's redistribution terms for Visual Studio | `msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll` copied into the package so the PC needs no installer. Not covered by Lexia's licence |
| Piper Voice `en_GB-cori-medium` | CC BY 4.0 (the dataset). Model: no licence stated | The default English Voice, `voices\`. The voice of Cori Samuel (LibriVox reader 92), from the Hi-Fi Multi-Speaker English TTS Dataset by NVIDIA (https://www.openslr.org/109/, CC BY 4.0; Bakhturina et al., Interspeech 2021). Fine-tuned by the Piper project from its `hifi_tts_92` base (trained from scratch on the same recordings), phonemised as British English. The attribution is in the notices. Provided as is. See "Voices" below |
| Piper Voice `en_US-mike-medium` | CC0 (the dataset and the base model). Model: no licence stated | A second English Voice, `voices\`. Dataset from https://github.com/OHF-Voice/voice-datasets (CC0), fine-tuned by the Piper project from its `rdh` base (CC0). No attribution is required, credit given as a courtesy. Provided as is. See "Voices" below |
| Atkinson Hyperlegible Next | SIL Open Font License 1.1 | The font of the Settings window (`src\res\fonts\`, copied unchanged and embedded in the exe as resources; the licence file is beside it) |
| Stratis UI Icons | CC BY 4.0 | The line icons of the Settings window and the Control bar (`src\res\icons\`, SVG, embedded in the exe as resources; the source is in `src\res\icons\SOURCE.txt`). `discord.svg`, `github.svg` and `itch.svg` are those sites' logos, used only to link to Lexia there |
| Inno Setup 6 | Inno Setup License (a zlib-style licence) | Builds the installer (`installer\lexia.iss`, task H4). A build tool, not part of Lexia's code; the setup program it makes carries its own small runtime, which its licence lets anyone redistribute. Its uninstaller (`unins000.exe`) stays in the Lexia folder, so it has an entry in `THIRD-PARTY-NOTICES.txt` (`licenses/components/inno-setup.txt`) |
| Old Book Illustrations engravings | Public domain | The pictures of the Settings window (`src\res\img\`, vectorised by the project as one-colour SVG files, embedded in the exe as resources) and the Lexia logo (`src\res\lexia_logo.svg` and `lexia_logo_off.svg`, made into the exe and tray icons by `scripts\make_tray_icons.ps1`), from https://www.oldbookillustrations.com/. The logo's engraving is "Faust—Scene III Headpiece" (https://www.oldbookillustrations.com/illustrations/headpiece-scene-3/); the rounded square, the crop and the Off version are the Lexia project's own design, and the Off logo also contains the Stratis UI icon `volume-07` (CC BY 4.0, row above) |

Voices the user downloads later (Voice library, task B5) come from the Hugging Face `rhasspy/piper-voices` catalogue, only when the user asks, and are not part of the package; each has its own licence, which is the user's to check. The Voices Lexia ships are the ones above.

Pinned download URLs and hashes for ONNX Runtime, the models and the Voices are in `scripts\fetch_deps.ps1` and `voices\voices.json`; `rec_en.onnx` is copied from `ocr_models\` and checked against the hash in the same script.

## Voices

Voices are separate data files in `voices/`, listed in `voices/voices.json` (with their pinned download source, SHA-256 and a `ship` flag). `scripts/fetch_deps.ps1` fetches them; the package step copies only the ones marked `"ship": true`. The licence research is in task B6.

- **English: `en_GB-cori-medium` (the default) and `en_US-mike-medium`, shipped** (B8; `rhasspy/piper-voices` commit `cf910fb`, 2026-11-09). The default is the first English Voice in `voices.json`. Cori: dataset CC BY 4.0 (Hi-Fi TTS, NVIDIA), fine-tuned from `hifi_tts_92`, which was trained from scratch. Mike: dataset CC0, fine-tuned from `rdh` (CC0). Neither comes from `lessac`. At the earlier pin (`5041a7a`) Mike was fine-tuned from `lessac`, so only the `cf910fb` files may ship. No Voice has a licence line of its own for the model weights; the repository `rhasspy/piper-voices` is tagged MIT and Piper's `docs/VOICES.md` says it imposes no restrictions on voice models. `en_US-ljspeech-medium` shipped in 0.5 and 0.6. `en_US-lessac-medium` and every voice fine-tuned from it must not be shipped (the Blizzard 2013 Lessac data is research-only).
- **Bulgarian: no Voice.** Bulgarian is not offered (ADR-0007); its Voice is to be the owner's own (E6).

## The package's notices

`build\package\Lexia\` (and `Lexia.zip`) holds `LICENSE` (Lexia's GPL-3.0-or-later text, copied from the repository root), `THIRD-PARTY-NOTICES.txt` and `licenses\onnxruntime-ThirdPartyNotices.txt` (ONNX Runtime's own list of what its binary contains). `cmake/package.cmake` makes `THIRD-PARTY-NOTICES.txt` with `cmake/notices.cmake` from the files in `licenses/`:

- `licenses/header.txt` is the top of the file.
- `licenses/components/<name>.txt` is one component, in file-name order. The first line is its title. It gives the name, version where known, licence, copyright holders, source and what Lexia uses it for. Lines that start with `@` are directives:
  - `@include <file>` puts that file's text in as it is. `{ORT_DIR}`, `{SOURCE_DIR}` (the source tree, always set; for the font's and the icons' own licence files), `{VCPKG_SHARE}` (vcpkg's `share\` folder, where each port's `copyright` file is), `{PIPER_SRC}` and `{ESPEAK_SRC}` in the path are replaced by the build's folders. Prefer this to retyping a licence: the text then follows the version that is built.
  - `@licence-text <id>` refers to `licenses/texts/<id>.txt`, which is printed once at the end under LICENCE TEXTS however many components use it.
  - `@ship <file> [as <name>]` copies a long file to `licenses\` in the package instead of repeating it.
  - `@covers <name>...` says which `.exe` / `.dll` files in the package this entry is the licence for (`*` is a wildcard).
- `licenses/components/voice-<Voice name>.txt` is the entry for a Piper Voice. It is used only when `voices/voices.json` has `"ship": true` for that Voice.

The package step fails (so does the build) when a Voice with `"ship": true` has no `voice-<name>.txt`, and when a `.exe` or `.dll` in the package is covered by no `@covers` line. The ctest `package_notices` (`tests/notices_test.cmake`) checks both.

To add a component (the ones planned: SoundTouch, the Atkinson Hyperlegible font):

1. Check its licence is GPL-3-compatible (ADR-0003) and add a row to the table above.
2. Add `licenses/components/<name>.txt`. Take the licence text from the vcpkg port's `copyright` file or the component's own source with `@include`, or add the text to `licenses/texts/` if it is a standard one (the SIL OFL for a font, for instance) and refer to it with `@licence-text`. If the component brings a DLL, add `@covers` for it. Fonts and other data files are not checked by `@covers`; name them in "Used for".
3. Build and read `build\package\Lexia\THIRD-PARTY-NOTICES.txt`.

A new Voice: add its `voice-<name>.txt` (the wording for each kind of Voice is in `docs/notes/voice-licences.md`, section 5) before setting `"ship": true`.

Not covered by `@covers`: the OCR models, `espeak-ng-data\` and the Voice files are data. Their entries (`paddleocr-models.txt`, `espeak-ng.txt`, `voice-*.txt`) are written by hand.

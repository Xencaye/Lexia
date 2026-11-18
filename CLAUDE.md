# Lexia — notes for Claude Code

Windows accessibility app for people with dyslexia: press a hotkey, click or drag on a frozen screenshot of the
monitor, and the text there is OCR'd and read aloud. Works over browsers, images, video and full-screen games.
English only (ADR-0007): no Language control in the UI; Bulgarian is in the code but not offered. Language support is
planned for 0.8, Linux on Ubuntu for 1.0 (stream J).

## Build (Developer PowerShell for VS, $env:VCPKG_ROOT set)

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure      # runs lexia_tests (doctest, tests/) and the tool tests
.\scripts\score_layout.ps1                                 # Fixture score vs tests\fixtures\baseline.json; exit 1 if any got worse
```

Generator: Visual Studio 18 2026. CMake 4.4. C++17. vcpkg manifest mode.

Output: `build\Release\lexia.exe`; package in `build\package\Lexia\` and `Lexia.zip`.

**Installer** (`-DLEXIA_INSTALLER=ON`, needs Inno Setup 6, `winget install JRSoftware.InnoSetup`):
`build\package\Lexia-<version>-setup.exe` from `installer\lexia.iss`, run by `cmake/installer.cmake` after the
package step. Per-user by default (`%LOCALAPPDATA%\Programs\Lexia`; all users is offered). Tasks: Run at startup;
Run as administrator (start state = `runAsAdmin` in the settings.json of the person who runs Setup; written after the
install by `lexia.exe --set-run-as-admin on|off`, run `runasoriginaluser`). A running Lexia is closed for the person
(`installer\close_lexia.isi`, shared by Setup and the uninstaller: a Yes / No question, the registered message
`Lexia.Quit` to the `LexiaApp` window, up to 10 s for the mutex to go, then an offer to end it with `taskkill`, one UAC
prompt for an elevated Lexia; silent runs never ask: Quit, wait, `taskkill` as the user, an elevated survivor stops
the run). Start Lexia on the last page (its window opens, Lexia is Off). No runtime to install; an update removes the
old `ui\` folder and `WebView2Loader.dll` of 0.5, but keeps the old `voices\` files (an update never deletes the
user's data): Lexia adopts a Voice that is no longer shipped into the user's own Voices at its next start
(`AdoptRetiredVoices`). The uninstaller removes the Run value and asks about
`%LOCALAPPDATA%\Lexia`. The installer's words are in the .iss (Inno Setup cannot read `strings/`).

**Signing** (optional, OFF by default and in CI): `-DLEXIA_SIGN=ON` Authenticode-signs `lexia.exe` and `piper.dll` in
the package folder before the zip (`cmake/sign.cmake`; `signtool` from the Windows SDK; certificate by env
`LEXIA_SIGN_THUMBPRINT`, or `LEXIA_SIGN_PFX` + `LEXIA_SIGN_PFX_PASSWORD`; never cached). See `docs/signing.md`.

**clangd** (the LSP tool): `.\scripts\compile_db.ps1` writes `build\clangd\compile_commands.json` (a Ninja configure,
nothing built; the VS generator can't), which `.clangd` points at. Re-run it after adding a source file. clangd is
LLVM's (`C:\Program Files\LLVM\bin`, must be on PATH).

**Not in git** (fetch with `powershell -ExecutionPolicy Bypass -File scripts\fetch_deps.ps1`):
- `onnxruntime\` — prebuilt ONNX Runtime zip (include\, lib\)
- `models\` — det.onnx, rec_en.onnx, dict_en.txt, rec_cyrillic.onnx, dict_cyrillic.txt. `rec_en.onnx` is Lexia's
  fine-tune for game fonts (task L): it is in git as `ocr_models\rec_en.onnx` (`ocr_models/README.md`) and the script
  copies it after a SHA-256 check; a retrained model changes the file and the hash in `fetch_deps.ps1` together
- `voices\` — the Piper Voice files listed in `voices\voices.json` (all of them ship: Cori from its own pinned
  `source`, Norman from the list's; Lexia has no Bulgarian Voice now)

Piper is not fetched: the first build clones piper1-gpl and espeak-ng at pinned commits and builds `piper.dll` into
`build\lp_install` (CMake `libpiper_external`; needs the network and a short build path).

**Release:** build Release with `-DLEXIA_INSTALLER=ON`, get a clear run (How work runs, step 4), copy
`build\package\Lexia-<v>-setup.exe` and `Lexia.zip` (as `Lexia-<v>-portable.zip`) to `release\<v>\` (git-ignored),
tag `v<v>`, then raise the version in `vcpkg.json` for the next one.

## Running Lexia

- `lexia.exe` is a GUI program with no console; it lives in the tray. Tray menu: On / Off, Open Lexia (also a
  double-click on the icon), Quit. Quit = `App::Quit()`: `App::Run` ends, which stops speech, releases the keys,
  closes the Settings window and removes the icon (a clean exit). Closing the Settings window does not end Lexia.
- Only one Lexia runs: a second launch asks the first to open its Settings window and exits.
- At start the Settings window opens (and comes to the front) unless Lexia was started with `--minimised`.
- On / Off is Off on the very first start (no settings.json) and remembered after that. Off (tray, Home, or
  `"enabled": false`) frees every key without ending Lexia.
- Run at startup writes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Lexia` = `"<exe>"`, plus `--minimised`
  when Start minimised is on (on by default, usable only while Run at startup is on). Every start writes the value
  again (this exe's path, the switch as the setting says).
- **Run as admin** (ADR-0008): with `runAsAdmin` on and Lexia not elevated, `wWinMain` lets go of the mutex, starts
  `lexia.exe --elevated-relaunch` (plus `--minimised` if it was) with the `runas` verb (UAC prompt) and ends; the new
  one waits up to 5 s for the mutex and gets the foreground (`AllowSetForegroundWindow`). A cancelled prompt or a
  failure is logged and Lexia goes on unelevated. Turning the switch on in the window relaunches the same way and
  ends the old one with `Quit()`. The Run value is unchanged, so with the switch on every logon shows the UAC prompt.
- Development: `lexia.exe --console` opens a console (log lines and recognised text print there). Ctrl+C or closing
  the console ends Lexia through `ExitProcess`: the keys are released, but `App::Run`'s shutdown does not run, so
  speech is cut off; use the tray Quit for a clean exit.
- If Lexia cannot start (OCR models missing) a message box says so.
- The exe has an application manifest (`src/lexia.manifest`: PerMonitorV2, Common Controls 6, UTF-8 active code page,
  Windows 10/11). Icons: `src/res/lexia.ico` (On, also the exe and Setup icon) and `lexia_off.ico` (Off), made from
  the owner's logo SVGs `src/res/lexia_logo.svg` / `lexia_logo_off.svg` (on the public-domain Old Book Illustrations
  engraving "Faust—Scene III Headpiece") by `scripts\make_tray_icons.ps1` (16–256 px frames, drawn by
  `lexia_svgshot`; committed; rerun only when a logo file changes).

## Dev tools (built, not packaged)

- **`lexia_cli`** (`build\cli\Release\lexia_cli.exe`, its own folder so its extra DLLs stay out of the package):
  `<shot.png> [--lang en|bg] [--rect x,y,w,h] [--json out.json] [--draw out.png] [--score expected.txt]` runs the app's
  OCR + layout on a saved Shot; no flag prints the Blocks' text (the `expected.txt` format; `--score` prints the
  Fixture score as one line of JSON). Models beside itself, else the dev `models\`.
  `--say "text" --lang en|bg --wav out.wav` speaks to a WAV (`--speed`, `--pitch`, `--volume`).
  Voice library: `--voices-catalogue` (the download list compiled into Lexia, `src/tts/download_list.cpp`; no network),
  `--voice-download <name>` (a Voice of that list; the only network use, and only when you run it; checks size and
  MD5), `--voice-add <file.zip|.onnx|.onnx.json>`; the last two write to `%LOCALAPPDATA%\Lexia\voices` unless `--voices-dir <folder>`.
  `--version`.
- **`score_layout.ps1`**: `-Cli <exe>` (default `build\cli\Release`), `-Tolerance` (default 0.02 on the normalised
  distance; the Block-count error may not rise at all), `-SaveBaseline` (rewrites the baseline: say why in the PR).
- **`lexia_barshot`** (`build\barshot\Release`): `--size large|small --state idle|speaking|paused|analysing
  [--hover pause|skip|stop|help|size] [--theme light|dark] [--dpi N] out.png` writes the Control bar as `controlbar.cpp`
  draws it (shadow margin 32 px at 96 dpi), to compare with `wireframe\control_bar_*.png`.
- **`training/rec_en/`** (Python, not built by CMake, not packaged): fine-tunes `rec_en.onnx` for pixel game fonts
  (task L). `setup.ps1` (venv with PaddlePaddle 2.6.2 GPU + CUDA 11.8 pip packages, PaddleOCR v2.9.1, the pretrained
  checkpoint, fonts and books from `sources.json` checked against `sources.lock.json`, PaddleOCR's own `rec_en.onnx`
  to compare with, the lines), `train.ps1`, `score_epochs.ps1` (each epoch's Fixture score: the epoch to ship is
  chosen there, not by the synthetic accuracy), `export.ps1 [-Checkpoint iter_epoch_N]` (ONNX, `stamp_onnx.py` (the
  Apache-2.0 "modified" notice in the model's metadata), `check_onnx.py`), `try_model.ps1` (the Fixture score with
  the new model in `models\`, then the shipped `ocr_models\rec_en.onnx` back). Work folder
  `%USERPROFILE%\LexiaTrain` (outside the repo and OneDrive; nothing in it is committed). Steps in its `README.md`.
- **`lexia_svgshot`** (`build\svgshot\Release`): `--px N in.svg out.png` draws one SVG at N x N px with Direct2D
  (`DrawSvgDocument`, the app's renderer) on a transparent background (straight alpha PNG). No resources, no shared
  code; `make_tray_icons.ps1` uses it (System.Drawing cannot read SVG).
- **`lexia_settingsshot`** (`build\settingsshot\Release`): draws a page of the Settings window off-screen (800 x 600
  DIPs, the same widget tree and `D2dCanvas`; WARP is enough), writes a PNG and prints `colours=N`.
  `--page gallery|home|voices|settings|report|help|about [--on] [--hover <control>] [--voices none|changed|user]
  [--sheet delete|deleteFailed|catalogue|downloading|done|failed|failed-baddownload|added|refused|presetSaved|presetLoaded|presetFailed|presetLoadedOther|presetNotInstalled]
  [--select <key action or switch name>] [--capture read|pause|speech] [--switches] [--reading] [--theme light|dark]
  [--dpi N] out.png`. `gallery` is every toolkit control in its states (hover forced with `Widget::PreviewHover`);
  the real pages are the real `SettingsUi` on the sample `UiState` of `src/settingsshot_sample.*` (also
  `SampleCatalogue()`); `src/settingsshot_pages.*` (`BuildShotPage`) builds them; `--sheet` feeds the sheets through
  the window's `VoiceEventSink`; `--reading` = the selected card is read aloud (stop icon).
  `.\scripts\settings_shots.ps1 [-Dpi N]` writes every page, light and dark, to `build\settingsshot\pages\` with the
  file names of `wireframe\html-reference\`. Compare the layout with `wireframe\html-reference\` (shots of the old
  HTML pages; their colours are old) and the colours with the owner's mockups `wireframe\<page>.png` /
  `<page>_dark.png`. ctest `settingsshot_smoke` fails if a gallery PNG is missing, small or one colour.
- The colour pairs are checked by ctest `palette_test` (`tests/palette_test.cpp`; colours in
  `src/native_ui/palette.cpp`): text 4.5:1, edges and states 3:1.

## Module map

`docs/module-map.md`: which file owns what in `src/`, with its rules (threads, lifetimes, what is pure and tested).
Read the rows of the modules before you plan or review a change to them; a source file added, removed or renamed
under `src/` updates its row in the same change.

## Rules

- **Never take focus in the overlay.** It must stay `WS_EX_NOACTIVATE`: full-screen games
  minimise when they lose focus. Don't "fix" keyboard input going to the game by activating it.
- **Updates keep the user's data** (owner, Nov 10). An update (Setup, the portable zip, or a newer Lexia's first
  start) never deletes or resets settings.json values, the user's Voices, Presets or the chosen Voice unless there is
  no other way; then the PR says why and the owner agrees first. A Voice that a new version stops shipping stays
  usable as one of the user's own Voices, so its controls and Presets keep working. A setting that is no longer read
  stays in the file. Never add `{app}\voices` or `%LOCALAPPDATA%\Lexia` to the installer's `[InstallDelete]`.
- Keep `layout.*` free of Win32 and ONNX so it stays unit-testable.
- Text is UTF-8 in `std::string` internally; convert to UTF-16 only at Win32 boundaries.
  Source files are UTF-8 (compiled with `/utf-8`).
- Zero compiler warnings at `/W4`. Don't silence a warning without a comment saying why.
  CI (`.github/workflows/ci.yml`) builds Release with `-DLEXIA_WARNINGS_AS_ERRORS=ON` (`/WX` on Lexia's own
  targets; OFF by default), runs `ctest` and `score_layout.ps1`. `vcpkg.json` pins `builtin-baseline`: bump it on purpose.
- Any change to grouping/reading order must run the fixture score before and after and report both.
  No fixture may get worse beyond the tolerance in `scripts/score_layout.ps1`.
- Text the user sees or hears (Announcements, Control bar, tray, Settings window) goes through `strings/`, never a
  literal in the code.
- **British English everywhere** (owner, Nov 7): UI text (`strings/table_en.cpp`), the installer, docs and comments ("minimise", "colour", "analyse", "recognise"). Identifiers and keys that come from other APIs or older files keep their spelling (`SC_MINIMIZE`, `RecognizeBlocks`).
- Diagnostics go through the logger (`log.h`), not `std::cout`. Recognised text is never logged.
- New dependencies go in `vcpkg.json`; check the licence first and add it to the package's
  third-party notices (`licenses/components/`, see `docs/third-party.md`).
- Lexia is GPL-3.0-or-later (`docs/adr/0003-gpl-3-licence.md`): every dependency, and every
  shipped voice, must have a GPL-3-compatible licence.
- **Use clangd for C++.** Navigate and review C++ with the LSP tool (clangd): definitions, references,
  hover, call hierarchy, symbols. Cross-check references with Grep until the index is complete. Run
  `.\scripts\compile_db.ps1` after you add a source file. This applies to every subagent too.
- **Use Security Guidance when you plan code.** The Security Guidance plugin (Anthropic directory) must be
  enabled while you plan and write code; if it is missing, ask the owner to install it. Each code plan has a
  Security section (input from files, network only on request (ADR-0005), path traversal, DLL search path,
  COM and UI Automation lifetimes, links only from `LinkUrl`).
- **Comment the code you write.** Every function, class and non-obvious block you add or change gets a comment:
  the why, the rules it keeps, what a caller must know (thread, lifetime, units). Comment `.cpp` bodies too, not only
  headers. A change to commented code updates its comment.
- **Docs are part of the task.** `.\scripts\check_docs.ps1` is the machine half of the docs (what it checks: the
  script's header; CI runs it, `.github/workflows/docs.yml`); the docs review (How work runs, step 5) is the reading
  half. A change that implements or changes a plan item (a task id, an R-finding) updates the plan and this file's "Plan"
  section in the same PR (tick the task, its status, the milestones; add its by-hand check to "The owner's open
  checks"). A PR that needs no plan change gets the label `docs-not-needed` (the content checks still run). The PR
  template (`.github/pull_request_template.md`) has the checklist.

## How work runs

One task per branch (`feat/<task-id>`, from `main`), one task per PR. Every task takes these steps, in order.

**Roles.** The **overseer** is the main session: Opus 5.5 at high effort. It plans, splits the task, briefs and
reviews the builders, owns the clear run, commits and reports. If the session runs another model, tell the owner
before work starts. **Builders** are Sonnet 5.5 subagents (`Agent` with `model: "sonnet"`): `effort: "high"` to write
or fix code, `effort: "medium"` for searches; the docs review is its own agent (step 5). Parallel builders get `isolation: "worktree"` and
first reset to the task branch tip (a worktree starts from `main`). A builder brief holds the task id, the files, the
done-when, the rules of this file that bear on it (clangd, comments, `strings/`, British English, the Security
section) and what to report. Builders do not see the owner's memory: put every owner rule a builder needs in the brief.

1. **Plan** (overseer): the steps, the files, the done-when and a Security section (see Rules).
2. **Build** (builders): the code, its tests and its comments.
3. **Review** (overseer): read every builder diff with clangd and Grep against the Rules. Send each finding back to a
   builder.
4. **Clear run** (owner, Nov 18). One full run is: a Release build in the owner's tree `build\` (stop a running
   `lexia.exe` or `lexia_cli.exe` first and say so) with no `warning` line in the output, then `ctest`,
   `.\scripts\score_layout.ps1` and `.\scripts\check_docs.ps1`. Each failure or warning goes to a builder to fix, then
   the full run starts again from the build. The run is **clear** when one full run finds no issue. The overseer runs
   it itself: a builder's "it passes" does not count. **Docs only:** when the change touches only Markdown files
   (`*.md`), one full run is `.\scripts\check_docs.ps1` alone.
5. **Docs review**: run the agent `docs-reviewer` (`.claude/agents/docs-reviewer.md`: Sonnet 5.5, medium effort, a
   fresh context; it holds the list of documents) with the task's base branch. It fixes each stale line in the docs,
   and reports each document of its list by name, plus the items it leaves to the overseer (code, `strings/`, the
   installer, a licence question). The overseer deals with each of those items. If a file changed, do step 4 again.
6. **Commit and report** (overseer): commit on the task branch when the run is clear (a push needs the owner's yes).
   Stop at the task boundary. Report what changed, the last clear run (with the Fixture numbers), the docs review
   and a short test list for the owner. Only the owner can check what is visual or audible and what happens inside
   games (Undertale is the reference title): list these, and do not claim them.

## Plan

The plan, `docs/Lexia — Code Review & Agent Plan.md`, is the source: task ids, the order of work, what is done
("Done", one line each) and what is open ("Status", "The owner's open checks"). This section is its summary and says
the same.

- **Released:** the last release is Lexia 0.6.2 (tags `v0.5.0` to `v0.6.2`; files in `release\<v>\`, git-ignored).
- **Now:** Lexia 0.7 (`vcpkg.json` says 0.7.0). Done: A6, B9–B13, G3, L0–L3, L5, L6. Open: L4 (new pixel-font
  Fixtures from the owner's screenshots). Owner decisions: the shipped Voices (the old Cori, the default, and Norman)
  are final; the Download sheet's list grows by one row per Voice (`docs/notes/voice-download-list.md`).
- **Open, the owner's:** the by-hand checks in the plan (also L7, the game-font recogniser), D7 (the tutorial video),
  A5. Open code-review findings: R24, R25, R33 (low). No GitHub issue is open.
- **Later:** E1, E2, E6 (a Bulgarian Voice from the owner's own voice) and the Language UI in Lexia 0.8; J (Linux on
  Ubuntu) and H5 (free signing through SignPath Foundation) in Lexia 1.0. 0.7 and 0.8 ship unsigned.
- **History reset (2026-11-05):** a PR or issue number from before the reset (#1–#42, and the tag `v0.5.0`) points to
  the private repo `Xencaye/Lexia-archive`.

## Agent skills

### Issue tracker

GitHub Issues, through the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default labels: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `docs/GLOSSARY.md` (its terms are the words of the code and the UI text) and `docs/adr/`. See
`docs/agents/domain.md`.

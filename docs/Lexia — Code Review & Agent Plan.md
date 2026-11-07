# Lexia — Code Review & Agent Plan

Oct 23, 2026 · @Eleonora · updated Oct 25, 2026 after the design grilling (see [Decisions](#decisions-oct-25-2026)) · status updated Nov 5, 2026

**Lexia 0.5.0 was released on Nov 5, 2026** (tag `v0.5.0`: the installer and a portable zip). The next
release is **Lexia 0.6** (read-along, stream G; `vcpkg.json` says 0.6.0). Lexia 0.5 turned the console
prototype into an app people with dyslexia can install and use without editing files:
- neural voices, English only (ADR-0007; language support is planned for Lexia 0.8);
- a Reading queue with Pause / Skip / Stop;
- spoken Announcements;
- a Settings window;
- a Control bar on the Overlay;
- an installer.

Every term below is defined in `GLOSSARY.md`, and every decision with a trade-off is in `docs/adr/`.
On-screen wording follows the glossary.

## Status (Nov 5, 2026)

**Lexia 0.5.0 is released** (Nov 5, tag `v0.5.0`): `Lexia-0.5.0-setup.exe` and `Lexia-0.5.0-portable.zip`.
Done and merged into `main`: Phase 0, A1–A4, B1–B6, C1–C5, D2–D6, D8, D9, D10, E3–E5, H1–H4 and I1 (D2–D8 in
PR #31; a cleanup of unused code in #32 and a readability pass with comments in #33, the D8 fixes in #35,
D9 the start options in #37, the About page in #38, H4 the installer in #39, D10 in #40, all Nov 5).
No GitHub issue is open.
**History reset (Nov 5):** the repo `Xencaye/Lexia` restarted from one commit of the current files. The PR and issue
numbers in this plan (#1–#42) and the tag `v0.5.0` are in the old history, now the private repo `Xencaye/Lexia-archive`.
Carried over from 0.5 into 0.6 (the owner's):

- **D7**: the owner supplies the tutorial video (the key card is read aloud instead of clips: D10).
- **D8**: the owner's by-hand tests, the final dark palette, and the 6 colour pairs of the design that fail the contrast check.
- **H4**: the owner's check of the installer's pages; later, signing the installer and a CI job.
- **A5** (owner): the check in games, a browser and a video with subtitles.

In progress (Nov 7): **K, the native Settings window** (ADR-0009), on `feat/K-native-settings`: K0 to K6 and K8 (the
owner's changes after the first look) are done and merged there. **K7, your check, is the only open step**; then K merges
to `main`.
Later: G (Lexia 0.6), E1, E2, E6 (the owner's Bulgarian Voice) and the Language UI (Lexia 0.8), and J, Linux on
Ubuntu (Lexia 1.0).

## Verdict

No rebuild. The module split (capture → ocr → layout → overlay → speech) is the right shape, C++ is still the right language, and the build/packaging setup is better than most hobby projects at this stage.

What needs work before agents can move fast is not the features but the scaffolding around them:

1. **A way to verify without you.** Today every change waits for you to build and judge the result by eye. Claude Code running on your Windows PC can build itself, but it still can't *see* whether grouping got better. An offline test harness (screenshot in → blocks out, compared against expected results) fixes that.
2. **One owner of app state.** `main.cpp` holds everything in one function and one thread. That is fine now, but the tray icon, settings window and a second speech engine all need a central `App` object and OCR off the UI thread.
3. **Small correctness fixes** in the build (UTF-8 source, warnings, an app manifest) that will bite as soon as Bulgarian strings and a GUI arrive.

Scope note: this review covers `CMakeLists.txt`, `vcpkg.json` and `main.cpp`. The other modules (ocr, layout, overlay, speech, settings, capture, language) weren't shared, so task P0.1 is a full audit of them by an agent that can see the whole repo.

## Code review findings

High = fix in Phase 0. Medium = fix when the area is touched. Low = nice to have.

| # | Where | Finding | Fix | Priority |
| --- | --- | --- | --- | --- |
| R1 | CMakeLists | No `/utf-8`. MSVC reads source in the system code page (cp1251 on a Bulgarian Windows), so Cyrillic literals and `—` in comments can be mangled or warn. | `target_compile_options(lexia PRIVATE /utf-8)` | High |
| R2 | CMakeLists | No warning level. Bugs that `/W4 /permissive-` would catch go unnoticed, and agents have no compiler feedback to learn from. | `/W4 /permissive-`, fix warnings once, then keep at zero | High |
| R3 | CMakeLists | Everything compiles into one exe, so nothing can be tested in isolation. | Split into `lexia_core` (static lib: layout, ocr, language, settings) + `lexia` exe + `lexia_tests` + `lexia_cli` | High |
| R4 | CMakeLists | No app manifest. DPI awareness is set in code, which works, but a GUI later needs Common Controls v6 and a manifest is the documented way to declare per-monitor DPI v2. | Add `lexia.manifest` to the sources | Medium |
| R5 | CMakeLists | ONNX Runtime is linked by raw `.lib` path and its DLLs found by `file(GLOB)` at configure time. An empty glob makes the copy command fail with a confusing error. | Wrap in an `IMPORTED` target (`onnxruntime::onnxruntime`) with `IMPORTED_LOCATION`, copy via `$<TARGET_RUNTIME_DLLS>` | Medium |
| R6 | CMakeLists | Packaging runs after every build, Debug included. | Run only for Release (`$<CONFIG:Release>`) or move to a `package` target | Low |
| R7 | CMakeLists | Console subsystem. Fine for now; a tray app shouldn't open a console window for friends. | `WIN32` subsystem + optional `--console` flag that calls `AllocConsole` | Medium (Phase 3) |
| R8 | main.cpp | `Run()` owns all state as locals and handles every hotkey inline. Adding a tray menu and settings window means duplicating that logic. | Extract an `App` class with `OpenOverlay()`, `Stop()`, `SwitchLanguage()`, `ApplySettings()`; hotkeys, tray and settings window all call it | High |
| R9 | main.cpp | Drag-OCR runs inside `onSelect` on the UI thread, so the overlay freezes while it runs. Full-screen analysis on overlay open presumably does the same. | OCR worker thread with a job queue; results posted back with `PostMessage` | Medium |
| R10 | main.cpp | `LastAnalysisMs()` is a global read from a different module — hidden coupling. | Return timing inside the result struct | Low |
| R11 | main.cpp | Empty-text check uses `find_first_not_of(' ')`, so text that is only newlines or tabs is spoken as "nothing". | Use `Trim(text).empty()` from common.h. **Fixed** (Nov 5, PR #33, in `App::OpenOverlay`) | Low |
| R12 | main.cpp | If `ocr.Init(cand)` fails during a language switch, the engine may be left half-initialised and the old language gone. | Build a new `OcrEngine`, swap only on success | Medium |
| R13 | main.cpp | A hotkey clash makes the app exit. For a tray app the user should get a notification and a way to pick another key. | Report and continue; mark the action unbound | Medium (Phase 3) |
| R14 | main.cpp | All diagnostics go to `std::cout`. With no console on a friend's PC there is nothing to send you. | Small logger writing to `%LOCALAPPDATA%\Lexia\lexia.log` (keep console output too) | High |
| R15 | main.cpp | Colour conversion `0xRRGGBB → COLORREF` lives in `main`. | Have settings return `COLORREF` directly | Low |
| R16 | Repo | No tests, no CI, no `CLAUDE.md`. | Phase 0 | High |
| R17 | main.cpp:88; settings.cpp:149,151,162; ocr.cpp:76,91-92,98,374 | `fs::path::string()` converts to the ANSI code page and throws `std::system_error` for characters outside it (a Cyrillic folder on an English Windows). main.cpp:88 runs at every start and nothing catches it: crash on startup. When it does convert, cp1251 bytes go to a UTF-8 console as mojibake. | `.u8string()` wherever a path is printed (P0.7) | High |
| R18 | layout.h:4,18; common.h:3,11,17 | layout.* pulls in `<windows.h>` and uses `RECT`/`POINT`, against the "layout.* free of Win32" rule. | Plain `Rect`/`Point` in common.h; convert in ocr/overlay (P0.4) | Medium |
| R19 | overlay.cpp:288 | A Click before the Analysis finishes runs `while (!g_ready) Sleep(15)` on the UI thread with no message pump: the Overlay freezes and F8/Esc/F7 do nothing until it ends (they are queued). | Hold the Click and handle it when the Analysis is done (C2) | Medium |
| R20 | overlay.cpp:242; ocr.cpp:129,150; main.cpp:182-199 | No exception boundary in the Analysis worker or `main()`. Only `Ort::Exception` is caught, so `cv::Exception`, `bad_alloc` or R17's `system_error` end in `std::terminate` with no message or log. | Catch `std::exception` in the worker (empty Blocks, set ready) and in `main()`; log it (C2, P0.7) | Medium |
| R21 | main.cpp:158-160 | Every Pick's recognised text goes to stdout. A mechanical cout→logger swap in P0.7 would put it in lexia.log, against the intent of ADR-0005 (the log is meant to be sent to you). | Drop the text, or log it only at an off-by-default level (P0.7) | Medium |
| R22 | settings.cpp:19 | settings.json lives next to the exe. If installed under Program Files (Inno's usual `{autopf}` default) it is read-only, so defaults can't be written and C3 can't save the Language. | `%LOCALAPPDATA%\Lexia\settings.json`, exe folder as fallback for the zip (H4, C3) | Medium |
| R23 | ocr.cpp:123-127 vs 380; overlay.cpp:263-269 | `OcrEngine::Init` replaces `rec_`/`dict_`/`useSpace_` unlocked while `ReadLines` may run on other threads. Safe today only because the Overlay loop has no HK_LANG branch; C3 would make it a data race. | Switch only after `worker.join()`, or swap a new engine under a mutex (C3, see R12) | Medium |
| R24 | capture.cpp:21-22,34 | `CreateCompatibleDC` and `BitBlt` results are unchecked. A failed capture (secure desktop, locked session) is a black Shot reported as success, so B3's "Screen capture failed" never fires for these failures. | Check both, return false (B3) | Low |
| R25 | capture.cpp:34 | DIB bits are read right after `BitBlt` without the `GdiFlush()` the CreateDIBSection docs require. | `GdiFlush()` after `BitBlt` | Low |
| R26 | common.h:28; ocr.cpp:180; layout.cpp:56 | `HasLetterOrDigit` counts any byte ≥ 0x80 as a letter, so lines of only "—", "•", "…" pass the filters and are read aloud. | Decode UTF-8 and test letter ranges (A3, E3) | Low |
| R27 | layout.cpp:142 | The hyphen re-join tests `std::islower` on the first byte only, so it never fires for Cyrillic: "раз-/дел" is read as "раз- дел". | Also accept lowercase Cyrillic; run the fixture score (A3, E3) | Low |
| R28 | layout.cpp:14 | `SortReadingOrder` is a file-`static` template not declared in layout.h, but P0.5 is to test it. | Declare it in layout.h (P0.5) | Low |
| R29 | speech.h:6; speech.cpp:18; language.h:24 | `Utf8ToWide` lives in speech.* (CLAUDE.md says common.h), so B1 deleting SAPI removes it from under main.cpp:100-101,166. `GuessLanguage` takes `std::wstring`, against "UTF-8 internally". | Move `Utf8ToWide` to common.h; `GuessLanguage` takes UTF-8 (P0.4) | Low |
| R30 | CMakeLists.txt:40; ocr.cpp:18-19,53 | `MODELS_DIR` bakes the build machine's absolute path into the Release exe; a package without `models\` falls back to it and prints it. | Define `MODELS_DIR` only for Debug, CLI and tests (P0.3) | Low |
| R31 | vcpkg.json | No `builtin-baseline`, so OpenCV and nlohmann-json versions depend on the vcpkg checkout doing the build. | Add `builtin-baseline` (P0.8) | Low |
| R32 | overlay.cpp:81-93,151; layout.cpp:166 | Fixed pixel sizes with no DPI scaling: hint pill in the stock font with fixed offsets, 6 px click/drag threshold, 8 px hover tolerance. At 200% the hint is tiny and thresholds half as wide. | Overlay scales by `GetDpiForWindow()/96`; pass the tolerance into `FindBlockAt` so layout.* stays Win32-free (D4) | Low |
| R33 | main.cpp:186-187 | `SetProcessDpiAwarenessContext` is imported statically, so the exe won't load before Windows 10 1703 and the `SetProcessDPIAware` fallback is dead code. | Drop the fallback; declare DPI in the manifest (with R4) | Low |
| R34 | overlay.cpp:34-36 | Overlay state (`g_sel`, `g_ready`, `g_analysisMs`) is file-scope globals, so `RunSelector` can't be re-entered. | Move into a per-Session struct (C1, with R10) | Low |
| R35 | settings.cpp:180-183; main.cpp:72 | Two actions on the same key in settings.json aren't detected. The second `RegisterHotKey` fails, main.cpp:72 says "another app may be using it", and Lexia exits. | Reject duplicates in settings parsing (note + default for the second one) (P0.5 test, with R13) | Medium |

ADR-0001 and ADR-0005 hold in the code.

Unverified, for the owner to test: GDI `BitBlt` returning black for exclusive-fullscreen or HDR games; the detector using every core while a game runs (stutter?); SAPI `SetVoice` changing the voice of text already queued; whether ONNX Runtime allows a second `Ort::Env` (R12's fix would create one).

Things that are good and should stay: pimpl around ONNX and SAPI (SAPI goes away in B1; keep the pattern for Piper), `WS_EX_NOACTIVATE` overlay for full-screen games (ADR-0001), per-value fallback in settings, models resolved next to the exe first, STA COM with `Run()` scoped so SAPI is released before `CoUninitialize`.

## Working with Claude Code

The biggest speed-up is that Claude Code runs on your Windows PC, so it can run the CMake build and read the errors itself. The paste-errors-back loop goes away.

**Setup (once)**

1. Put the project in git and push to a private GitHub repo. Agents work on branches; you review diffs. Done: private repo `Xencaye/Lexia`.
2. Install Claude Code on the Windows machine where the build already works, and start it from a "Developer PowerShell for VS" with `VCPKG_ROOT` set, so it inherits the toolchain.
3. Drop the supplied `CLAUDE.md` in the repo root. Every session reads it: build commands, module map, rules.
4. Keep `models/` and `onnxruntime/` out of git (`.gitignore`), with a `scripts/fetch_deps.ps1` that downloads them, so worktrees can be set up quickly.

**How to split work between agents**

- **One task = one branch = one worktree.** Parallel agents only on tasks that touch different modules (the plan marks which). Two agents editing `main.cpp` at once will collide.
- **Every task has a check the agent can run itself**: it builds with zero warnings, `lexia_tests` passes, and for layout work the fixture score doesn't drop.
- **You stay the eyes and ears.** Anything about how it looks, sounds or behaves in a game is a manual check on your list, not something to delegate.
- **Start each task in plan mode** for anything bigger than a bug fix, read the plan, then let it run.
- **Small PRs.** Ask agents to stop at the task boundary and summarise; don't chain three tasks into one diff.

**Suggested agent roles**

| Role | Does | Works in |
| --- | --- | --- |
| Lead (your main session) | Picks the next task, writes the brief, reviews diffs, merges | main branch |
| Builder | Implements one task on its own worktree | `feat/<task-id>` |
| Reviewer | Fresh session reads the diff against the task's acceptance criteria and R-findings; doesn't edit | read-only |
| Researcher | Licences, API docs, voice availability; writes findings to `docs/notes/` | no code |
| Designer | Unused since Oct 28: Figma is scrapped. The `ui/` pages are the design (D0) and the Builder makes them | no code |

## Phase 0 — Foundations (do first, mostly sequential)

Goal: by the end of Phase 0 an agent can change layout or speech code and prove it didn't break anything without asking you. Expect 1–2 weeks of evenings.

Tracked as GitHub issues [#1–#11](https://github.com/Xencaye/Lexia/issues), with the order set by native blocked-by links: pick any open issue with no open blockers. P0.4, P0.5 and P0.6 are split into two issues each, one PR per issue (branches `feat/P0.4b`, `feat/P0.5b`, `feat/P0.6b` for the second parts).

- [x] **P0.1 Repo audit.** Agent reads every module and adds findings R17+ to this doc (threading, error handling, leaks, Unicode, DPI). No code changes. *Done when:* findings list exists with priorities.
- [x] **P0.2 Git + fetch script.** `.gitignore` for `build/`, `models/`, `onnxruntime/`; `scripts/fetch_deps.ps1` downloads the pinned ONNX Runtime zip and the 5 model files and checks SHA-256. *Done when:* a fresh clone builds after running the script. Issue: [#1](https://github.com/Xencaye/Lexia/issues/1).
- [x] **P0.3 Build hygiene (R1, R2, R5, R6, R30).** `/utf-8 /W4 /permissive-`, ORT as an imported target, package only in Release, `MODELS_DIR` not baked into Release. *Done when:* Release build has 0 warnings. Issue: [#2](https://github.com/Xencaye/Lexia/issues/2).
- [x] **P0.4 Split targets (R3).** `lexia_core` static lib + `lexia` exe. No behaviour change. *Done when:* app runs exactly as before (your manual check: F8, click, drag, F7, Ctrl+Alt+L).
  - **P0.4** [#4](https://github.com/Xencaye/Lexia/issues/4): the split, plus one core entry point for "recognise an area of the Shot → Blocks" used by both the Analysis and the Drag, so `lexia_cli` runs the app's pipeline rather than a copy.
  - **P0.4b** [#5](https://github.com/Xencaye/Lexia/issues/5): `layout.*` Win32-free (R18), `SortReadingOrder` declared (R28), `Utf8ToWide` to common.h and `GuessLanguage` on UTF-8 (R29).
- [x] **P0.5 Unit tests.** Add a test framework via vcpkg (doctest or Catch2) and `lexia_tests` with tests for `SortReadingOrder`, `GroupBlocks`, `BlocksToText`, `FindBlockAt`, settings parsing (bad values → default), `GuessLanguage`. Hook to `ctest`. *Done when:* `ctest -C Release` passes.
  - **P0.5a** [#7](https://github.com/Xencaye/Lexia/issues/7): doctest, `lexia_tests`, `ctest` hook, layout tests.
  - **P0.5b** [#8](https://github.com/Xencaye/Lexia/issues/8): settings parsing from a string, duplicate keys rejected (R35), `ParseHotkey`, `GuessLanguage`.
- [x] **P0.6 `lexia_cli` + fixture harness — the key task.**
  - **CLI**: `lexia_cli <image.png> [--lang en|bg] [--rect x,y,w,h] [--json out.json] [--draw out.png]` runs the same OCR + layout as the app. It writes the Blocks as JSON and an annotated image (boxes numbered in Reading order).
  - **Fixtures**: `tests/fixtures/<name>/{screen.png, expected.txt}`. A Fixture stores expected **Blocks only**, not Lines: `expected.txt` holds one Block per paragraph, separated by blank lines, in Reading order.
  - **Score**: `scripts/score_layout.ps1` runs every Fixture and prints a per-Fixture score, e.g. the normalised edit distance between produced and expected text, plus the Block-count error.

  *Done when:* the score runs in one command and gives a number per Fixture.
  - **P0.6a** [#9](https://github.com/Xencaye/Lexia/issues/9): `lexia_cli`, writing JSON, the annotated image, and `expected.txt`-format text on stdout.
  - **P0.6b** [#10](https://github.com/Xencaye/Lexia/issues/10): the Fixture score as a unit-tested pure function exposed by the CLI, `score_layout.ps1` with a baseline and tolerance, and one seed Fixture.
- [x] **P0.7 Logger (R14, R17, R21).** `log.h` with levels, writes to `%LOCALAPPDATA%\Lexia\lexia.log` and the console; replace `std::cout` diagnostics; paths printed as UTF-8. **Recognised text is never logged by default** (ADR-0005; logs may be attached to bug reports). *Done when:* the log file appears and rotates at \~1 MB. Issue: [#6](https://github.com/Xencaye/Lexia/issues/6).
- [x] **P0.8 CI (optional but cheap) (R31).** GitHub Actions `windows-latest`: vcpkg cache, `builtin-baseline`, build, `ctest`. OCR fixtures can run too if models are cached. *Done when:* PRs show a green check. Issue: [#11](https://github.com/Xencaye/Lexia/issues/11).

Your part in Phase 0: collect 15–30 screenshots where grouping goes wrong or right (browser article, game dialogue box, subtitles, PDF, menu, chat app) and write the text you *expected* to hear into `expected.txt`. That is the material the layout agent will optimise against. Issue: [#3](https://github.com/Xencaye/Lexia/issues/3) (`ready-for-human`).

## Workstreams and tasks

Each workstream lists the files it owns. Streams that own different files can run in parallel worktrees.

### A — Text grouping and reading order (owns `layout.*`; needs P0.5, P0.6)

Target Reading order is the order a person reads in: down each column before moving across. Today's code sorts in rows.

- [x] **A1 Baseline.** Run the fixture score on current code, commit the numbers to `docs/notes/layout-baseline.md`.
- [x] **A2 Failure taxonomy.** Agent looks at each annotated fixture image and classifies failures: columns merged, lines split, wrong order, UI chrome mixed in, single words dropped. Ranks by frequency.
- [x] **A3 Fix in order of frequency.** One failure class per PR. Typical levers: line-height–relative gap thresholds, column detection by x-overlap, left-alignment and font-height similarity, ignoring tiny isolated boxes. *Done when:* average score improves and no fixture gets worse by more than a set tolerance. Done for L1, L3 and L4 (Dist 0.1998 -> 0.1412, BlkErr 4.22 -> 2.39); L2 is deferred (`docs/notes/layout-failures.md`).
- [x] **A4 Parameters to settings.** Expose 2–3 tuning knobs (paragraph gap, column gap) in settings.json, hidden from the UI.
- [ ] **A5 Your check.** Re-test on Undertale, a browser and a video with subtitles.

### B — Speech engine (owns `speech.*`, new `tts/`; needs P0.4)

Piper replaces SAPI for all speech (ADR-0003, ADR-0004). Lexia plays its own audio. **Medium-quality
Piper voices only**: no Low/High models and no quality setting (a "lighter voices" toggle was
considered on Oct 25 and dropped for now).

- [x] **B1 Piper engine.**
  - Piper (`OHF-Voice/piper1-gpl`, espeak-ng) runs in-process via ONNX Runtime, replacing `SapiEngine`.
  - Audio goes through a sink interface, so B2 can be tested with a fake sink.
  - Expose phoneme/word timings, which B3 and G need.
  - Ship one Voice per Language that Lexia offers: English (ADR-0007: English only for now).
  - Each Reading is spoken Block by Block.

  *Done when:* `lexia_cli --say "text" --lang bg --wav out.wav` works. *Your check:* pick the English Voice by ear (this was F1), and confirm Bulgarian sounds right.
- [x] **B2 Reading queue: Pause, Skip, Stop** (needs B1).
  - New Picks join the end of the queue, and the queue plays across Sessions.
  - **Pause / Resume.**
  - **Skip**: next Block, then the next Reading. If paused, it resumes from there.
  - **Stop**: empties the queue.
  - Starting a Session while **playing** keeps the queue. Starting one while **paused** empties it.

  *Done when:* unit tests cover every rule, using the fake sink.
- [x] **B3 Announcements** (needs B2, I1).
  - Status messages spoken in the Interface language. An Announcement pauses the Reading at a word boundary, and the Reading resumes afterwards. If the queue was paused, it stays paused.
  - The newest Announcement replaces any that are waiting or playing. Skip ends only the Announcement, and Stop also cuts it off.
  - Events that produce one:
    - Language switched. Say the Language's own name in its own Voice.
    - No other Language installed.
    - A Pick found no text.
    - Screen capture failed.
    - A Voice is missing.
  - Starting or ending a Session is not announced.
- [x] **B4 Voice controls and Presets.** Settings per Voice:
  - **Speed**: `length_scale`.
  - **Pitch**: shifted after synthesis, e.g. with SoundTouch (LGPL; add it to the notices).
  - **Volume**.
  - **Expressiveness**: Piper's noise parameters, clamped to a safe range.

  Each has a Reset. **Presets** are named sets of these values, several per Voice. Stored in `settings.json`.
  Changed in D8 (Nov 3): Expressiveness is removed from the UI and the code, and a Preset is a file (save / load).
- [x] **B5 Voice library.**
  - **Download**: from Hugging Face `rhasspy/piper-voices` `voices.json`, filtered to Lexia's Languages and to Medium quality. Only on user request (ADR-0005).
  - **Add from file**: `.onnx` plus `.onnx.json`, or a zip of both.

  Both copy into `voices\`. Each Voice is attached to the Language matching its config's language code, and a Voice for a language Lexia can't read is refused. The UI is in D5.
- [x] **B6 Voice licences (Researcher).** Check every Voice Lexia ships. Dimitar's dataset is CC0, but the model was fine-tuned from the English *lessac* voice, and its model card gives no licence for the model itself. Record each Voice in the H2 notices.
  Findings: `docs/notes/voice-licences.md`. English: kristin, norman or ljspeech (medium). Dimitar can't ship as it is.

### C — App core and threading (owns `main.cpp`, new `app.*`; needs P0.4; do before D)

- [x] **C1 `App` class (R8, R15).** Move state out of `Run()`. Hotkeys, tray and Settings window call `App` methods. Owns the **On / Off** state, remembered across restarts. Off unregisters every key.
- [x] **C2 OCR worker (R9, R10).** Background thread, job queue, results via `PostMessage`. Overlay shows a small spinner during drag OCR.
- [x] **C3 Language switch.** Safe switch (R12) and hotkey clash handling (R13). The chosen Language is saved to `settings.json`. Switching during a Session re-runs the Analysis on the same Shot, and Picked areas stay. The switch stays in the C++ code, but Lexia 0.5 shows no Language key or control (ADR-0007); they come back in 0.8.
- [x] **C4 Live settings reload.** Watch settings.json (or reload on demand) and apply without restart.
- [x] **C5 Keys** (needs C1, B2).
  - **Read** F8, unchanged.
  - **Pause** F7.
  - **Speech** F6: tap = Skip, three taps within about 0.4 s of each other = Stop (D8; it was a double tap).
  - **Language**: unbound by default, and not shown in the Settings window until 0.8 (ADR-0007).
  - No quit key; quitting is done from the tray.

  The Pause and Speech keys are registered only while something is speaking or paused (ADR-0001). *Your check:* in Undertale, F6 and F7 reach the game when Lexia is silent.

### D — Tray, Settings window and Control bar (owns new `ui/`; needs C1 and an approved D0)

The C++ side of D2–D7 waited for your approval of D0 (Oct 28). D8 (Nov 3) replaced the D0 look; D2–D6 and D8 are merged into `main` (PR #31, Nov 5).

- [x] **D0 The `ui/` pages (Builder).** Real HTML/CSS pages for the Settings window, made from your wireframes in `wireframe\` (`home`, `voice`, `settings`, `help`, `overlay_large`, `overlay_mini` = Small) and opened in a browser with sample data (`scripts\ui_preview.ps1`). Figma is scrapped (Oct 28): the Figma call limit stopped that work, and the real pages are the design. On Oct 28 the owner asked for a redesign from scratch: the tabs and the Control bar keep the wireframe, the rest is placed freely, and Lexia 0.5 shows **no Language control at all** (ADR-0007). Where wireframe wording differs from the glossary, the glossary wins: "next" → Skip, "mood" → Expressiveness, "keybindings" → Keys, "Enable overlay" → On / Off, Mini → Small.
  - **Style**: neumorphism after the owner's reference image (soft raised cards that group tiles, rounded rectangles, colour only in thin rings and arcs), in neutral greys: light is almost white, dark is dark grey, no blue tone. Within these hard rules:
    - Text contrast ≥ 4.5:1. Toggle and other states ≥ 3:1 (WCAG 1.4.11) through fill, knob, word or bold, not through an outline (no strokes since Oct 28).
    - A visible focus ring on every control.
    - Atkinson Hyperlegible font.
    - Every screen in **light and dark**, in **English** (Bulgarian screens wait with the language, ADR-0007).
  - **Screens**:
    - **Home**: a large On / Off control (power icon, not a muted speaker), a reminder of the keys, and Report bug. No Language selector (0.8).
    - **Voice**: the Voice, and Add voice (Download / From file). No Language picker (0.8). Speed, Pitch, Volume and Expressiveness sliders, each with Reset. A Preset dropdown and Save preset.
    - **Settings**: Keys list (Read F8, Pause F7, Speech F6), each with a description and a silent captioned demo clip. Run at startup, Start minimised, Theme (System / Light / Dark), About. No Language key and no Interface language choice (0.8).
    - **Help**: a tutorial video.
    - **Voice download**: the catalogue, progress and a no-network error.
    - **Add from file**: success, plus the refusal "Lexia can't read German yet".
    - **Tray menu**: On/Off, Open Lexia, Quit.
    - **Installer pages** (H4): not part of the `ui/` pages. Inno Setup cannot show this style, so they are designed with H4.
    - **Control bar**: Large (icons and labels) and Small (icons only, tooltips like "Skip (F6)"); ◆ switches between them.
      - Buttons: Pause/Resume (⏸/▶ by state), Skip ⏭, Stop ■, Close ✕. No Language button (0.8).
      - States: Analysis running, idle (Pause/Skip/Stop greyed), speaking, paused.
      - A solid panel in the chosen theme, defaulting to top-centre and draggable.
  - **Your choices (Oct 28)**:
    - Neumorphism in light and dark, in rounded rectangles (changed from fully round on Oct 28). **No strokes** (owner, Oct 28): buttons and boxes are made only by shadows and gradients, with no black or white outline. A state never shows with shadow alone (a toggle also has a knob and the word On or Off; the selected tab is pressed in, Bold and in the accent; a disabled control is flat and faded).
    - One accent for controls: purple `#5e4fa2` (a lighter tint in dark). The Spectral palette (`wireframe\palette.png`) is only a small rainbow detail: the ring on the power button when Lexia is On, the progress of a Voice download and the Voice control gauges. The selected tab has no strip: it looks pressed in (Oct 28).
    - Icons: Fluent Emoji, 3D style (MIT) (changed from flat on Oct 28). *Replaced in D8.1 (Nov 3) by the Stratis UI line icons (CC BY 4.0); the emoji files are removed.*
    - Home On / Off is a large round power button, with the word On or Off under it.
    - Theme setting: System / Light / Dark. The default is System, which follows Windows.
  - **What D0 gives**:
    - `ui/index.html`, `css/`, `js/`, the font and the icons, and the page's one message contract (the bridge, in `ui/README.md`). A mock host gives sample data in a browser.
    - HTML previews of the Control bar (`ui/dev/controlbar.html`) and the tray menu (`ui/dev/tray.html`) as the spec for D4 and D2. The real ones are Win32 (ADR-0006).
    - The bridge tests (`ui_bridge` in ctest, headless Edge), and `scripts\ui_contrast.ps1` for the contrast of the colour pairs.
    - Screenshots of every screen and state in light and dark: `.\scripts\ui_preview.ps1 -Screens`.
  - **D3 later**: packages `ui/` and not `ui/dev/`, and adds the notices entries for Atkinson Hyperlegible Next (OFL) and the icons (`docs/third-party.md`; Fluent Emoji (MIT) at that time, the Stratis UI icons (CC BY 4.0) since D8.1).

  *Done when:* you approve the pages in the browser preview, from the screenshots. **Approved by the owner on Oct 28, 2026.**
- [x] **D1 Decision: native Win32 or a separate shell.** Decided: the Settings window is HTML/CSS in **WebView2** inside the C++ process. The Overlay and Control bar stay plain Win32 (ADR-0006).
- [x] **D2 Tray icon.** On/Off, Open Lexia, Quit. `WIN32` subsystem (R7), manifest (R4).
- [x] **D3 Settings window shell + Home tab.**
  - WebView2 with the `ui/` pages from D0 (neumorphic CSS, light/dark, bundled assets), destroyed on close. Package `ui/` but not `ui/dev/`, and take the `dev/dev_host.js` line out of `index.html` (`ui/README.md`). Add the notices entries for Atkinson Hyperlegible Next (OFL) and the icons (Fluent Emoji (MIT) at that time; since D8.1 the Stratis UI icons (CC BY 4.0) and the engravings).
  - The C++ side of the bridge in `ui/README.md`.
  - **Home**: On / Off and the keys reminder.
  - **Report bug** opens a pre-filled GitHub issue (Lexia and Windows versions only; logs are never attached automatically).
- [x] **D4 Control bar on the Overlay.**
  - Replaces the hint bar.
  - Large/Small size, remembered. Movable, top-centre by default, with the position remembered.
  - Shows the Analysis status. Close ends the Session.
  - Clicks and drags that start on the bar are never Picks. The bar exists only during a Session, and the Overlay stays `WS_EX_NOACTIVATE`.
  - Rename the selector to Overlay in code (`RunSelector`).

  *Your check:* in Undertale, using the bar doesn't minimise the game. Do bar clicks also reach the game? Note which.
- [x] **D5 Voice tab.** UI for B4 and B5. Lists and downloads Voices only for the Languages Lexia offers (`Language::offered`, ADR-0007): the host sends only those, and the page has no Language picker. The B5 core still knows Bulgarian.
- [x] **D6 Settings tab.**
  - The Keys editor with descriptions and demo clips.
  - Run at startup and start minimised.
  - Theme: System / Light / Dark (a new `theme` setting). The Interface language choice and the Language key come in 0.8 (ADR-0007).
  - About: version, GPL-3.0-or-later and the third-party notices.
- [x] **D9 Start options** (owner, Nov 5). A **start minimized** switch between run at startup and run as admin: on by default, drawn lighter and unusable while run at startup is off; on = the Run value gets `--minimised` (a start at logon stays in the tray), off = a start at logon opens the window. A manual start, and the first start after the installer, always open the Settings window, which now comes to the front (also after the elevated relaunch: `AllowSetForegroundWindow`). Every start writes the Run value again (this exe's path, the switch as the setting says). On / Off is **Off** on the very first start (no settings.json) and remembered after that.
  *Your check:* a fresh start (delete `%LOCALAPPDATA%\Lexia\settings.json`) is Off and opens the window; turn On, Quit, start again: still On; run at startup with start minimized on and off, then sign out and in.
- [x] **D10 Read the key card aloud** (owner, Nov 5). The key card's play button on the Settings page reads that key's description aloud (`readKeyCard { action }`: the host speaks `window.keys.<action>Desc` from the string table, one Block per line, in place of what was being spoken), so a person hears what a key does before Lexia is even On. It replaces the per-key demo clips of D7.
  *Your check:* select each key, press play, with Lexia Off and On.
- [ ] **D7 Help tab and videos.** A tutorial: silent, with captions in the Interface language, bundled. This replaces the old first-run welcome. (The per-key clips are replaced by D10.)
  - Status (D6 builder): the Help tab and its pages are in the window, and the `ui/img` and `ui/captions` folders are packaged as they are (`cmake/ui_files.cmake` copies all of `ui/` except `dev/` and `README.md`). The videos are the owner's content and are still to come; the page shows its poster and a placeholder caption until then.
- [x] **D8 UI overhaul (paper design).** The owner's new design (`wireframe/*.png`, Nov 3) replaces the D0 look of the Settings window and the Control bar. Merged into `main` (PR #31, Nov 5; branches D8.1 pages, D8.2 host, D8.3 core and wiring, D8.4 Control bar).
  - Six pages: home, voices, settings, and report, help, about. The window is a fixed 800 x 600 DIPs.
  - New features: Master volume, Run as admin (ADR-0008), dark mode from Windows, left-handed mouse, Presets as files, Delete voice.
  - Removed: Expressiveness, the Start minimised setting (the Run value now gets `--minimised`), and the Close button of the Control bar.
  - Icons: Stratis UI (CC BY 4.0). Pictures and the logo: public-domain engravings.

  - Fixes after the owner's review (Nov 5): a bold menu word keeps its room, so the other menu items do not move (`components.css`, `data-bold-room`); the About text goes around the engraving (`shape-outside`, measured on `wireframe/about.png`, and 14 px as in the design); the Control bar's Pause button is as wide as the word it shows, so every gap is 18 px as in the mockup (the bar gets wider when Pause becomes Resume and keeps its left edge); the Master volume moves in steps of 5 %, like the Voice controls. The bar's colours stay: the light theme already matches the mockups, and the dark one waits for the owner's palette. Second About mockup (Nov 5): the second block and the icons 33 px higher, new line edges (27, 23, 15 / 11, 7, 0 px), and a smaller two-line notice whose "here" opens THIRD-PARTY-NOTICES.txt; the package step also copies that file and LICENSE next to the dev exe, so the link works from `build\Release` too.

  *Your check:* the by-hand tests, the final dark palette (the current one is temporary), and the 6 colour pairs of the design that fail `ui_contrast.ps1`.

### E — Bulgarian (owns `language.*`; needs P0.6, B1 for E5) — on hold until 0.8

On hold since Oct 28 (ADR-0007): Lexia 0.5 offers English only. Language support is planned for Lexia 0.8. E3–E5 are done and stay in the code; Bulgarian is in the Language table but not offered. E1 and E2 wait until Bulgarian is brought back.

- [ ] **E1 Cyrillic fixtures** (on hold). 10+ Bulgarian screenshots with expected text.
- [ ] **E2 OCR quality** (on hold). Score `rec_cyrillic.onnx`; note confusions.
- [ ] **E6 The owner's Bulgarian Voice (owner).** The owner records their own voice and trains a Bulgarian Medium Piper Voice from it (decided Nov 5, 2026; it replaces Dimitar, whose model has no clear licence). Train from the clean Piper base (`rhasspy/piper-checkpoints/_base_model`, CC BY 4.0), not from a lessac-derived checkpoint. *Done when:* the Voice is in `voices/voices.json` with `ship: true`, it has a licence entry `licenses/components/voice-<name>.txt` (`docs/third-party.md`), and `lexia_cli --say "..." --lang bg --wav out.wav` speaks with it. *Your check:* listen to it.
- [x] **E3 Alphabet lock.** When decoding, ignore characters outside the current Language's Alphabet (its own letters plus Latin, digits and punctuation). For Bulgarian this removes ы, э, ё, ђ, і, ў. *Done when:* unit tests show no out-of-Alphabet output.
- [x] **E4 Lookalike fix.** In a word that mixes scripts using lookalike letters (о/o, с/c, р/p, А/A…), convert them to the word's majority script. Pure text logic, unit-tested.
- [x] **E5 Voice per script stretch.** The recogniser stays the Language the user chose. The *Voice* is picked per stretch of one script: the Voice of the Language that script matches, but only for stretches of two or more words. A single word stays in the surrounding Voice. This replaces the old "auto language" task.

### F — Neural voice prototype — closed

Superseded by the Oct 25 decisions: Lexia is GPL-3.0-or-later (ADR-0003), and Piper replaces SAPI (ADR-0004). F1's listen test became your check in B1, F2 is decided, and F3 became B1.

### G — Read-along highlighting (Lexia 0.6; needs B1, C2, D4)

- [ ] **G1** Map word timings from Piper (B1) back to OCR line boxes; draw the current word on the overlay.
- [ ] **G2** Setting to keep the overlay up while reading for highlighting. (The Control bar currently exists only during a Session; revisit together.)

### H — Distribution (owns `cmake/package.cmake`, new `installer/`)

- [x] **H1 Version 0.5.0** from `vcpkg.json` into the exe resources, README and About. `vcpkg.json` currently says `0.6.0`; you confirm which is right before this lands.
- [x] **H2 Licences.** A `LICENSE` file (GPL-3.0-or-later) and third-party notices: ONNX Runtime, OpenCV, nlohmann-json, PaddleOCR models, Piper, espeak-ng, SoundTouch, WebView2 SDK, Atkinson Hyperlegible (OFL), and every shipped Voice (B6).
- [x] **H3** Optional signing step, off by default.
- [x] **H4 Installer: Inno Setup** (decided), replacing `Lexia.zip`. Pages from D0: light or dark, run at startup, install location (an Interface language page only in 0.8, ADR-0007). It bundles the models of the offered Languages, Voices, notices, and WebView2 bootstrap handling for Windows 10.
  - Status (Nov 5): the script `installer/lexia.iss` and the CMake option `-DLEXIA_INSTALLER=ON` (`cmake/installer.cmake`). Per-user by default (all users offered), Run at startup as a task, Start Lexia on the last page (its window opens, Lexia is Off: D9), the WebView2 Runtime installed when it is missing (a task, on by default, that runs Microsoft's Evergreen Bootstrapper carried in the installer; it downloads the runtime from Microsoft, so the task says so and can be turned off: ADR-0005; the last page then offers Microsoft's page), an update closes nothing by force (AppMutex), the uninstaller removes the Run value and asks about the user's data. Built and tested on Nov 5: a silent install into a test folder (451 files, the Apps entry, the Start menu shortcut) and a silent uninstall (all gone, the user's data kept); the uninstaller removes the Run value only when it starts the installed lexia.exe, so a value of another Lexia (a portable copy, a developer build) stays. Its pages follow Windows' light / dark mode (`WizardStyle=modern dynamic`). Still open: the owner's check of the pages, signing the installer (`-DLEXIA_SIGN`), and a CI job. The first start after the install is Off and opens its window (D9, merged).

### I — Interface language (owns new `strings/`; needs P0.4)

- [x] **I1 String table.** All of Lexia's own text goes through one table: Announcements, Control bar, Settings window, settings messages, installer. English and Bulgarian; the Interface language is chosen at install (English preselected) and in Settings, separately from the Language. Adding a language means adding one table. Since Oct 28 only English is offered (ADR-0007); the Bulgarian table stays and is tested. The choice of Interface language comes in 0.8.

### J — Linux on Ubuntu (Lexia 1.0; owns new `platform/linux/`; needs the 0.8 code)

Decided Nov 5, 2026: Lexia 1.0 also runs on Ubuntu (the current LTS, x64). Windows stays the first platform.
The core is already portable: OCR (ONNX Runtime, OpenCV), layout, Piper and espeak-ng, the Reading queue,
settings, strings and the Win32-free widget tree of the Settings window (`src/native_ui/`). What is Win32 today is the shell around it: the hotkeys, the screen
capture, the Overlay and the Control bar, the tray, the audio sink, the Settings window (Direct2D since K; J5), and the
start-up, single-instance and elevation code. The rules stay: the Overlay never takes focus (ADR-0001), no
network except on request (ADR-0005), GPL-3-compatible dependencies only.

- [ ] **J0 Decision: X11, Wayland, or both (ADR).** Ubuntu's default session is GNOME on Wayland. Wayland lets
  no program take global keys, read the screen or show a window that never takes focus without the
  desktop's help: the screenshot portal asks the user each time, and GNOME has no layer-shell. X11 (and
  games that run under XWayland) allows all three. Decide what 1.0 supports and what Lexia does where it
  cannot work. *Done when:* an ADR records it, with a test on the owner's Ubuntu PC.
- [ ] **J1 Platform seam.** Put the Win32 parts behind small interfaces (keys, capture, overlay window, tray,
  audio sink, web view, autostart, single instance) with the Windows code unchanged behind them, so a
  Linux build adds files and changes no core code. No behaviour change on Windows (fixture score and tests
  the same).
- [ ] **J2 Linux build and CI.** CMake + vcpkg on Ubuntu (GCC or Clang, no MSVC-only flags), `lexia_core`,
  `lexia_tests` and `lexia_cli` first; a CI job on `ubuntu-latest` next to the Windows one. *Done when:*
  `ctest` and `score_layout` pass on Linux (the Fixtures give the same Blocks).
- [ ] **J3 Keys and capture.** Global keys and the Shot on Linux, as J0 decides (X11: `XGrabKey`, `XGetImage`
  / XShm; Wayland: the portals, or a clear message that it is not supported).
- [ ] **J4 Overlay and Control bar.** A window over the frozen Shot that does not take focus from a full-screen
  game, and the Control bar in it. *Your check:* a game on Steam for Linux / Proton does not minimise.
- [ ] **J5 Audio, tray and Settings window.** The audio sink (PipeWire or PulseAudio), the tray icon
  (StatusNotifierItem / AppIndicator; GNOME needs the AppIndicator extension, which Ubuntu ships), and
  the Settings window. Since K (ADR-0009) there are no `ui/` pages to reuse: the window reuses the
  Win32-free widget tree of `src/native_ui/` with a Cairo/Pango canvas and AT-SPI, or is a new window.
- [ ] **J6 Start-up and settings.** Settings and the log in `~/.config/lexia` and `~/.local/state/lexia`
  (XDG), Run at startup as a `~/.config/autostart` entry, one instance only. Run as admin has no Linux
  meaning and is not shown there.
- [ ] **J7 Package.** A `.deb` for Ubuntu (and maybe a Flatpak or AppImage, decided in J0), with the models,
  the shipped Voices and the third-party notices.
- [ ] **J8 Your check.** The whole of Lexia on your Ubuntu PC: a browser, a video with subtitles, and a game.

### K — Native Settings window (Lexia 0.6; owns `settings_window.*`, new `src/native_ui/`; replaces `ui/`)

The owner decided on Nov 7 that the Settings window becomes a native Win32 window in the D8 paper design,
drawn with Direct2D and DirectWrite, with full UI Automation; WebView2 and `ui/` are removed (ADR-0009,
supersedes ADR-0006). Sub-branches `feat/K.N` merge into `feat/K-native-settings`, then one PR into `main`.

- [x] **K0 Reference and ADR.** The HTML pages' screenshots (every page, state and sheet, light and dark) in
  `wireframe/html-reference/`, the visual spec after `ui/` is gone. ADR-0009.
- [x] **K1 Toolkit and assets.** Icons, pictures and fonts (Atkinson Hyperlegible Next TTF) as resources in the
  exe; one palette for the window and the Control bar (the contrast check becomes a doctest); the Win32-free
  widget tree with a `Canvas` interface (tested with a fake canvas); the Direct2D canvas; a shared tooltip.
  *Done so far (`feat/K.1b`):* the widget tree and every control (`src/native_ui/`: `Widget`, `Screen`, sheets,
  `IconButton`, `TextLink`, `Button`, `Select`, `Slider`, `SwitchRow`, `KeyChip`, `Notice`, `ProgressBar`,
  `ScrollArea`, `Label`, `Paragraph`, `Tween`), with 96 doctest cases on a fake canvas. `feat/K.1a`: assets as
  resources, the fonts, one palette, the shared tooltip. `feat/K.1c`: the Direct2D canvas (`D2dCanvas`, window and
  off-screen) and the `lexia_settingsshot` tool with the `gallery` page and ctest `settingsshot_smoke`; one
  `nui::FontWeight`. Nothing in K1 is open once K.1c is merged.
- [x] **K2 Window shell and simple pages.** The native window (800 x 600 DIPs, DPI, theme), the frame, Home,
  Help, Report, About; `App` sends `UiState` and gets `PageMessage` with no JSON; the pages and `--sheet` of `lexia_settingsshot`.
  *Done (`feat/K.2`):* `settings_window.*` is native (Direct2D, no WebView2 in it; the library stays in CMake until K6);
  `SettingsUi` + `pages/` (`Frame`, `HomePage`, `HelpPage`, `ReportPage`, `AboutPage`; `VoicesPage` and `SettingsPage` are
  empty places for K3 / K4); `App::HandlePageMessage(const PageMessage&)`, `pageReady_` and `window.noRuntime` gone;
  `lexia_settingsshot --page home|voices|settings|report|help|about [--on] [--hover]` and `scripts\settings_shots.ps1`
  (the `--sheet` switch comes with the sheets in K3); `native_ui_settings_ui_test.cpp` (14 cases). Compared with
  `wireframe/html-reference/` (light and dark): positions within 1 DIP; the dots of a dotted rule start one pixel later.
- [x] **K3 Voices page and its dialogs.** Voice row, sliders, presets; Delete voice, Voice download, Result.
  `VoiceTab` gets typed events.
  *Done (`feat/K.3`):* `voices_page.*` (the Voice `Select`, Download / Add from file / Delete, the "no voice" note, Speed
  (log2) / Pitch / Volume sliders with value text and reset buttons, the otter, Save / Load preset), `voice_sheets.*`
  (Delete voice, `DownloadSheet` with the views loading / failed / empty / list, one download at a time, scroll offset and
  focus kept, progress without a rebuild; `ResultSheet`), `voice_library_state.*` (the sheets' data, owned by `SettingsUi`,
  so a download is known while another page shows), `voice_text.*`; `voice_events.*`: `VoiceTab::Host::toPage(JSON)` is
  replaced by `Host::events` -> `VoiceEventSink` (typed), `ProblemTextId` is the port of the old `problemText`, the
  offered-Language filter and the refusal facts are typed (the JSON builders in `ui_host.cpp` stay until K6).
  Toolkit: `Sheet` border inset, wrapping title, title gap, per-line gaps, max height + flexible body, `Notice::Kind::Plain`,
  notice line 1.35; `user-profile-03.svg` without an SVG mask. `lexia_settingsshot --voices` / `--sheet` and 25 pictures in
  `scripts\settings_shots.ps1` with the reference file names; `native_ui_voices_test.cpp` (25 cases),
  `voice_events_test.cpp` (4 cases). Compared with `wireframe/html-reference/` (light and dark): positions within 1-2 DIPs;
  a scrolling download list leaves 10 DIPs for its scrollbar (the HTML shots had none); a few words wrap one word
  differently (DirectWrite and the browser measure a glyph a little differently).
- [x] **K4 Settings page.** Keys, key card, switches; key capture from the window's own key messages.
  *Done (`feat/K.4`):* `pages/settings_page.*` (rows, key card, play button, hint, five switches), `key_chord.*`
  (`KeyChordName`, pure), the capture in `settings_window.cpp` (F10, Alt combinations and PrintScreen reach a waiting
  chip; no hook, nothing logged; Alt+F4 still closes), `SettingsUi::ShowPage(Page, ShowOptions)`,
  `lexia_settingsshot --select / --capture / --switches` and the five `settings-*` shots of `settings_shots.ps1`;
  `native_ui_settings_page_test.cpp` and `key_chord_test.cpp` (32 cases). Compared with `wireframe/html-reference/`
  (light and dark): positions within 1 DIP; the card text and the switch labels sit 1 DIP lower than in the browser.
  The owner's by-hand check of the capture (F-keys, Alt and Ctrl combinations, F10, PrintScreen; Lexia's keys work
  again after a capture) is part of K7.
- [x] **K5 UI Automation.** A provider per control, its patterns and events; ctest `uia_tree`.
  *Done (`feat/K.5`):* `native_ui/access_tree.*` (pure: the accessible projection of the widget tree, the modal sheet
  that hides the page, `CheckAccess` / `PerformAccess`, `AccessTracker` + `AccessObserver` for focus, property,
  structure and live-region changes), the action hooks on the widgets (`AccInvoke` / `AccSetValue` / `AccExpand` /
  `AccSelect`), automation ids on the main controls, `native_ui/uia_provider.*` (`UiaHost` + one COM `Element`: root
  fragment, one fragment per offered widget, Invoke / Toggle / RangeValue / Value / ExpandCollapse / Selection /
  SelectionItem / ScrollItem / Window, the events, `UseComThreading`, a shared `alive` context cleared on
  `WM_DESTROY`, actions queued to the message loop), `SettingsWindow` forwards `WM_GETOBJECT` and the rest;
  `native_ui_access_test.cpp` (18 cases) and the UIA client exe `lexia_uia_test` (ctest `uia_tree`: skips with exit 0
  when there is no interactive desktop). A switch is a Button + Toggle with the control type text "switch" (strings
  `uia.switch`); a slider offers its number (RangeValue), not its value text. **K7 must check with Narrator:** see below.
- [x] **K6 Remove WebView2 and `ui/`.** CMake, vcpkg, installer (no runtime task), notices, scripts, docs.
  *Done (`feat/K.6`):* the assets moved to `src/res/{icons,img,fonts}` (still embedded as resources; nothing is read from
  disk); `ui/` (HTML, CSS, JS, `dev/`, the bridge README), `cmake/copy_ui.cmake`, `cmake/ui_files.cmake`, the package's
  `ui\` copy, ctest `ui_bridge`, `scripts\ui_preview.ps1` and `scripts\ui_contrast.ps1` are deleted (`palette_test` is the
  contrast check); the `webview2` vcpkg dependency, `WebView2Loader.dll`, the `webview2` and `wil` notices entries and the
  WebView2 and WIL rows of `docs/third-party.md` are gone; the installer has no WebView2 task, bootstrapper, registry
  check or download link (and `scripts\fetch_deps.ps1 -Installer` with `installer\redist` are gone; it still removes an
  old `{app}\ui` and `*.dll` on an update); dead code: `StringsMessage`, `StateMessage`, `ParsePageMessage`, the JSON
  builders of `ui_host.*` and `lexia_cli --strings-json` with their tests (`ui_host.*` keeps `UiState`, `PageMessage`,
  `LinkUrl`, `IsDarkTheme`, `FillVoiceState`; the name is older than its content); `stale_data.*`: at start the old
  `%LOCALAPPDATA%\Lexia\WebView2` folder is removed once, a junction or link inside is never followed (`stale_data_test.cpp`,
  4 cases). ADR-0006 stays as a superseded record.
- [ ] **K7 Your check.** Every page and dialog against the HTML shots, keys and mouse, Narrator (the K5 list: turn
  Narrator on with Win+Ctrl+Enter; the Settings window opens at the window title; Tab through the home page and hear
  each name and role; the power button says "Lexia On / Off" with its state, and Space or Enter changes it; the
  six links read as links and the current page as selected; the Master volume and the Voice sliders say their name and
  number, and the arrow keys change them; on the Voices page the Voice choice says its value and opens with Alt+Down
  or Enter, the arrow keys read each Voice and Enter chooses; Delete voice opens a dialog that is read by its title,
  and the page under it cannot be reached (Narrator scan mode stays inside it) until Esc; on the Settings page each
  key says its action and key, a press starts the capture and the hint "Press the new key..." is read at once, the five
  switches read as "switch" with on or off, and a disabled Start minimised says it is unavailable; Narrator's
  caps lock + F5 / Insert + Space actions work on every button; the download sheet's rows are read and the
  list scrolls to the focused one; if a switch is read badly try CheckBox in `ControlTypeOf`), dark mode,
  150 % / 200 % and a move between monitors. After K6 also: install `Lexia-0.6.0-setup.exe` on a PC without the
  WebView2 Runtime (no runtime task is shown, the window opens), an update over Lexia 0.5 (the old `ui\` folder and
  `WebView2Loader.dll` are gone from the install folder, the `WebView2` folder in `%LOCALAPPDATA%\Lexia` is removed at
  the first start and the log says so; settings.json, Voices and Presets stay), and the uninstaller (the program, the
  Run value; it still asks about `%LOCALAPPDATA%\Lexia`).
- [x] **K8 The owner's changes after the first look (Nov 7).** On `feat/K.7-fixes`: (1) the key card's read button
  is the Master volume's speaker icon, left of the title, no ring, and it wiggles a little every 5 s (none with reduced
  motion; a one-shot wake timer between wiggles, frames only while it moves; `Canvas::PushRotation`); (2) loading a
  Preset follows the file's Voice: a Voice that is not installed changes nothing and says so ("The voice … is not
  currently installed"), another installed Voice gets the controls without becoming active, an old file without a
  Voice goes to the active one (`ChoosePresetTarget`, `PresetVoiceForDisplay` for the untrusted name); (3) Setup and
  the uninstaller close a running Lexia on request (the `Lexia.Quit` message, let through UIPI so an elevated Lexia
  hears it too; after 10 s an offer to end it by force, with one UAC prompt for an elevated one; silent runs never ask);
  (4) a "Run as administrator" task in Setup, checked when the setting is on, written with
  `lexia.exe --set-run-as-admin on|off` as the installing person. *Your check:* the K8 items in the PR's test list.

## Order and milestones

Phase 0 comes first. After it, A, B, C, E and I can run as parallel agents because they own different
files. D0 (the `ui/` pages) can start at any time; the C++ side of D2–D7 and the H4 pages wait for your approval of it.

```
Phase 0 ──► A (layout) ─────────────────────────────────────────────┐
        ├─► B1 Piper ──► B2 queue ──► B3 Announcements, B4, B5      │
        ├─► C1 App ──► C2–C5                                         ├─► Lexia 0.5
        ├─► E1–E4 Bulgarian OCR; E5 after B1                         │
        ├─► I1 strings                                               │
D0 ui/ pages ──(you approve)──► D2–D7, H4 ──────────────────────────────┘
                                         0.6: G read-along
                                         0.8: E1, E2, E6, Language UI
                                         1.0: J Linux (Ubuntu)
```

Milestones, each ending with your hands-on check:

1. **M1 Agents can verify alone** (done) — Phase 0 done; build is warning-free, tests and fixture score run in one command.
2. **M2 Grouping you trust** — A done; fixture score up, Undertale and browser re-tested.
3. **M3 Right voice per language** — B1, B6, E done; Piper speaking English and Bulgarian, Bulgarian OCR scored and locked to its Alphabet. *Since Oct 28 (ADR-0007): English only; the Bulgarian part waits with E1, E2 and a Bulgarian Voice.*
4. **M4 Lexia 0.5** (released Nov 5, 2026 as 0.5.0; carried over: the D7 video, the D8 checks, A5) — B2–B5, C, D, H, I done; installer, tray, Settings window, Control bar, Reading queue, Announcements, Voice library.
5. **M5 Lexia 0.6: Read-along** — G done; K, the native Settings window, built (K0 to K6); your K7 check is open (owner, Nov 7).
6. **M6 Lexia 0.8: Languages** — E1, E2, the owner's Bulgarian Voice (E6), the Language selector, the Language key, the Control bar Language button, and the Interface language choice (Settings and installer).
7. **M7 Lexia 1.0: Linux** — J done; Lexia runs on Ubuntu as on Windows, within what J0 decides for Wayland.

## Decisions (Oct 25, 2026)

From a design grilling. Terms are in `GLOSSARY.md`, and the reasoning is in `docs/adr/`.

- **Lexia is free and open source, GPL-3.0-or-later** (ADR-0003). Option 1 of the old F2 list.
- **Piper replaces SAPI** for all speech; the SAPI-plus-OneCore route is rejected (ADR-0004).
- **The Overlay never takes focus; input comes from global hotkeys only** (ADR-0001). Low-level input hooks were rejected because Lexia will be used over online games with anti-cheat. Unregistered keys and raw-input clicks still reach the game; that's accepted for now.
- **PaddleOCR stays** (ADR-0002): Tesseract and Windows OCR did poorly on stylised, rendered text.
- **Network only when the user asks, and the screen never leaves the PC** (ADR-0005).
- **Settings window in WebView2** (ADR-0006).
- **The Interface language is separate from the Language**: English and Bulgarian, chosen at install.
- **Inno Setup installer.**
- **Figma mockup before any UI code.** (Replaced on Oct 28: see below.)

## Decisions (Oct 28, 2026)

- **English only for now** (ADR-0007): Lexia reads English and speaks its own words in English.
  Bulgarian stays in the code but is not offered; more languages remain a goal. E1 and E2 are
  on hold.
- **No Language UI in 0.5; language support in 0.8.** Lexia 0.5 shows no Language selector, list,
  key or Control bar button. The C++ Language switch stays. Language support is planned for 0.8 (M6).
- **D0 redesign.** The `ui/` pages are made again from scratch: the tabs and the Control bar follow the
  wireframe (the bar at the top centre), the look follows the owner's reference image in neutral greys,
  with Fluent Emoji 3D icons and the Spectral palette. (Replaced by the D8 paper design on Nov 3: line icons and engravings, no emoji.)
- **Figma is scrapped; D0 is the real `ui/` pages.** The Settings window is HTML/CSS (ADR-0006), so
  the mockup is the real page. You check it in a browser and send screenshots with edit requests. The
  C++ side of D2–D7 starts after you approve it.

Background, kept for reference: the Bulgarian Piper voice comes from the CC0 "Dimitar" dataset. The
original MIT `rhasspy/piper` was archived on 6 Oct 2025, and development continues as
`OHF-Voice/piper1-gpl` (GPL-3.0-or-later), because espeak-ng is GPL-3.0. The MIT fork `piper-plus`
drops espeak-ng but doesn't cover Bulgarian.

## Decisions (Nov 5, 2026)

- **Linux on Ubuntu in Lexia 1.0** (stream J, milestone M7). Windows stays first; the Wayland question is J0.
- **The owner trains the Bulgarian Voice** from their own voice (E6, Lexia 0.8); the Dimitar retrain is removed.

## Open questions

- [ ] Did row-based reading order fix the scrambled text? (Becomes fixture data either way.)
- [x] Is Lexia going to be open source? **Yes, GPL-3.0-or-later.**
- [x] Is the code already in git, and do you want CI on GitHub Actions? **Yes: private repo `Xencaye/Lexia`; CI is P0.8 ([#11](https://github.com/Xencaye/Lexia/issues/11)).**
- [x] Settings window: Win32 C++ or a separate C# app? **WebView2 in-process (ADR-0006).**
- [x] Free forever, donations, or paid later? **Free.**
- [ ] Target hardware for friends' PCs: any very old CPUs? Affects ONNX Runtime build choice, OCR speed budget and Piper voice quality (medium vs low).
- [x] Version: 0.5.0 (this plan) vs `0.6.0` in `vcpkg.json`? **0.5.0** (owner, Oct 26).

## Sources

- [Dimitar 1.0 Bulgarian TTS dataset — Mozilla Data Collective](https://prod.datacollective.mozillafoundation.org/datasets/cmhpahaib00d8mk07ely2m8wh)
- [piper-voices-bg-bg — AUR](https://aur.archlinux.org/packages/piper-voices-bg-bg)
- [piper-voices voices.json — Hugging Face](https://huggingface.co/rhasspy/piper-voices/raw/main/voices.json)
- [bg_BG dimitar MODEL_CARD — Hugging Face](https://huggingface.co/rhasspy/piper-voices/raw/main/bg/bg_BG/dimitar/medium/MODEL_CARD)
- [Piper TTS review (licence and repository history) — PromptQuorum](https://www.promptquorum.com/power-local-llm/piper-tts-review)
- [piper-plus (MIT fork, espeak-ng-free) — GitHub](https://github.com/ayutaz/piper-plus/)

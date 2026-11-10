# Lexia — Plan

@Eleonora · started Oct 23, 2026 · cleaned up Nov 8, 2026 (the finished tasks are one line each; their full
records are in git history and in the PRs)

Every term below is defined in `GLOSSARY.md`, and every decision with a trade-off is in `docs/adr/`.
On-screen wording follows the glossary.

## Status (Nov 8, 2026)

- **Released:** Lexia 0.5.0 (Nov 5, tag `v0.5.0`) and **Lexia 0.6.0** (Nov 8, tag `v0.6.0`:
  `Lexia-0.6.0-setup.exe` and `Lexia-0.6.0-portable.zip`, kept in `release\0.6.0\`). **Lexia 0.6.1** (Nov 8, tag `v0.6.1`,
  made from `v0.6.0`, files in `release\0.6.1\`): the same code with a corrected package `README.txt` (the
  0.6.0 one still described the console prototype). **Lexia 0.6.2** (Nov 8, tag `v0.6.2`, made from `v0.6.1`, files
  in `release\0.6.2\`): the same code with the new Discord invite (`https://discord.gg/vPsjF5NzPG`; the old one was
  broken). Main has the same invite.
- **Now:** towards **Lexia 0.7** (owner, Nov 8: future changes go into 0.7). `vcpkg.json` says 0.7.0 (PR #12).
  Done for 0.7: B8, the Voices Cori (the default) and Mike instead of ljspeech; B9, an update keeps a Voice that Lexia
  no longer ships (owner, Nov 10): it becomes one of the user's own Voices.
- **Open, the owner's:** the by-hand checks (below), D7 (the tutorial video), A5 (games, browser, video).
- **Later:** E1, E2, E6 and the Language UI (Lexia 0.8); J, Linux on Ubuntu, and H5, signing (Lexia 1.0).
- No GitHub issue is open.
- **History reset (Nov 5):** `Xencaye/Lexia` restarted from one commit. PR and issue numbers from before the reset
  (the old #1–#42) and the tag `v0.5.0` are in the private repo `Xencaye/Lexia-archive`. PR numbers #1–#12 in this
  plan are in the new repo.

## How we work

- One task = one branch (`feat/<task-id>`) = one PR. Stop at the task boundary and summarise: what changed, how it
  was verified, what the owner must check by hand.
- Every task has checks an agent can run itself: a Release build with zero warnings, `ctest`, and for layout work
  `score_layout.ps1` (no Fixture worse than the tolerance).
- The owner is the eyes and ears: anything visual, audible, or inside a game is on the owner's list.
- A change that implements or changes a task updates this plan and the "Plan" section of `CLAUDE.md` in the same PR.
- New and changed code is commented (the why, the non-obvious). C++ is navigated with clangd. Code plans use the
  Security Guidance plugin and have a Security section.
- Before each merge: `.\scripts\check_docs.ps1` passes (CI: `.github/workflows/docs.yml`), and the PR template's
  docs checklist is done.

## The owner's open checks

These are the by-hand checks of merged work. Each item is short; tick it here when it is done.

- [ ] **K7 Settings window.** Every page and sheet against `wireframe/html-reference/` (layout) and the mockups
  `wireframe/<page>.png` / `<page>_dark.png` (colours); keys and mouse; dark mode; 150 % / 200 % and a move between
  monitors. Key capture: F-keys, Alt and Ctrl combinations, F10, PrintScreen; Lexia's keys work again after a capture.
  **Narrator** (Win+Ctrl+Enter): the window opens at its title; Tab through the home page and hear each name and role;
  the power button says "Lexia On / Off" with its state, Space or Enter changes it; the six links read as links, the
  current page as selected; the Master volume and the Voice sliders say name and number, the arrow keys change them;
  the Voice choice says its value and opens with Alt+Down or Enter; the Delete voice dialog is read by its title and
  the page under it cannot be reached until Esc; on the Settings page each key says its action and key, a press starts
  the capture and the hint is read at once; the switches read as "switch" with on or off; a disabled Start minimised
  says it is unavailable; Narrator's actions work on every button; the download sheet's rows are read and the list
  scrolls to the focused one (if a switch is read badly, try CheckBox in `ControlTypeOf`). **Install:**
  the setup file on a PC without the WebView2 Runtime (the window opens); an update over 0.5 (the old `ui\` folder and
  `WebView2Loader.dll` are gone, `%LOCALAPPDATA%\Lexia\WebView2` is removed at the first start and logged;
  settings.json, Voices and Presets stay); the uninstaller (the program, the Run value; it asks about
  `%LOCALAPPDATA%\Lexia`). Setup closes a running Lexia when asked (also an elevated one); the Run as administrator task.
- [ ] **G1 Read-along bar.** With read-along on: the bar sits under the word you hear; at a hyphen at a Line end; on
  numbers; after Pause, Skip and Stop; with two Picks queued.
- [ ] **G2 Read-along layer.** With read-along on, in a browser and in Undertale, leave the Overlay while it speaks:
  the bar goes on under the heard word; mouse and keys still reach the game or page; the bar goes when the Reading
  ends, on Stop (Control bar, Speech key triple tap), Off and Quit; it is not in the next Shot; Pause keeps the bar on
  its word. With read-along off nothing stays, and no bar shows on the Overlay.
- [ ] **K9 Settings page.** The page against your mockup (light and dark); hover and Tab over each switch and read the
  card; the read button on a switch's card; turn read-along off while the layer shows: the layer goes; Narrator reads
  a switch as a switch.
- [ ] **K10 Window buttons.** Drag the window by the empty area; minimise and close with the mouse, Tab + Enter,
  Alt+Space and Alt+F4; the shadow and the corners; 100 / 150 / 200 % and a move between monitors; Narrator reads both
  buttons.
- [ ] **K11 Read / stop button.** Click the speaker on a card: speech starts and the icon changes; click again: it
  stops; once more: it starts from the first sentence; a card that finishes shows the speaker again; another card shows
  the speaker and a click replaces the reading; a Pick queued behind a card goes on when the card is stopped; Narrator
  reads "Stop reading ..."; no shaking while it shows stop; 150 % and dark.
- [ ] **K12 Logo.** The exe icon in Explorer (large and small), the tray icon On and Off (16 and 32 px, on a dark and a
  light taskbar), the Setup icon and window, the Start menu entry; the glossary reads right to you.
- [ ] **K13 Palette.** Every page light and dark, the sheets (no mockup: they use the same tokens), the Control bar in
  a Session, the tray icons. The dark pairs below the contrast minimum (`palette_test.cpp`, reported, not changed):
  text on the accent 4.17 (key chip, needs 4.5), the enable button's text 2.83, the disable button's text 4.40, an icon
  button's hover 2.41, Help / size on the bar 1.73, accent on a bar tile 2.41, the bar tile edge 1.73; light: the idle
  window icons 1.35. Decide if any must change.
- [ ] **D9 Start options.** A fresh start (no settings.json) is Off and opens the window; turn On, Quit, start again:
  still On; run at startup with start minimised on and off, then sign out and in.
- [ ] **D10 Key card aloud.** Select each key, press play, with Lexia Off and On.
- [ ] **D4 / C5 in games.** In Undertale, the Control bar does not minimise the game (note whether bar clicks also
  reach the game); F6 and F7 reach the game when Lexia is silent.
- [ ] **A5 Grouping.** Undertale, a browser and a video with subtitles.
- [ ] **H4 Installer pages.** Look at each page of Setup, light and dark.
- [ ] **B8 Voices Cori and Mike.** Listen to Cori (the default) and Mike in Lexia, at Speed, Pitch and Volume
  changed; pick Mike on the Voices page and Pick text.
- [ ] **B9 Update keeps old Voices.** Update a 0.6 install with ljspeech active, its Speed changed and a ljspeech Preset
  saved: ljspeech is on the Voices page as your own Voice (Delete works), still speaks, keeps its Speed, and the Preset
  loads into it. Then delete it, Quit and start Lexia again: it stays deleted.

## Open tasks

### D7 Help video (owner)

A silent tutorial with captions in the Interface language, shipped in the package. The owner makes the video; the
Help page shows its place until then.

### Open code-review findings (low priority; fix when the area is touched)

| # | Where | Finding | Fix |
| --- | --- | --- | --- |
| R24 | `capture.cpp` | `CreateCompatibleDC` and `BitBlt` results are not checked: a failed capture (secure desktop, locked session) gives a black Shot reported as success, so "Screen capture failed" never comes. | Check both, return false |
| R25 | `capture.cpp` | The DIB bits are read right after `BitBlt` without the `GdiFlush()` that the CreateDIBSection docs ask for. | `GdiFlush()` after `BitBlt` |
| R33 | `main.cpp` | `SetProcessDpiAwarenessContext` is imported statically, so the `SetProcessDPIAware` fallback never runs (the exe does not load before Windows 10 1703). | Remove the fallback; the manifest declares PerMonitorV2 |

R1–R23, R26–R32 and R34–R35 are fixed (Phase 0, A, C, D, E). Unverified, for the owner: GDI `BitBlt` returning
black for exclusive-fullscreen or HDR games; the detector using every core while a game runs (stutter?).

### E — Bulgarian (Lexia 0.8; on hold, ADR-0007)

Lexia offers English only. E3–E5 are done and stay in the code; Bulgarian is in the Language table but not offered.

- [ ] **E1 Cyrillic fixtures.** 10+ Bulgarian screenshots with expected text.
- [ ] **E2 OCR quality.** Score `rec_cyrillic.onnx`; note confusions.
- [ ] **E6 The owner's Bulgarian Voice (owner).** The owner records their own voice and trains a Bulgarian Medium
  Piper Voice (decided Nov 5). Train from the clean Piper base
  (`rhasspy/piper-checkpoints/_base_model`, CC BY 4.0), not from a lessac-derived checkpoint. *Done when:* the Voice is
  in `voices/voices.json` with `ship: true`, has `licenses/components/voice-<name>.txt` (`docs/third-party.md`), and
  `lexia_cli --say "..." --lang bg --wav out.wav` speaks with it.
- [ ] **Language UI.** The Language selector, the Language key in the Settings window, a Control bar Language button,
  the Interface language choice (Settings and installer).

### H5 — Free signing through SignPath Foundation (Lexia 1.0)

SignPath Foundation signs open-source projects for free (the publisher shown is "SignPath Foundation"). Signing runs
only from a GitHub Actions release workflow, and each release needs the owner's approval in SignPath. SignPath asks
that the project can be found with a simple Google search, so the owner works on that first. If the approval has not
come by the 1.0 release, 1.0 ships unsigned; nothing here blocks a release.

- [ ] **H5.0 Be found (owner).** A project page, the GitHub repo and releases linked from places people read.
- [ ] **H5.1 Apply (owner).** Multi-factor authentication on GitHub (later SignPath) for every member; apply at
  signpath.org with the repo URL. Done when SignPath gives an organisation id, a project slug and a release policy.
- [ ] **H5.2 Eligibility (agent, then owner).** Every shipped part against `licenses/components/` (all OSI or
  public domain / CC BY data); ask SignPath whether public-domain and CC BY data files (the Voice, icons, pictures) are
  acceptable. Also: the uninstaller, no hidden system changes, privacy (ADR-0005).
- [ ] **H5.3 Code signing policy (agent).** A "Code signing policy" section in `README.md` (the attribution line, the
  roles, the privacy statement), linked from the release notes.
- [ ] **H5.4 Release workflow (agent).** `.github/workflows/release.yml` on a `v*` tag on `main` (and
  `workflow_dispatch` for a test run): `windows-latest`, **no caches** (SignPath's origin check), `fetch_deps.ps1`,
  configure with `-DLEXIA_INSTALLER=ON -DLEXIA_WARNINGS_AS_ERRORS=ON`, build, `ctest`, `score_layout.ps1`; actions pinned
  to a SHA; `permissions: contents: read, actions: read`; the token only in the secret `SIGNPATH_API_TOKEN`.
- [ ] **H5.5 Two signing requests (agent).** Sign `lexia.exe` first and put it back into `build\package\Lexia\`; then
  build the installer and the zip from that folder and sign the setup file; attach both to a draft GitHub release.
  The artifact configuration (`.signpath/artifact-configuration.xml`) takes the version from `vcpkg.json`.
- [ ] **H5.6 What is signed.** Only `lexia.exe` and the setup file; `piper.dll` (upstream code) is not signed under
  SignPath. `-DLEXIA_SIGN` (H3) stays for a certificate of the owner's own.
- [ ] **H5.7 Docs (agent).** `docs/signing.md` (the workflow, the approval, the secret, `signtool verify /pa /v`),
  CLAUDE.md (release steps). *Done when:* a `v1.0.0` tag gives a draft release whose setup file is signed by SignPath
  Foundation and installs and uninstalls on a clean PC.

### J — Linux on Ubuntu (Lexia 1.0)

Decided Nov 5: Lexia 1.0 also runs on Ubuntu (the current LTS, x64). Windows stays first. The core is portable (OCR,
layout, Piper, the Reading queue, settings, strings, the widget tree of `src/native_ui/`); the shell is Win32 (keys,
capture, Overlay, Control bar, tray, audio sink, the Direct2D Settings window, start-up, single instance, elevation).
The rules stay: the Overlay never takes focus (ADR-0001), network only on request (ADR-0005), GPL-3-compatible only.

- [ ] **J0 Decision: X11, Wayland, or both (ADR).** Wayland gives no global keys, screen reading or no-focus window
  without the desktop's help; X11 (and XWayland games) allows all three. *Done when:* an ADR records it, tested on the
  owner's Ubuntu PC.
- [ ] **J1 Platform seam.** The Win32 parts behind small interfaces, the Windows code unchanged; no behaviour change
  (tests and fixture score the same).
- [ ] **J2 Linux build and CI.** CMake + vcpkg with GCC or Clang; `lexia_core`, `lexia_tests`, `lexia_cli` first; a
  CI job on `ubuntu-latest`. *Done when:* `ctest` and `score_layout` pass on Linux.
- [ ] **J3 Keys and capture,** as J0 decides (X11: `XGrabKey`, `XGetImage` / XShm; Wayland: portals, or a clear message).
- [ ] **J4 Overlay and Control bar** that do not take focus from a full-screen game (owner's check: Steam / Proton).
- [ ] **J5 Audio, tray and Settings window.** PipeWire or PulseAudio; StatusNotifierItem / AppIndicator; the widget
  tree with a Cairo/Pango canvas and AT-SPI, or a new window.
- [ ] **J6 Start-up and settings.** XDG folders, a `~/.config/autostart` entry, one instance; no Run as admin.
- [ ] **J7 Package.** A `.deb` (maybe Flatpak or AppImage, decided in J0) with the models, Voices and notices.
- [ ] **J8 Your check.** A browser, a video with subtitles and a game on your Ubuntu PC.

## Done (one line each)

- **Phase 0** (P0.1–P0.8): repo audit (R17+), git and `fetch_deps.ps1`, build hygiene (`/utf-8 /W4`), `lexia_core`
  split, doctest `lexia_tests`, `lexia_cli` and the Fixture score, the logger, CI.
- **A — Layout:** A1 baseline (`docs/notes/layout-baseline.md`), A2 failure classes (`docs/notes/layout-failures.md`),
  A3 fixes for L1, L3, L4 (Dist 0.1998 → 0.1412, BlkErr 4.22 → 2.39; L2 deferred), A4 `layout` knobs in settings.json.
- **B — Speech:** B1 Piper in-process (ADR-0004), B2 Reading queue, B3 Announcements, B4 Voice controls (Speed, Pitch,
  Volume; Presets as files since D8), B5 Voice library, B6 Voice licences (`docs/notes/voice-licences.md`), B8 (0.7)
  the shipped Voices `en_GB-cori-medium` (the default) and `en_US-mike-medium` instead of `en_US-ljspeech-medium`
  (`rhasspy/piper-voices` commit `cf910fb`; `docs/third-party.md`); B9 (0.7) an update keeps a Voice that Lexia no
  longer ships: Setup no longer empties `{app}\voices`, and `AdoptRetiredVoices` (`tts/voice_library.*`, called by
  `App::Init`) copies it into the user's Voices folder once (the names handled are kept in settings.json `adoptedVoices`, so deleting the Voice sticks; the rule "Updates keep the user's data").
- **C — App core:** C1 `App`, C2 OCR worker, C3 safe Language switch, C4 live settings reload, C5 keys (F8, F7, F6; no
  quit key).
- **D — Tray, Settings window, Control bar:** D0 / D1 (HTML pages in WebView2, replaced by K), D2 tray, D3–D6 window and
  pages, D4 Control bar, D8 the paper design (ADR-0008 Run as admin), D9 start options, D10 the key card read aloud.
- **E3–E5:** the Alphabet lock, the Lookalike fix, a Voice per script Stretch.
- **F:** closed (became B1, ADR-0003, ADR-0004).
- **G — Read-along (0.6):** G1 the Read-along bar on the Overlay (PR #2), G2 the Read-along layer (PR #3); since K9
  one switch, "enable read-along" (`readAlong`, Off by default), controls both.
- **H:** H1 version from `vcpkg.json`, H2 licences and notices, H3 optional signing (`docs/signing.md`), H4 Inno Setup
  installer.
- **I1:** the string table (`strings/`).
- **K — Native Settings window (0.6, ADR-0009):** K0 reference shots, K1 toolkit, K2 window and simple pages, K3 Voices
  page, K4 Settings page and key capture, K5 UI Automation, K6 WebView2 and `ui/` removed, K8 the owner's first changes
  (PR #1); K9 new Settings page and switch cards (PR #5, texts PR #7), K10 own minimise and close buttons (PR #6), K11
  read / stop button (PR #8), K12 logo and the name "read-along" (PR #9), K13 final palette and SVG engravings (PR #10).

## Milestones

1. **M1 Agents can verify alone** — done.
2. **M2 Grouping you trust** — A done; A5 (your check) open.
3. **M3 Right voice per language** — English done (ADR-0007); Bulgarian waits for 0.8.
4. **M4 Lexia 0.5** — released Nov 5, 2026.
5. **M5 Lexia 0.6: Read-along and the native Settings window** — released Nov 8, 2026; your checks open.
6. **M6 Lexia 0.7** — future changes after 0.6 (owner, Nov 8); B8 done (the Voices Cori and Mike); B9 done (an update
   keeps a Voice that Lexia no longer ships).
7. **M7 Lexia 0.8: Languages** — E1, E2, E6, the Language UI.
8. **M8 Lexia 1.0: Linux and signing** — J; H5 if SignPath accepts Lexia.

## Decisions

- **Free and open source, GPL-3.0-or-later** (ADR-0003). Free, no paid version.
- **Piper replaces SAPI** for all speech (ADR-0004); Medium-quality Voices only.
- **The Overlay never takes focus; input from global hotkeys only** (ADR-0001). No low-level input hooks (anti-cheat).
- **PaddleOCR stays** (ADR-0002).
- **Network only when the user asks; the screen never leaves the PC** (ADR-0005).
- **English only for now** (ADR-0007, Oct 28): no Language UI until 0.8; Bulgarian stays in the code.
- **Run as admin** is a setting with a UAC relaunch (ADR-0008).
- **Native Settings window** in Direct2D with UI Automation (ADR-0009, Nov 7; supersedes ADR-0006, WebView2).
- **Inno Setup installer.**
- **Updates keep the user's data** (owner, Nov 10): an update never deletes or resets settings, the user's Voices,
  Presets or the chosen Voice unless there is no other way; a Voice that a new version no longer ships stays usable
  as one of the user's own Voices (B9).
- **Linux on Ubuntu in 1.0** (Nov 5); **the owner trains the Bulgarian Voice** (E6, Nov 5).
- **SignPath signing moved to 1.0** (Nov 7); 0.7 and 0.8 ship unsigned.

## Open questions

- [ ] Target hardware for friends' PCs: any very old CPUs? Affects the ONNX Runtime build, the OCR speed budget and
  the Voice quality.

## Sources

- [piper-voices voices.json — Hugging Face](https://huggingface.co/rhasspy/piper-voices/raw/main/voices.json)

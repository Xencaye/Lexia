# ui/ — the Settings window pages

HTML, CSS and JavaScript for the Settings window (ADR-0006). The pages open in a normal browser
with sample data (`scripts\ui_preview.ps1`), and inside WebView2 with the C++ host (`src/settings_window.*`, `src/ui_host.*`).
No network at run time: fonts, icons, pictures and scripts are local files (ADR-0005).

The window is a fixed 800 x 600 frame (D8, the "paper" design of `wireframe/*.png`): six pages, the links
`home | voices | settings` at the top, `report | help | about` at the bottom left, the Master volume and the flute at the
bottom right. Everything is placed in px.

| Part | What it owns |
| --- | --- |
| `index.html` | The one document: the six pages as `<section class="page">` (home, voices, settings, report, help, about), the two `<nav>`s of buttons (the current one has `aria-current="page"`; no tablist), the Master volume, and the sheets as `<dialog>` (Voice download, the result of a file dialog or a delete, Delete a Voice) |
| `css/` | `tokens.css` (the palette, a temporary dark set, `TODO(owner)`), `base.css` (font, reset, focus ring, motion), `icons.css` (the icons as CSS masks, the engravings as luminance masks), `components.css` (icon button, text link, button, select, slider, switch, key chip, sheet, notice), `frame.css` (the frame, the links, the rules, the Master volume, the flute), `pages.css` (the place of every part of every page), `voice_library.css` (the Voice download sheet and the result sheet) |
| `js/bridge.js` | `lexia.send`, `lexia.on`, the transport (WebView2 or the mock host) |
| `js/i18n.js` | `lexia.t`, `lexia.tf`: the words, from the `strings` message |
| `js/controls.js` | The two shared parts: `lexia.makeSlider` (a `role="slider"` that draws the state; speed has a log scale) and `lexia.sheet` (open and close a `<dialog>`, focus back) |
| `js/app.js` | The six pages and `?page=`, Home (the power circle and the button), the Master volume, the theme (`dark`) |
| `js/voice.js`, `js/voice_library.js` | The voices page (Voice, the three icon buttons, three sliders, reset, delete, preset files); the Voice download sheet and the result sheet (add from a file, delete, preset results) |
| `js/settings.js`, `js/pages.js` | The settings page (key rows, the card, key capture, four switches); the report and about pages (`openLink`, `openNotices`) |
| `fonts/`, `icons/`, `img/` | Atkinson Hyperlegible Next (OFL); the line icons (SVG, Stratis UI, CC BY 4.0; the sources are in `icons/SOURCE.txt`); the engravings (JPEG, black lines on white). The licence files stay beside them |
| `dev/` | **Dev only. Never packaged.** See below |

Icons and engravings are drawn as CSS masks (`mask: url(...)`), so their colour comes from the page (`currentColor`, the
tint tokens), not from the file. A mask is a CORS fetch: on `file://` it loads only when Edge runs with
`--allow-file-access-from-files` (the preview script and the `ui_bridge` test pass it); inside WebView2 the page is
same-origin (`https://lexia.invalid`) and needs nothing.

## Dev only: `ui/dev/`

Everything in `ui/dev/` is for the browser preview and the tests. The build (`cmake/ui_files.cmake`, run by the package step and next to `lexia.exe` after every build) copies `index.html`, `css/`,
`js/`, `fonts/`, `icons/` and `img/`, and leaves out `dev/` and this README.

- `dev/dev_host.js`: in a browser it loads the mock host (or, with `?test`, the bridge tests); inside
  WebView2 it does nothing. **The copy step removes the whole "DEV ONLY" block** (the one `<script src="dev/dev_host.js">` line) from the copied
  `index.html` and fails if a mention of `dev_host` is left. The source keeps the block, so the browser preview and the tests work.
- `dev/mock_host.js`: the sample state and the host's answers. `dev/strings.js` is written by
  `scripts\ui_preview.ps1` from `lexia_cli --strings-json en` (not in git).
- `dev/tests*.js`, `dev/tests.html`: the bridge tests (ctest `ui_bridge`, headless Edge, `index.html?test`).
- `dev/controlbar.*`, `dev/tray.*`: the Control bar and tray menu previews, the visual spec for D4 and D2.

## Preview and URL switches

`.\scripts\ui_preview.ps1 [-Screens] [-NoOpen] [-Cli <exe>]` copies `ui\` to `build\ui-preview\` and
opens it in Edge (with `--allow-file-access-from-files`). `-Screens` also writes 800 x 600 screenshots to
`build\ui-preview\shots\`. Switches are described in the header of `dev/mock_host.js`: `?theme=`, `?state=`,
`?page=`, `?hover=<id>`, `?sheet=`, `?voices=none`, `?pitch=`, `?startup=`, `?admin=`, `?left=`, and more.
`?page=` (`?tab=` is an older name; `voice` means `voices`), `?select=` and `?capture=` are read by the page itself, so the C++ host can open the window on a place
(the Control bar's Help opens `?page=help`).

## The bridge: the whole contract

The page and its host talk only through JSON messages of the form `{ "type": string, ...payload }`.
Inside WebView2 the transport is `window.chrome.webview` (`postMessage` and the `message` event).
In a browser the mock host is the transport. The page keeps no copy of a setting: it draws what the
last `state` says. A user action sends a message, and the change shows when the next `state` comes.
Problems are codes, never words: the page turns a code into text with the strings table. The page never sends a URL
(ADR-0005): `openLink` names a link and the host owns the address. When the page has sent `ready`, the host answers
`strings` first, then `state`.

### Host to page

| Message | Payload | Meaning |
| --- | --- | --- |
| `strings` | `{ language: "en", strings: { id: text } }` | The Interface language's table (`StringsToJson`). The page holds no text of its own |
| `state` | see "The `state` message" | The whole current state. Sent again after every change |
| `catalogue` | `{ voices: [{ name, language, sizeBytes, installed }] }` | The downloadable Voices (Medium, Languages Lexia reads). The page lists every well-formed row and shows no Language. Opens the download sheet if it is closed |
| `catalogueFailed` | `{ problem }` | The catalogue could not be fetched (`"network"`) |
| `downloadProgress` | `{ name, done, total }` | Bytes done and bytes in all |
| `downloadDone` | `{ name }` | The Voice is installed; the next `state` lists it |
| `downloadFailed` | `{ name, problem, language?, quality? }` | `problem` is a `ProblemId` of `voice_library.h`; `"cancelled"` is the answer to `cancelDownload` and shows no error |
| `voiceAdded` | `{ voice, language }` | Add from a file worked |
| `voiceRefused` | `{ problem, language?, quality? }` | Add from a file was refused. `language` is the English name ("German"), `quality` is lower case |
| `voiceDeleteFailed` | `{ voice, problem }` | `deleteVoice` did not work. `problem` is a `ProblemId` (`"shipped"` has its own text; any other shows the general one). Shown in the result sheet. A success is the next `state` (the Voice is gone from `installed`) |
| `presetSaved` | `{ name }` | `savePresetFile` worked; `name` is the file's name. Result sheet |
| `presetLoaded` | `{ name }` | `loadPresetFile` worked; the next `state` has the new controls. Result sheet |
| `presetFailed` | `{ problem }` | `"badFile"`, `"cannotWrite"` or `"noVoice"`. Result sheet. A cancelled file dialog sends nothing |

### Page to host

| Message | Payload | Sent when |
| --- | --- | --- |
| `ready` | none | The page has loaded |
| `setOn` | `{ on }` | The power circle, or the enable / disable button under it |
| `setVoice` | `{ language, voice }` | A Voice is chosen |
| `setControl` | `{ name, value }` | A slider moves. `name` is `speed`, `pitch` or `volume`. The value is inside the range of the `state` |
| `resetControl` | `{ name }` | The reset icon beside a changed slider |
| `openCatalogue` | none | The download icon, or Try again on the sheet |
| `download` | `{ name }` | Download (or Try again) on a catalogue row |
| `cancelDownload` | `{ name }` | Cancel on the row that downloads |
| `addFromFile` | none | The add icon, or Choose another file. The host opens the native file dialog |
| `deleteVoice` | `{ voice }` | Delete in the confirmation of the delete icon. The icon works only for a Voice in `voices.eng.removable` |
| `savePresetFile`, `loadPresetFile` | none | save preset, load preset. The host opens the file dialog in the presets folder |
| `captureKey` | `{ action }` | A key chip is selected; the page waits for a key. The host may stop the Lexia keys while it waits |
| `setKey` | `{ action, key }` | The page heard a key (`"F8"`, `"Ctrl+Alt+R"`). The host checks it and answers with a new `state` (also when it refuses the key) |
| `cancelKeyCapture` | `{ action }` | Esc, a press outside the chip, or the focus left the chip |
| `readKeyCard` | `{ action }` | The key card's play button: the host reads that key's description aloud (D10) |
| `setRunAtStartup`, `setStartMinimised`, `setRunAsAdmin`, `setLeftHanded` | `{ on }` | The switches (start minimized only while run at startup is on) |
| `setTheme` | `{ theme }` | The dark mode switch: `"light"` or `"dark"`, the other of what `dark` says |
| `setMasterVolume` | `{ value }` | The Master volume slider (0 .. 100) |
| `openLink` | `{ link }` | `"mail"`, `"discord"`, `"github"` (report page; github also on about) or `"itch"` (about) |
| `openNotices` | none | about > third-party notices. The host opens `THIRD-PARTY-NOTICES.txt` |

### The `state` message

```
type: "state"
on:             bool                                   Lexia On or Off
language:       "eng"                                  English only (ADR-0007); the page uses it only to find `voices[language]`
theme:          "system" | "light" | "dark"            the stored choice (the page does not use it: see `dark`)
dark:           bool                                   the theme in use (IsDarkTheme(theme, Windows)); the page sets data-theme to it
                                                       and the dark mode switch shows it
version:        "0.5.0"                                about
runAtStartup:   bool
startMinimised: bool            a start at logon stays in the tray (Settings::startMinimised)
runAsAdmin:     bool                                   the stored setting
elevated:       bool                                   Lexia runs elevated now (not drawn yet)
leftHanded:     bool
masterVolume:   { value, min: 0, max: 100, def: 100, step: 5 }   Settings::speechVolume
keys:           [{ action: "read"|"pause"|"speech", key: "F8" }]  key "" = no key; an action "language" is ignored
voices:         { eng: { installed: ["name", ...], active: "name", removable: ["name", ...] } }
                                                       active "" = none; removable = the Voices a person added (not shipped)
controls:       { speed|pitch|volume: { value, min, max, def, step } }
                                                       the active Voice's controls; absent with no Voice
```

Fields are read in `js/app.js` (on, dark, masterVolume), `js/voice.js` (language, voices, controls),
`js/settings.js` (keys, runAtStartup, startMinimised, runAsAdmin, dark, leftHanded), `js/pages.js` (version).
`js/voice_library.js` reads no `state` field. A full example is the state in `dev/mock_host.js`.

Removed since D0: `presets`, `controls.expressiveness`, and the messages `reportBug`,
`savePreset`, `selectPreset`, `deletePreset`. (`startMinimised` and `setStartMinimised` were removed in D8
and came back in D9, as the switch under run at startup.)

### Screens and what the page owns

The page owns only what is not a setting: which page is shown, which key row is selected (a hover, a focus or a click
selects it), which chip waits for a key, the sheets and the progress of the download that runs. The speed slider
position is on a log scale (1.0 at the centre of 0.5 .. 2); the value it sends is still on the step grid of the `state`.
The demo button of the key card has no clip yet: it is off (`aria-disabled`) and its tooltip says so.

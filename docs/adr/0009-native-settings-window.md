# The Settings window is native Win32, drawn with Direct2D; WebView2 is removed

Status: accepted (2026-11-07). Supersedes ADR-0006.

The Settings window (Home, Voices, Settings, Report, Help, About and their dialogs) is a native Win32
window. Lexia draws every control itself with Direct2D and DirectWrite, in the D8 paper design, as the
Control bar already does. UI Automation providers make each control visible to screen readers. The
HTML pages in `ui/` and WebView2 are removed. The screenshots of the HTML pages in
`wireframe/html-reference/` are the visual specification.

## Why

- No runtime to install. Windows 10 often has no WebView2 Runtime; the installer had to carry
  Microsoft's bootstrapper and download the runtime.
- No browser process: about 100 MB less memory while the window is open, and the window opens at once.
- One drawing stack. The Control bar and the Settings window share the palette, the icons, the font
  (Atkinson Hyperlegible Next, now also in the Control bar) and the tooltip.
- A smaller attack surface. No HTML engine, no virtual host and no request filter: the assets are
  resources in the exe, and nothing is fetched (ADR-0005 holds by construction).

## Considered Options

- **Keep WebView2** (ADR-0006): least work, but it keeps the runtime, the memory and the bootstrapper.
- **Owner-drawn common controls**: free keyboard and screen-reader support, but the look of the
  design (thin sliders, dotted rules, masked engravings) does not fit them.
- **Direct2D, self-drawn, with UI Automation** (chosen).

## Consequences

- Lexia must give keyboard focus, Tab order and UI Automation itself; the widget tree in
  `src/native_ui/` (Win32-free, tested with a fake canvas) holds this logic.
- The Linux Settings window (J5) cannot reuse HTML pages. It can reuse the widget tree with a
  Cairo/Pango canvas, or be a new window.
- Visual changes are checked with `lexia_settingsshot` against `wireframe/html-reference/`.

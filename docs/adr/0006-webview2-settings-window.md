# The Settings window is HTML/CSS in WebView2; the Overlay stays plain Win32

The Settings window (Home, Voice, Settings, Help tabs) is built in HTML/CSS hosted in WebView2.
The neumorphic look is plain CSS `box-shadow`, and help videos, Interface-language strings and
light/dark mode are trivial there; the runtime ships with Windows 11. The Overlay does not use it:
it must stay `WS_EX_NOACTIVATE` (ADR-0001) and appear instantly.

## Considered Options

- **Qt Quick**: native and capable, but a heavy dependency and a long vcpkg build.
- **Hand-drawn Win32**: no dependencies, but soft shadows, animation and video are a lot of work.

## Consequences

- About 100 MB of extra memory, only while the Settings window is open; it is destroyed on close.
- The page and the C++ side talk through WebView2 messages; the page must work with no network
  (ADR-0005), so all its assets are bundled.

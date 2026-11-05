# The Overlay never takes focus; input comes from global hotkeys

The Overlay shows the frozen Shot in a `WS_EX_NOACTIVATE` window and gets its keys only through
`RegisterHotKey` (Esc is registered just for the length of a Session). Old full-screen games
minimise when they lose focus, so taking focus (the obvious way to receive keyboard input) is out.

## Considered Options

- **Take focus**: rejected; the game minimises.
- **Session-only low-level keyboard/mouse hooks** (`WH_KEYBOARD_LL` / `WH_MOUSE_LL`) to swallow all
  input and route it to the Overlay: rejected because Lexia will be used over online games, whose
  anti-cheat may flag input hooks.

## Consequences

- Keys that are not registered hotkeys still reach the game under the Overlay, and the game keeps
  running behind the frozen Shot.
- Games that read the mouse through raw input may also receive clicks meant for the Overlay.
- Keyboard navigation inside the Overlay (beyond the registered keys) isn't possible.

These leaks are accepted for now; a better input route can be revisited later.
- The Pause and Speech keys (F7, F6) are registered only while Lexia is speaking or paused, so
  they reach the game the rest of the time. While Lexia speaks they are Lexia's, in games too.
  The speech thread tells the UI thread through a hidden message window; if Lexia cannot make
  that window, it holds F7 and F6 all the time instead (logged), since they could not work otherwise.

# Lexia is licensed GPL-3.0-or-later

Lexia is free and open source. Its speech engine, Piper (`OHF-Voice/piper1-gpl`), and the espeak-ng
phonemiser it builds on are GPL-3.0, so linking them into `lexia.exe` makes the exe GPL-3. I
license all of Lexia GPL-3.0-or-later rather than keep a permissive licence by running Piper as a
separate process: there is nothing to keep closed, and the extra process would be cost without
benefit.

## Consequences

- Every dependency must be GPL-3-compatible (MIT, BSD, Apache-2.0, LGPL, GPL-3 are; GPL-2-only is not).
- Voice models are licensed separately from the engine; each shipped voice's licence must allow
  redistribution and goes in the third-party notices.

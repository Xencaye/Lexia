# English only for now; language support stays in the code

Decided by the owner on 2026-10-28. Lexia offers one Language, English, for reading, and one
Interface language, English, for its own words. Bulgarian is not offered until the owner decides to
bring it back. Supporting more than one language stays a goal: the code keeps everything that makes
a second Language possible, so offering one again is a small change, not a rebuild.

The retraining of the Bulgarian Voice (B7, issue #12) is dropped. The Bulgarian OCR tasks that were
not started (E1 Cyrillic Fixtures, E2 OCR quality) wait with it.

## Consequences

- `Language` and `strings::InterfaceLanguage` have an `offered` flag. Bulgarian's is `false`.
- The app uses only what is offered. `KeepToOffered` (settings.h) turns a `language` or
  `interfaceLanguage` in settings.json that is not offered into English, with a note in the log.
  The Language switch skips a Language that is not offered.
- What is not offered stays in the code and its tests: the Bulgarian Language entry and string
  table, the Alphabet lock (E3), the Lookalike fix (E4), the Voice per script stretch (E5), and
  `lexia_cli --lang bg` for development. `ParseSettings` and the Voice library still know Bulgarian.
- The package leaves out the models of a Language that is not offered (`cmake/package.cmake`).
- UI work (D0, D3–D6, H4) shows only offered languages: no Interface language choice and no
  Language list while there is one of each, and screens in English only.
- Since 2026-10-28 (D0 redesign), Lexia 0.5 shows no Language control at all: no Language selector
  (Home, Voice), no Language key in Settings, no Language button on the Control bar. The C++
  Language switch and its `switchLanguage` key setting stay. Language support is planned for
  Lexia 0.8 (milestone M6 in the plan).
- To offer a Language again: set its `offered` flag, put its models back in the package, give it a
  shipped Voice (`voices/voices.json`, licence checked, `docs/third-party.md`), and run its Fixtures.

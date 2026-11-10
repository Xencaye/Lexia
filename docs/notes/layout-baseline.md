# Layout baseline (A1)

The current numbers are in `tests/fixtures/baseline.json`; this page is the A1 record.

Measured 2026-10-25 on commit `90a3213` (before any change to `layout.*`), 18 Fixtures.

> **Note (2026-11-05):** the `steam_01` Fixture (a Steam screenshot) is removed from `tests/fixtures/`
> and from `baseline.json`. Its row below stays as a record of this measurement.

| Fixture | Dist | BlkErr | Expected Blocks | Produced Blocks |
| --- | --- | --- | --- | --- |
| lovebyte_01 | 0.2444 | 2 | 6 | 8 |
| lovebyte_02 | 0.3832 | 5 | 14 | 19 |
| lovebyte_03 | 0.2598 | 5 | 12 | 17 |
| oblivion_01 | 0.2870 | 4 | 2 | 6 |
| seed-article | 0.0000 | 0 | 4 | 4 |
| steam_01 | 0.4237 | 25 | 81 | 56 |
| twd_01 | 0.1250 | 0 | 1 | 1 |
| twd_02 | 0.3622 | 13 | 19 | 6 |
| undertale_01 | 0.8750 | 0 | 1 | 1 |
| undertale_02 | 0.0000 | 0 | 1 | 1 |
| undertale_03 | 0.0000 | 0 | 1 | 1 |
| undertale_04 | 0.0429 | 1 | 2 | 3 |
| undertale_05 | 0.1706 | 6 | 10 | 4 |
| unraveled_01 | 0.1106 | 7 | 11 | 4 |
| unraveled_02 | 0.0508 | 1 | 3 | 4 |
| unraveled_03 | 0.0732 | 1 | 3 | 2 |
| unraveled_04 | 0.1101 | 6 | 8 | 2 |
| wha_01 | 0.0784 | 0 | 2 | 2 |

**Average: Dist 0.1998, BlkErr 4.22 over 18 Fixtures.**

Dist is the normalised edit distance of the joined Block text (0 = identical, case ignored);
BlkErr is |produced Blocks - expected Blocks|. Both are in `tests/fixtures/baseline.json`.

> **Note (2026-11-10), row fragments and tight centred stacks:** over the 17 Fixtures left in
> `tests/fixtures/` the average went from Dist 0.1356 / BlkErr 1.76 to **Dist 0.1310 / BlkErr 1.29**
> (`baseline.json` rewritten). lovebyte_01 (Dist 0.0889 -> 0.0455, BlkErr 2 -> 0) and lovebyte_02 (0.0659 -> 0.0184,
> BlkErr 5 -> 1) improved because "Save" | "Files" and "Early" | "Access" | "Version" | "0.9.18" are one Block now;
> oblivion_01 has two Blocks fewer (BlkErr 4 -> 2) but its Dist rose 0.2870 -> 0.3009, because the joined
> "preloading:" and ": 2%" keep the OCR's stray colon. The other Fixtures are unchanged. A row join of 1.2 line
> heights keeps the 1.7 lh grid of lovebyte_02 and the 3 lh gaps of twd_02 apart. The centred-stack cue is limited to
> all-caps text: allowing any case merged a centred list of statistics (unraveled_01) and a value centred under
> another column's value (twd_02), and both got worse.

To re-run: build (`cmake --build build --config Release`), then
`powershell -ExecutionPolicy Bypass -File scripts\score_layout.ps1`. It prints this table, the
average, and exits 1 if any Fixture got worse than `baseline.json`. Add `-SaveBaseline` to
rewrite the baseline after a deliberate improvement.

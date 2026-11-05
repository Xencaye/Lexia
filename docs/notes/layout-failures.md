# Layout failures (A2)

Taken on commit `f2bb3ee` (code unchanged since the A1 baseline: Average Dist 0.1998, BlkErr 4.22).
Method: for each of the 18 Fixtures, `lexia_cli <screen.png> --json` and `--draw` against
`expected.txt`; line geometry (gap, left/right offset and width ratio, all in line heights) read
from the JSON. BlkErr counts below are net (produced minus expected) per Fixture, so a
class's share is approximate.

> **Note (2026-11-05):** the `steam_01` Fixture (a Steam screenshot) is removed from `tests/fixtures/`
> and from `baseline.json`. The `steam_01` text below stays as a record of this analysis.

Failure classes:

- **L1 List items merged.** Short, stacked, evenly spaced Lines of one menu or list become one
  Block (Expected: one Block per item). Also covers label + value pairs ("PLAY TIME" / "5.4 hours")
  and title + subtitle pairs. Geometry: the Lines are short (4 to 8 line heights wide), the gap is
  0.06 to 0.45 line heights, widths vary or are equal, so `canJoin` (gap up to 0.9 lh, then 1.8x the
  median, left offset up to 1.5 lh, or right-aligned, or centred) accepts them. A real paragraph
  differs in that its non-final Lines are long and nearly as wide as each other
  (seed-article: 695/711/705 px at 29 px line height, i.e. 24 lh), and the next Line is never
  much wider than the one above.
- **L2 Same-row pieces not joined.** The detector returns one Line per word or phrase and nothing
  joins pieces that sit side by side on one baseline with a word-sized gap: "Early" "Access"
  "Version 0.9.18", "Save" "Files", "Shaders preloading:" ": 2%".
- **L3 Icon noise.** A 1 to 2 character Line with a box far smaller than the page's typical Line
  ("X" 12x11 px on a page of 29 px text, "A" 15x9, "L." 7x7, "1" 9x8) becomes its own Block, and
  is read aloud.
- **L4 Reading order across panels.** Rows are formed across the whole width, so a panel on the right
  interleaves with the one on the left (steam_01: "FRIENDS WHO PLAY" lands between two Blocks of
  the main column). The Fixtures disagree on what is right (see "Needs a product decision").
- **D1 Detector merge (not fixable in `layout.*`).** One detector Line spans several items:
  "[Enter] Select  [Esc] Back  [Space] View legal agreements" (expected: three Blocks),
  "Steam View Friends Games Help". Layout never sees word boxes, so it cannot split a Line.
- **O1 OCR / detector miss or misread (A cannot fix).** Stylised logos read as nonsense
  ("LpJebyte.eHf", "Semrd", "Kvotch"), a photographed screen with no detection (undertale_01), digit
  and letter errors ("EHTER", "Cance1", "Ful1screen", "UNOERTALE"), a missed "2" (unraveled_03),
  "Loading." read as "LOADING" (twd_01).

## Per Fixture

| Fixture | Dist | BlkErr (Exp/Got) | Failures |
| --- | --- | --- | --- |
| lovebyte_01 | 0.2444 | 2 (6/8) | L2: "Early" "Access" "Version 0.9.18" are 3 Blocks (+2). O1: logo read as "LpJebyte.eHf" (1560x282 px). Order: Expected puts "Early Access..." before "Start", produced is top-to-bottom (the Fixture's own order is arguable). |
| lovebyte_02 | 0.3832 | 5 (14/19) | L2: "Early/Access/Version/0.9.18" (+3), "Save"/"Files" (+1). O1: logo read as "E" (141x151). |
| lovebyte_03 | 0.2598 | 5 (12/17) | L1: username "nonononodoka" joined with the message below (offset 0.67 lh), "glitchangelcore" with "LET'S GO!". L3: "B", "L.", "#4", "d>>". L4: "Chat" "610" (top right) come before "Equinox..." "LIVE" (top left). |
| oblivion_01 | 0.2870 | 4 (2/6) | L2: "Shaders preloading:" + ": 2%". O1: three logo Blocks ("Semrd", "Tho GoTT Rood", "Kvotch"). |
| seed-article | 0.0000 | 0 (4/4) | none; two columns and headings come out right. |
| steam_01 | 0.4237 | 25 (81/56) | L1: sidebar list of 14 Lines in one Block, label/value pairs (PLAY TIME, ACHIEVEMENTS, FRIENDS WHO PLAY, "Don't conform"/"Complete Little frogs" x6, "You've played..."/"Would you recommend...", ACTIVITY/Say something/SEPTEMBER). L3: "1", "0", "a", "F", "Q", "33", "TO". L4: right panel interleaved with the main column. D1: top menu bar. O1: "Scolls", "play"/"played". |
| twd_02 | 0.3622 | 13 (19/6) | L1: 8 labels in one Block, 4 values in one, "OFF"+"2560x 1440" in one (right-aligned and the widths differ). The labels column then the values column are read, Expected is row by row, so Dist suffers from the order too. |
| twd_01 | 0.1250 | 0 (1/1) | O1 only ("Loading." vs "LOADING"). |
| undertale_01 | 0.8750 | 0 (1/1) | O1: photographed screen, nothing detected. Largest Dist in the set, no layout cause. |
| undertale_02 | 0.0000 | 0 | none. |
| undertale_03 | 0.0000 | 0 | none. |
| undertale_04 | 0.0429 | 1 (2/3) | L3: "X" (12x11) next to the dialogue. |
| undertale_05 | 0.1706 | 6 (10/4) | L1: six menu entries in one Block (gap 0.24 to 0.42 lh, lines 10 lh wide), "Begin Game"/"Settings" joined. O1: five misreads. |
| unraveled_01 | 0.1106 | 7 (11/4) | L1: "Save slot 1/2/3" + "Quit game" (4 Lines, widths equal, 4 lh wide), four "Completed ..." Lines (centred). L3: "X". D1: footer hints. |
| unraveled_02 | 0.0508 | 1 (3/4) | L3: "A" (15x9). |
| unraveled_03 | 0.0732 | 1 (3/2) | O1: "2" not detected. Layout correct. |
| unraveled_04 | 0.1101 | 6 (8/2) | L1: six menu entries in one Block (widths 3 to 7 lh, gap 0.03 to 0.16 lh). D1: footer hints. |
| wha_01 | 0.0784 | 0 (2/2) | OCR only. |

## Classes ranked

Net BlkErr over the 18 Fixtures is 76 (average 4.22). Fixtures "hurt" counts Fixtures where the
class is at least one of the causes.

| Rank | Class | Fixtures hurt | BlkErr at stake (approx.) | Dist at stake | Fixable in A? |
| --- | --- | --- | --- | --- | --- |
| 1 | L1 List items merged | 6 (twd_02, steam_01, unraveled_01, unraveled_04, undertale_05, lovebyte_03) | about 55 of 76 | Large: twd_02 0.36, steam_01 0.42, undertale_05 0.17, unraveled 0.11 each, partly through Reading order of the merged Blocks | Yes (A3) |
| 2 | L2 Same-row pieces not joined | 3 (lovebyte_01, lovebyte_02, oblivion_01) | about 7 | small (text order unchanged) | Yes (A3) |
| 3 | L3 Icon noise | 6 (undertale_04, unraveled_01, unraveled_02, lovebyte_03, steam_01, lovebyte_02 partly) | about 10 Blocks | small, a few characters each | Partly: only the tiny ones; a 141x151 "E" is not tiny |
| 4 | L4 Reading order across panels | 2 (steam_01, lovebyte_03; twd_02 reads the other way) | n/a | steam_01 at most part of 0.42 | Needs a product decision |
| 5 | D1 Detector merge | 3 (unraveled_01, unraveled_04, steam_01) | about 5 | small | No (needs `ocr.*`) |
| 6 | O1 OCR | 12 | n/a | all of undertale_01 (0.875), most of the rest of the Dist | No |

## Needs a product decision

L4. The Fixtures disagree: steam_01's Expected reads panel by panel (whole main column, then the
right panel), while twd_02's Expected is row by row across two columns (label, value, label, value)
and lovebyte_03's is nearly pure top-to-bottom. A column-first Reading order (XY cut) would help
the first and hurt the second. Question for the owner: for a screen made of separate panels, should the
Reading order go panel by panel or row by row?

## Done / deferred (after A3, A4 and the review)

Average Dist 0.1998 -> 0.1412, BlkErr 4.22 -> 2.39 (branch feat/A3a).

- **L4 went first although marked "needs a product decision" above.** `docs/GLOSSARY.md` already
  defines Reading order as "down each column before moving across to the next", so a column-first
  order is the stated behaviour, not a guess. A table is still read row by row, because the cut
  takes the widest empty strip: a narrow gutter loses to the row gaps (twd_02), a wide one
  (5 line heights, `columnGap`) wins (steam_01, lovebyte_02). The Fixtures that expect another
  order (lovebyte_03's top-right header before the left column) are the owner's to settle in A5.
- **L4, L1 and L3 share one branch** because the BlkErr metric couples them. L4 alone: lovebyte_03
  Dist 0.2598 -> 0.2913 (over the 0.02 tolerance). L4 + L1: lovebyte_03 BlkErr 5 -> 6. L3 alone:
  steam_01 BlkErr 25 -> 26 and unraveled_01 7 -> 8 (dropping noise from an under-segmented Fixture
  moves the Block count away from Expected). L1 alone: steam_01 Dist 0.4237 -> 0.50, because the
  split Blocks interleave with the panels beside them until L4 orders them. Only all three pass.
- **lovebyte_03 rose 0.2598 -> 0.2709 (+0.011), within the 0.02 tolerance.** Its Expected order puts
  "Chat 610" (top right) before the left column's "Hey there"; the column-first order puts the
  whole left side first. Its BlkErr is unchanged (5), and the other Fixtures gained
  more than this costs.
- **L2 (same-row pieces: "Early" "Access" "Version", "Save" "Files") deferred.** It affects 3
  Fixtures and about 7 Blocks of BlkErr, with no change in Dist, and A3 stops after three classes.
  A rule would join Lines on one baseline with a word-sized gap (under about 1 line height); it
  must not join table cells ("File 01" "File 02", gaps of 1.7 line heights).
- **D1 and O1 stay open** (detector merges, OCR misses): `ocr.*` is not this stream's.
- **Review follow-up, short paragraphs.** The list rule (a Line must be 8 line heights wide to go on)
  split a centred two-line subtitle, a narrow speech bubble and a short dialogue. Geometry cannot
  tell those from a three-entry menu ("Save slot 1/2/3": equal widths, same gaps), so the rule now
  also lets a Line continue when the next one starts in lower case. Left open for A5: a short
  paragraph whose next Line starts in a capital (the sentence ended at the line break) still splits
  into one Block per Line, and a menu written in lower case ("settings", "quit") now joins.

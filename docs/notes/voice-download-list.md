# The Voices for the download list (B12)

Date: 2026-11-12 (section 1 and section 7 updated on 2026-11-17, B12). The research, sections 2 to 6, changed no code. This is not legal advice. It reports what the primary sources
say and where they say nothing. `docs/notes/voice-licences.md` (B6) is about the Voices that Lexia ships. This note is
about the Voices that the Download sheet can offer. Lexia does not distribute these Voices: the user downloads each one
from its source.

## 1. What the owner decided

Nov 12:

- Lexia ships Medium Voices only, so that the installer stays small.
- The download list can have any quality (X-low, Low, Medium, High).
- The owner selects the Voices of the download list by ear. The list shows a label of allowed use for each Voice.

Nov 17 (B13):

- Lexia ships the OLD Cori Medium (section 5) as the default English Voice. It is the copy at `rhasspy/piper-voices`
  commit `5041a7a`. Norman is the second shipped Voice. Cori and Norman are final: the owner does not pick another
  shipped Voice. LJ Speech is not shipped now (an update keeps it as the user's own Voice, B9).
- The download list can take a Voice from any source: Hugging Face (at a pinned commit) or Bryce Beattie's LibriVox
  Voices (https://brycebeattie.com/files/tts/). The owner picks each Voice by ear. The "Medium only" rule is for
  what Lexia ships, not for the download list.

Nov 17 (B12, the owner's pick):

- The Download sheet offers only a short list that the owner picked: **Kristin, John and LJ Speech** (all `en_US`,
  Medium, public domain). Lexia does not fetch the full catalogue now. The list is compiled into Lexia, so the sheet
  opens with no network request.
- No labels for now. All three Voices are public domain, so no label of allowed use is needed. The labels (section 6)
  come back when the owner adds a Voice that needs one.
- The owner will add more Voices to the list later. Section 7 says how.

## 2. Sources and how they were checked

- **`rhasspy/piper-voices`**, commit `ae9d835` (2026-11-11, the latest). It has 32 English Voices in 39 files. Its
  `voices.json` is the same as the one of `main` on 2026-11-12. Between `cf910fb` (the commit that Lexia reads now) and
  `ae9d835`, only `en_US-ryan-medium` changed for English.
- For each file, the SHA-256 of the download is the same as the Git LFS hash at `ae9d835`.
- Each of the 40 files from Hugging Face (39 + the en_AU Voice) passes `lexia_cli --voice-add`.
  Thus Lexia can use them now.
- Each MODEL_CARD was read at `ae9d835`. The dataset licence pages were read on 2026-11-12, where the page loads.
- **Bryce Beattie's page** (https://brycebeattie.com/files/tts/) has 4 Voices that are not in `rhasspy/piper-voices`
  at `ae9d835`. One of them, cori-med, is at the older commit `5041a7a` (section 5).
  The files are on `sfo3.digitaloceanspaces.com/bkmdls/`, with no published hash. Lexia refuses these files as they
  are (`unknown-quality`): their config gives the Voice name as the quality, and the language code `en`.
- **Hugging Face search** for other English Piper Voices: most results are copies of `rhasspy/piper-voices`. The only new
  Voice with a free data licence is `DataCraftsmanAustralia/piper-en_AU-librivox-medium` (commit `7f35faf`,
  2026-09-22), but its base makes it unclear (section 4).
  These were not used: the OpenVoiceOS `miro` and `dii` Voices (CC BY-NC-ND), and the GLaDOS and other character
  Voices (the voice of a character is not free).

## 3. The base model is part of the licence question

A fine-tuned Voice starts from the weights of a base Voice. At `cf910fb`, `rhasspy/piper-voices` trained some Voices
again from two new clean bases:

- `hifi_tts_92` (trained from scratch on Hi-Fi TTS reader 92, CC BY 4.0).
- `rdh` (nl_BE, trained from scratch, dataset CC0 1.0).

Thus some tables in section 3 of `voice-licences.md` are old: alba, jenny_dioco, the medium amy, cori-medium,
hfc_female, joe, mike and ryan-medium are not fine-tuned from lessac now. Two old bases are still a problem:

- **lessac**: the Blizzard 2013 research licence (see `voice-licences.md`, section 2).
- **ryan-low**: CC BY-NC-SA 4.0. Five Low Voices are fine-tuned from it.

## 4. The list, in categories

"Data licence" is the licence of the recordings. "Base" is where the training started.

### Free for any use

| Voice | Qualities | Data licence | Base | Note |
| --- | --- | --- | --- | --- |
| en_US-ljspeech | medium, high | Public domain | from scratch | Shipped until B13 (Medium); now an own Voice after an update |
| en_US-norman | medium | Public domain (LibriVox) | from scratch | Ships |
| en_US-kristin | medium | Public domain (LibriVox) | from scratch | |
| en_US-john | medium | Public domain (LibriVox) | kristin | |
| en_GB-cori | high | Public domain (LibriVox) | from scratch | The old cori of Bryce Beattie |
| en_US-joe | medium | CC0 (OHF-Voice voice-datasets) | rdh (CC0) | B10: sounds worse than its old version |
| en_US-mike | medium | CC0 (OHF-Voice voice-datasets) | rdh (CC0) | B10: says "library" incorrectly |
| Bryce: cori-med (old en_GB-cori-medium) | medium | Public domain (LibriVox) | from scratch | Ships since B13 (the copy at `5041a7a`, section 5) |
| Bryce: mv2 ("ManyVoice") | medium, 16 speakers | Public domain (LibriVox) | from scratch | Not on Hugging Face. For the list, Lexia must write its config with a known quality (its own config does not say one) and its SHA-256 must be in the list |

### Free for any use, credit the Voice

| Voice | Qualities | Data licence | Base | Note |
| --- | --- | --- | --- | --- |
| en_US-hifi_tts_92 | medium | CC BY 4.0 (Hi-Fi TTS, NVIDIA) | from scratch | US phonemes with British vowels (card) |
| en_GB-cori | medium | CC BY 4.0 (Hi-Fi TTS, NVIDIA) | hifi_tts_92 | B10: says "library" incorrectly |
| en_GB-alba | medium | CC BY 4.0 (University of Edinburgh) | hifi_tts_92 | Scottish |
| en_GB-jenny_dioco | medium | Jenny licence: credit "Jenny (Dioco)", commercial use permitted | hifi_tts_92 | Irish. B10: sounds worse than its old version |
| en_US-libritts | high, 904 speakers | CC BY 4.0 (LibriTTS) | from scratch | |
| Bryce: jenny | medium | Jenny licence | from scratch | Not on Hugging Face; Lexia refuses the file as it is |
| Bryce: clean100 | medium, 245 speakers | CC BY 4.0 (LibriTTS-R) | from scratch | Not on Hugging Face; Lexia refuses the file as it is |

### Non-commercial use only

| Voice | Qualities | Data licence | Base |
| --- | --- | --- | --- |
| en_GB-semaine | medium, 4 speakers | CC BY-NC-SA 4.0 | lessac |
| en_US-hfc_female | medium | CC BY-NC-SA 4.0 (Hi-Fi-CAPTAIN) | hifi_tts_92 |
| en_US-hfc_male | medium | CC BY-NC-SA 4.0 (Hi-Fi-CAPTAIN) | lessac |
| en_US-l2arctic | medium, 24 speakers | CC BY-NC 4.0 | lessac |
| en_US-ryan | low, medium, high | CC BY-NC-SA 4.0 | high and low: from scratch; medium: rdh |

### Research use only

| Voice | Qualities | Data licence |
| --- | --- | --- |
| en_US-lessac | low, medium, high | Blizzard 2013 research licence |

### Licence unclear

The recordings are free, but the base or the cards make the licence of the model unclear.

| Voice | Qualities | Data licence | Why unclear |
| --- | --- | --- | --- |
| en_GB-aru | medium, 12 speakers | CC BY 4.0 | base lessac |
| en_GB-vctk | medium, 109 speakers | CC BY 4.0 | base lessac |
| en_US-arctic | medium, 18 speakers | CMU ARCTIC: free for any use, keep the notice | base lessac |
| en_US-reza_ibrahim | medium | CC0 | base lessac |
| en_US-sam | medium | Apache-2.0 | base lessac |
| en_GB-northern_english_male | medium | CC BY-SA 4.0 | base lessac |
| en_US-libritts_r | medium, 904 speakers | CC BY 4.0 | the piper-voices card says "fine-tuned from lessac". The piper-checkpoints card says "trained from scratch", but that checkpoint was replaced on 2026-09-17, and the `.onnx` in piper-voices is from 2023-09-23 |
| en_AU-librivox (DataCraftsmanAustralia) | medium, 10 speakers | Model CC BY 4.0; recordings public domain; dataset CC0 | base: the jenny_dioco checkpoint `epoch=2748` (its `train/2-train.sh`). That checkpoint is from before `cf910fb`, when jenny_dioco was fine-tuned from lessac |
| en_US-bryce | medium | Public domain (the author's own voice) | base is "an unreleased voice" |
| en_US-kathleen | low | CC0 | base ryan-low (CC BY-NC-SA) |
| en_GB-southern_english_female | low | CC BY-SA 4.0 | base ryan-low (CC BY-NC-SA) |

### No licence found

| Voice | Qualities | What the source says |
| --- | --- | --- |
| en_GB-alan | low, medium | Mycroft `mimic3-voices` LICENSE: "All Rights Reserved" |
| en_US-amy | low, medium | Mycroft: no licence found |
| en_US-danny | low | Mycroft: no licence found |
| en_US-kusal | medium | Mycroft `mimic2`: no licence for the data |

## 5. The old Cori Medium (Bryce Beattie's `cori-med`)

Checked on 2026-11-17. The owner hears that this Voice says "library", "anonymized" and "account" correctly. The new
`en_GB-cori-medium` of `cf910fb` (base `hifi_tts_92`, espeak voice `en-gb-x-rp`) says them badly (B10).

**It is one model in three places.** The `.onnx` has the same SHA-256 in each place:
`1899f98e5fb8310154f3c2973f4b8a929ba7245e722b3d3a85680b833d95f10d` (63,531,379 bytes).

- Bryce Beattie's page: `https://sfo3.digitaloceanspaces.com/bkmdls/cori-med.onnx`. Its config has the quality
  `cori-med` and the language code `en`, thus Lexia refuses it (`unknown-quality`).
- `rhasspy/piper-voices` at commit `5041a7a49ae5b3a0f30e6837769eb03fdbb231bd` (2026-10-20), path
  `en/en_GB/cori/medium/`. Its config has the quality `medium` and the code `en_GB` (SHA-256 `e262c16d…`). Lexia can
  use this copy. Git LFS gives the hash of the `.onnx`, and `voices.json` at that commit gives the MD5. `cf910fb`
  replaced the file with the new model.
- The owner's `%LOCALAPPDATA%\Lexia\voices\en_GB-cori-medium.*`: the same two files as `5041a7a`.

**Licence: public domain.** The sources agree:

- Bryce Beattie's page (read on 2026-11-12 and 2026-11-17, no change) says: "License: public domain". He trained
  it from scratch for 640 epochs on about 24 hours of recordings that he collected from LibriVox. All recordings
  are by the reader Cori Samuel.
- The page also gives permission for "any legal and ethical purpose" and for a copy on Hugging Face.
- The MODEL_CARD at `5041a7a` says: dataset `https://librivox.org`, licence public domain, trained from scratch.
- The MODEL_CARD at `cf910fb` names the old Voice as Bryce Beattie's `cori-med-640.ckpt`, trained from scratch.
- LibriVox's page `https://librivox.org/pages/public-domain/` says that all LibriVox recordings are public domain in
  the USA. It also says that a user outside the USA must check the status of the book text in their country.

**What is not known:**

- Bryce Beattie does not list the books that he used. Thus nobody can check the status of each text outside the
  USA. This is the same for norman, kristin and john, and for ljspeech (also LibriVox books).
- His page gives no hash. The match with `5041a7a` is the only check of the file.
- There is no base model and no other dataset, thus no other licence applies.

**For Lexia:** the old Cori Medium is in the category "Free for any use". It has the same licence status as the
Voices norman and ljspeech. Since B13 (owner, Nov 17) Lexia ships it as the default English Voice: `voices/voices.json`
gives it its own `source` (the pinned commit `5041a7a`), `scripts/fetch_deps.ps1` checks both files against the
SHA-256 above and `e262c16d…`, and `licenses/components/voice-en_GB-cori-medium.txt` is its notice. The Download sheet
no longer reads the catalogue (B12), so it cannot offer the NEWER Cori of `cf910fb`. If the owner adds a Voice of the
name `en_GB-cori-medium` to the list, the shipped Voice of that name wins in the Voice list: do not add one.

## 6. Recommendation

- Use these labels in the UI when a Voice needs one: "Free for any use", "Free for any use, credit the Voice" and
  "Non-commercial use only". The owner set the labels aside on Nov 17: the three Voices of the list are public domain.
- Offer only Voices from the first two categories. If the owner offers a non-commercial Voice, show its label.
- Do not offer the Voices that are research only, unclear or with no licence.
- Any quality may be in the list (owner, Nov 17), from Hugging Face or from Bryce Beattie's page. For a Voice from
  Bryce's page (not on the list now) the list needs the SHA-256 of each file (his page gives none; take it from a copy that the owner has
  heard) and Lexia must write a config with a known quality and a full language code (his configs say `cori-med`,
  `mv2` and the code `en`, which Lexia refuses as `unknown-quality`).
- For a Voice that needs credit, show the credit on the Voices page and in the Download sheet. The Jenny licence asks
  for the name "Jenny (Dioco)" where practical.

## 7. The list as it is now (B12), and how to add a Voice

The list is in `src/tts/download_list.cpp` (`kListed`, one row for each Voice). It is compiled into Lexia. The files
of all three Voices are at `rhasspy/piper-voices`, commit `cf910fb59f1d79ab3b625a63993a108751c6c25d`. Section 2 found
them unchanged up to `ae9d835`. The numbers below come from `voices.json` of `cf910fb`. The SHA-256 of each `.onnx` is
the Git LFS id, and Lexia checked that the download has it. Lexia itself checks the size and the MD5.

| Voice | Files | Size (model + config) | `.onnx` SHA-256 (start) |
| --- | --- | --- | --- |
| en_US-kristin-medium | `en/en_US/kristin/medium/` | 63,531,379 + 4,968 bytes | `5849957f` |
| en_US-john-medium | `en/en_US/john/medium/` | 63,531,379 + 4,965 bytes | `789c6c87` |
| en_US-ljspeech-medium | `en/en_US/ljspeech/medium/` | 63,531,379 + 4,972 bytes | `6f52a751` |

The sheet shows the Voices in this order (the owner's order). A Voice that the user has already shows "Installed".

### Adding a Voice to the list

1. Choose a Voice from the sections above. It must be in a category that the owner accepts, and its source must be
   Hugging Face `rhasspy/piper-voices` at a 40-digit commit (a branch name is not allowed). Lexia reads only a Voice
   that has a known quality and a Language that Lexia reads (English now).
2. Read `voices.json` at that commit (`https://huggingface.co/rhasspy/piper-voices/resolve/<commit>/voices.json`).
   Copy the `size_bytes` and the `md5_digest` of the `.onnx` and of the `.onnx.json`.
3. Download the two files into a new, empty folder. Check the size and the MD5. Check the SHA-256 of the `.onnx`
   against the Git LFS pointer (`https://huggingface.co/rhasspy/piper-voices/raw/<commit>/<path>.onnx`).
4. Add one row to `kListed` in `src/tts/download_list.cpp`: the language code, the English name of the language, the
   dataset, the quality, `kPinnedCommitUrl` (or another pinned base URL), and the four numbers. Put the SHA-256 in the
   comment above the row. The file names follow Piper's layout, so the code makes the paths itself.
5. Run `lexia_cli --voices-catalogue` (it shows the row) and `lexia_cli --voice-download <name> --voices-dir <new
   folder>` (it installs the Voice). Run `ctest`: the list tests check the base URL, the paths, the sizes, the MD5
   values and the names.
6. If the Voice needs a label or a credit (section 6), do B12.4 in the plan first. Add the notice of the Voice to
   `docs/third-party.md` if the licence needs it.
7. Do not put a Voice that Lexia ships (`voices/voices.json`) on the list.

## 8. Samples

The owner has WAV samples of all of the Voices above in `%USERPROFILE%\LexiaVoiceSamples` (outside the repository;
nothing is committed). Each Voice reads the same text: the owner's message of Nov 12, then the words that B8 and B10
found incorrect ("library", "anonymized", "account", and also "librarian" and a past "read"). The engine is
`piper-tts` 1.8.0 (the same piper1-gpl version as Lexia), speaker 0 (as Lexia), with the default settings of each
Voice. For mv2, each speaker has a WAV. The WAVs are in one folder for each category that the owner considers
(`wav\1 Free for any use`, `wav\2 Free for any use - credit the Voice`, `wav\3 Non-commercial use only`);
`index.html` and `summary.csv` list them. The owner does not consider the research-only, unclear and no-licence
Voices (Nov 12), so their samples were deleted.

## 9. Sources

- Voice index and cards: https://huggingface.co/rhasspy/piper-voices/tree/ae9d8352b73c1b1750748843e753a504b00e05b0/en
- Checkpoint cards (libritts_r, hifi_tts_92): https://huggingface.co/datasets/rhasspy/piper-checkpoints
- rdh card: https://huggingface.co/rhasspy/piper-voices/raw/main/nl/nl_BE/rdh/medium/MODEL_CARD
- Bryce Beattie's Voices: https://brycebeattie.com/files/tts/
- The old Cori Medium at `5041a7a`: https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_GB/cori/medium
- LibriVox and the public domain: https://librivox.org/pages/public-domain/
- en_AU librivox: https://huggingface.co/DataCraftsmanAustralia/piper-en_AU-librivox-medium (`README.md`, `ATTRIBUTION.md`)
- Jenny licence: https://github.com/dioco-group/jenny-tts-dataset
- OHF-Voice voice-datasets (CC0): https://github.com/OHF-Voice/voice-datasets
- Mycroft mimic3-voices (alan): https://github.com/MycroftAI/mimic3-voices/tree/master/voices/en_UK/apope_low
- CMU ARCTIC: http://www.festvox.org/cmu_arctic/
- L2-ARCTIC: https://psi.engr.tamu.edu/l2-arctic-corpus/
- OpenSLR 60, 83, 109, 141: https://www.openslr.org/
- Alba: https://datashare.ed.ac.uk/handle/10283/3270
- Sam: https://github.com/Sam-Accenture-Non-Binary-Voice/non-binary-voice-files
- SEMAINE: https://github.com/marytts/dfki-semaine-data
- Blizzard 2013 Lessac licence: https://www.cstr.ed.ac.uk/projects/blizzard/2013/lessac_blizzard2013/license.html

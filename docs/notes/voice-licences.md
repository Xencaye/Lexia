# Voice licences (B6)

Update 2026-11-10 (B8): `rhasspy/piper-voices` commit `cf910fb` (2026-11-09) retrained several voices from the
`rdh` (CC0) and `hifi_tts_92` bases. Lexia 0.7 ships `en_GB-cori-medium` (now from `hifi_tts_92`, dataset Hi-Fi
TTS, CC BY 4.0, British phonemes) as the default and `en_US-mike-medium` (from `rdh`, CC0), and no longer ships
`en_US-ljspeech-medium`. Dimitar is now from `rdh` too. The tables below describe the voices as they were on
2026-10-25; `docs/third-party.md` has the current state.

Update 2026-10-28: Lexia offers English only for now (ADR-0007). The Bulgarian findings below are kept for a
later return of the language.

Date: 2026-10-25. Researcher, no code, repo not edited. Not legal advice: it reports what the primary sources say and where they are silent.

Lexia is GPL-3.0-or-later (ADR 0003: "each shipped voice's licence must allow redistribution and goes in the third-party notices"). It ships Piper Medium voices (about 60 MB each, ADR 0004).

## 1. Verdict table

| Voice | Dataset licence (model card) | Model's own licence | Base / lineage | Verdict |
| --- | --- | --- | --- | --- |
| `bg_BG-dimitar-medium` | CC0 (OHF-Voice/voice-datasets) | None stated. Only the HF repo tag `license: mit` | Fine-tuned from `en_US-lessac-medium` | **Do not ship yet** (blocked until cleared). Clean fix below |
| `en_US-lessac-medium` | Blizzard 2013 Lessac research licence: research only, excludes voice-synthesis products | None stated | From scratch on that dataset | **Do not ship** |
| `en_US-kristin-medium` | Public domain (LibriVox) | None stated | From scratch (author's page) | **Recommended** |
| `en_US-norman-medium` | Public domain (LibriVox) | None stated | From scratch | **Recommended** |
| `en_US-ljspeech-medium` | Public domain (LJ Speech; author says "no restrictions") | None stated | From scratch | **Recommended** |
| `en_GB-cori-medium` | Public domain (LibriVox) | None stated | From scratch | Safe alternative (British) |
| `en_US-john-medium` | Public domain (LibriVox) | None stated | Fine-tuned from Kristin (itself clean) | Safe alternative (male) |
| All other en_US / en_GB Medium voices | See section 3 | None stated | Mostly fine-tuned from lessac | Do not ship (lessac lineage and/or NC / SA dataset), except `bryce` (unknown base) |

Open before anything ships: the two decisions in section 6.

## 2. Q1: `bg_BG-dimitar-medium` and lessac

### What the sources say

- Dimitar MODEL_CARD, verbatim: dataset URL `https://github.com/OHF-Voice/voice-datasets`, "License: CC0", "Finetuned from U.S. English lessac voice (medium quality)". The card has no "model licence" line. Piper's VOICES.md says the card "contains important licensing information", so for this voice the card's only licensing information is the dataset licence.
- The dataset is CC0. The repo README says "These datasets are licensed under CC0 (public domain). If you use them, please cite this repository!". `CITATION.cff` has `license: CC0-1.0`. Bulgarian entry: `bg_BG / dimitar`, added 2024-07-31, 115 MB zip. The README does not say who recorded it ("crowdsourced ... for Home Assistant's Year of Voice by Nabu Casa"). The voice was added to piper-voices on 2025-10-31 (HF commit "Add dimitar (bg)").
- Lessac's MODEL_CARD: dataset `https://www.cstr.ed.ac.uk/projects/blizzard/2013/lessac_blizzard2013/`, licence link `.../license.html`, "Trained from scratch."
- The CSTR dataset page says the data "is released under a license for non-commercial use only". The licence page (a click-through "Research Licence Agreement" between Voice Factory International and Lessac Technologies and the user) says:
  - "Research Purposes" means purposes "associated with research and exploration ... using, incorporating or based upon the Materials (in whole or in part) or any copies of the Materials", and "excludes ... developing, adapting, amending or otherwise using the Materials for any commercial purpose, including the development, marketing, commercialisation, sale or licencing of voice synthesis or speech recognition products or services".
  - Cl. 3.2: "non-exclusive and non-transferable licence, without the right to sub-licence ... exclusively for Research Purposes only".
  - Cl. 7: the user agrees "not to lend, hire, sell, distribute or otherwise part with the Materials".
  - Cl. 8: use "by the User" only; no third party use.
  - "Materials" is defined as audio recordings, text, speech clips, prompts, notes, labels. A trained model is not listed. The licence does not mention trained models or derivatives.
- Piper statements:
  - piper1-gpl `docs/VOICES.md`: "Piper is intended for personal use and text to speech research only; we do not impose any additional restrictions on voice models. Some voices may have restrictive licenses, however, so please review them carefully!"
  - Maintainer (synesthesiam), HF piper-voices discussion #94 (about Polish lessac-derived voices, reply dated "Aug 6", year not shown on the page): "This is something that I don't think has been tested yet legally, so I'm not sure what the answer is." and "I've trained a new base model from LibriTTS-R (CC-BY) and will slowly be re-training many of the Piper voices so that they can at least be used for commercial purposes." The questions in that thread (are lessac fine-tunes derivative works; is a "Base model licence" field possible) are still unanswered and the thread is open.
  - Maintainer, rhasspy/piper discussion #271 (2023): "I'm not a lawyer", Piper "does not impose any additional licenses on the checkpoints or voice models", "responsibility of the end user to make the ultimate judgement". A community member there (StoryHack, accepted answer, not a maintainer) says the Blizzard licence "will not allow any derivative to be used commercially".
  - OHF-Voice/piper1-gpl issue #314 (opened 2026-09-28, still open, no replies) asks for the actual grant covering lessac-initialised Russian voices. No answer exists.
  - Repo metadata: `rhasspy/piper-voices` and `rhasspy/piper-checkpoints` are tagged `license: mit`. That is a repo-wide tag, not a per-voice grant.

### Does fine-tuning carry the restriction into Dimitar?

The sources do not say. Neither the licence text, the model card, nor the maintainers answer it. The maintainer calls it untested. I will not guess; the facts are:

1. Dimitar's weights were initialised from lessac weights. Whether that makes Dimitar "based upon the Materials" is a legal question nobody with authority has answered.
2. If it does, the licence is "Research Purposes only", and the definition names "development ... of voice synthesis ... products" as commercial. A free GPL app is not a sale, but the definition is not limited to sales. The text is ambiguous for free software and no source resolves it.
3. The grant is personal and non-transferable with no sublicence. Piper's author accepted that licence for his own use. No source shows a right to pass lessac-derived rights on to Lexia's users. The MIT tag on the HF repo cannot give away rights the trainer may not have held.

### Can Lexia redistribute it inside a GPL-3 installer?

- GPL side: no problem. A voice is a separate data file next to the exe, an "aggregate" under GPLv3 section 5; it does not need to be GPL, only redistributable. CC0 data is fine.
- Redistribution side: not established, because of the lessac lineage above.

**Verdict: do not ship `bg_BG-dimitar-medium` as it stands.** "Ship with notice" does not fix it, because a notice cannot supply permission that may be missing. The dataset half is clean. The unresolved half is the lessac initialisation.

Ways to unblock, in order of cleanliness:

1. **Retrain Dimitar from a clean base** and ship that. The data is CC0 (`bg_BG-dimitar.zip`, 115 MB). A clean base exists: `rhasspy/piper-checkpoints/_base_model`, MODEL_CARD "Trained from scratch on LibriTTS-R train-clean-360 with MRD and SDP", dataset CC BY 4.0 (OpenSLR 141). Needs Piper 1.5.0+ and a GPU run by the owner (training is a separate task). The published checkpoint `bg/bg_BG/dimitar/medium/epoch=5097-step=152516.ckpt` is lessac-derived, so do not continue from it. The maintainer says he is retraining voices this way but no Bulgarian entry has changed (the Dimitar card still says lessac).
2. **Written statement from the Open Home Foundation / synesthesiam** (issue on OHF-Voice/piper1-gpl, or voice@openhomefoundation.org, the address given in `CITATION.cff` and the piper1-gpl README) that lessac-derived voices may be redistributed in free software. Issue #314 shows how slowly this goes.
3. **Permission from the licensors** of the lessac data (Lessac Technologies Inc., 256 Highland Street, West Newton, MA 02465, www.lessactech.com; Voice Factory International). Cl. 9 of their licence tells licensees to refer such requests to them.
4. **Accept the risk and ship with a notice.** Possible, but it is a legal call for the owner, not something the sources clear. If chosen, the notice must say plainly that the voice was fine-tuned from lessac (text in section 5).

The same applies to the plan's "no model licence" remark: confirmed. The card gives none.

## 3. Q2: every en_US / en_GB Medium voice in `voices.json`

`voices.json` (178 voices at the time of writing) has no licence field; licences come only from the per-voice MODEL_CARD. 26 en_US/en_GB Medium voices; sizes are the `.onnx` size (60 MB unless stated).

| Voice | Dataset licence on the card | Lineage on the card | Ship in GPL-3 app? |
| --- | --- | --- | --- |
| en_US-kristin | public domain (LibriVox) | Trained from scratch, 2000 epochs | Yes |
| en_US-norman | public domain (LibriVox) | Trained from scratch, 1200 epochs | Yes |
| en_US-ljspeech | public domain | Trained from scratch, 1000 epochs | Yes |
| en_GB-cori | public domain (LibriVox) | Trained from scratch, 640 epochs | Yes |
| en_US-john | public domain (LibriVox) | Fine-tuned from Kristin | Yes (Kristin is clean) |
| en_US-bryce | public domain (author's own voice, no dataset URL) | "Finetuned ... from an unreleased voice which had 2500 epochs" | Unknown base: avoid |
| en_US-lessac | Blizzard 2013 research licence | From scratch | No |
| en_US-libritts_r (77.7 MB) | CC BY 4.0 | Conflict: piper-voices card says "Fine-tuned from English lessac medium on train-clean-360"; the piper-checkpoints card says "Trained from scatch. Requires Piper 1.5 or higher." | No until the conflict is resolved |
| en_US-joe | CC0 (OHF) | From lessac | No (lessac) |
| en_US-mike | CC0 (OHF; added to the dataset repo 2026-10-24, only a day before this note) | From lessac | No (lessac) |
| en_US-amy | "See URL" (Mycroft mimic3-voices) | From lessac | No |
| en_US-kusal | "See URL" (Mycroft mimic2) | From lessac | No |
| en_US-arctic | "See LICENSE file" (CMU Arctic, festvox) | From lessac | No |
| en_US-sam | Apache-2.0 | From lessac | No (lessac) |
| en_US-hfc_female / hfc_male | CC BY-NC-SA 4.0 | From lessac | No (NC and SA) |
| en_US-ryan | CC BY-NC-SA 4.0 | From lessac | No (NC and SA) |
| en_US-l2arctic | CC BY-NC 4.0 | From lessac | No (NC) |
| en_US-reza_ibrahim | CC0 | From lessac | No (lessac); also a Persian/English Quran dataset |
| en_GB-alba | CC BY 4.0 | From lessac | No (lessac) |
| en_GB-aru | CC BY 4.0 | From lessac | No (lessac) |
| en_GB-vctk | CC BY 4.0 | From lessac | No (lessac) |
| en_GB-northern_english_male | CC BY-SA 4.0 | From lessac | No (SA and lessac) |
| en_GB-alan | "See URL" (Mycroft mimic3-voices apope_low) | From lessac | No |
| en_GB-jenny_dioco | "See URL" (dioco-group/jenny-tts-dataset) | From lessac | No |
| en_GB-semaine | CC BY-NC-SA 4.0 | From lessac | No (NC and SA) |

Counts: 5 voices have a clean from-scratch lineage and permissive dataset (kristin, norman, ljspeech, cori, and john via kristin). Everything else is lessac-derived or carries an NC/SA dataset licence, except `bryce` (unknown base).

Evidence for "from scratch": the card text plus Bryce Beattie's own page (https://brycebeattie.com/files/tts/), which says "I want there to be more voices available without restrictive licenses" and lists each as "Trained from scratch" with "License: public domain". Note these are self-declared by one volunteer; no independent audit exists.

Not in scope but relevant: `en_US-libritts-high` (Trained from scratch on train-clean-360, CC BY 4.0 per its card) is a clean multi-speaker voice but High quality, not Medium, and larger.

## 4. Recommendation (the owner picks by ear)

1. **`en_US-kristin-medium`**, female US. From scratch, LibriVox public domain.
2. **`en_US-norman-medium`**, male US. From scratch, LibriVox public domain.
3. **`en_US-ljspeech-medium`**, female US. From scratch, on LJ Speech, whose author states "There are no restrictions on its use." (https://keithito.com/LJ-Speech-Dataset/). Best-documented dataset provenance of the three.

Alternates if the owner wants British English or another male voice: `en_GB-cori-medium` (from scratch) and `en_US-john-medium` (from Kristin).

Caveats for all of them (sources are ambiguous or silent here):
- "Public domain" for LibriVox-based voices rests on LibriVox's own statement that "all our recordings are public domain (definitely in the USA, and maybe in your country as well)" (https://librivox.org/pages/public-domain/). The note about other countries is LibriVox's, not mine. The per-voice card gives no reader list or book list, so I cannot check each source recording.
- The CC0 and public-domain statements are made by the trainer (Bryce Beattie); there is no signed grant.
- Piper's VOICES.md says Piper "is intended for personal use and text to speech research only". It is not a licence term on voices ("we do not impose any additional restrictions on voice models") but is worth knowing for a distributed app.
- GPL fit: PD / CC0 / CC BY data shipped as separate files is an aggregate (GPLv3 section 5); nothing here forces the voice to be GPL. (GPLv3 text from memory of https://www.gnu.org/licenses/gpl-3.0.html, not fetched in this pass.)

## 5. Q3: notice text for the third-party notices

Nothing below is legally required for public-domain voices; the texts are courtesy credits, and the CC BY text is required only if a CC BY voice is chosen (none of the recommended ones is). Wording is mine, built from the sources. Replace `<...>` with final values.

### en_US-kristin-medium, en_US-norman-medium, en_GB-cori-medium, en_US-john-medium

```
Piper voice "<kristin|norman|cori|john> (medium)"
Voice model by Bryce Beattie (https://brycebeattie.com/files/tts/), trained with Piper
from LibriVox recordings (https://librivox.org). Training data and model: public domain
(per the model card in rhasspy/piper-voices). No attribution is required; credit given
as a courtesy. Provided as is, without warranty.
```

(For `john`, add: "Fine-tuned from the voice 'kristin'.")

### en_US-ljspeech-medium

```
Piper voice "ljspeech (medium)"
Trained by Bryce Beattie (https://brycebeattie.com/files/tts/) on the LJ Speech Dataset
by Keith Ito (https://keithito.com/LJ-Speech-Dataset/), which is in the public domain
(texts from public domain books, audio recorded for LibriVox). Public domain; no
attribution is required; credit given as a courtesy. Provided as is, without warranty.
```

### bg_BG-dimitar-medium (only after it is cleared, section 2)

The dataset asks to be cited ("If you use them, please cite this repository!"). The lineage must be stated honestly:

```
Piper voice "dimitar (medium)", Bulgarian
Training data: Open Home Foundation Voice datasets, bg_BG/dimitar
(https://github.com/OHF-Voice/voice-datasets), CC0 1.0. Cited as requested by the
dataset: Open Home Foundation, "Open Home Foundation Voice" (Nabu Casa crowdsourced
voice datasets).
Model: rhasspy/piper-voices bg/bg_BG/dimitar/medium (model card gives no model licence).
```

If the clean-base retrain is shipped instead, replace the model line with: "Model: retrained by the Lexia author from the Piper base model (trained from scratch on LibriTTS-R, CC BY 4.0, https://www.openslr.org/141/), and add the LibriTTS-R credit: "Koizumi, Zen, Karita, Ding, Yatabe, Morioka, Bacchiani, Zhang, Han, Bapna, LibriTTS-R: A Restored Multi-Speaker Text-to-Speech Corpus, arXiv 2023." The OpenSLR page asks users to cite the dataset paper and the licence is CC BY 4.0, so this credit is needed. The licence link is https://creativecommons.org/licenses/by/4.0/ (standard CC text; not fetched).

If the lessac-derived Dimitar is shipped despite section 2, add: "This model was fine-tuned from the Piper voice en_US-lessac-medium, trained on the Lessac Technologies / Voice Factory Blizzard Challenge 2013 data, which is licensed for research purposes. Its redistribution terms are not stated by the model's authors."

### Piper itself and espeak-ng

Out of scope for B6 (engine notices), but Lexia's H2 notices should keep these separate from voice entries.

## 6. Open questions and points where sources are silent or ambiguous

1. **Does a lessac fine-tune inherit the Blizzard research restriction?** Silent. The licence never mentions trained models; the maintainer calls it legally untested. Blocks Dimitar. Decision for the owner: retrain from `_base_model` (cleanest), or ask OHF / the licensors in writing, or accept the risk.
2. **Is a free GPL app "commercial" under the Blizzard definition?** Ambiguous. "Development ... of voice synthesis ... products or services" is listed as commercial, and the clause is not limited to money.
3. **No voice has a model licence line.** The only model-level statements are the HF repo tag `license: mit` and VOICES.md "we do not impose any additional restrictions".
4. **`en_US-libritts_r-medium`:** the two model cards contradict each other (lessac fine-tune vs. from scratch). Unresolved.
5. **`en_US-bryce-medium`:** base is "an unreleased voice"; lineage unknowable.
6. **The "from scratch" claims** are self-declared by one volunteer, and LibriVox's public domain status is LibriVox's own claim, with a caveat for other countries.
7. **Recorder of Dimitar** is not named, and nothing shows how the contributor released the recordings to CC0 (the repo only states it).
8. **Maintainer reply date in HF discussion #94** shows "Aug 6" with no year.
9. **Piper's VOICES.md "personal use and research only" line** is not defined further; I read it as a statement of intent.
10. **The new LibriTTS-R base** exists (`piper-checkpoints/_base_model`) and other languages use it (`et_EE-news`, `ko_KR-kss`, `ja_JP-hi_fi_captain` and more); Bulgarian has not been re-trained from it. Bulgarian has no other Piper voice in `voices.json`, so there is no drop-in alternative.
11. This note is research, not legal advice; an owner shipping to the public may want a lawyer's view on item 1.

## 7. Sources

- Dimitar model card: https://huggingface.co/rhasspy/piper-voices/raw/main/bg/bg_BG/dimitar/medium/MODEL_CARD
- Lessac model card: https://huggingface.co/rhasspy/piper-voices/raw/main/en/en_US/lessac/medium/MODEL_CARD
- All other model cards: `https://huggingface.co/rhasspy/piper-voices/raw/main/<lang>/<locale>/<voice>/medium/MODEL_CARD` (read for the 26 voices in section 3)
- Voice index: https://huggingface.co/rhasspy/piper-voices/raw/main/voices.json
- HF repo (licence tag mit; commit history): https://huggingface.co/rhasspy/piper-voices and https://huggingface.co/api/models/rhasspy/piper-voices/commits/main
- Blizzard 2013 Lessac dataset page: https://www.cstr.ed.ac.uk/projects/blizzard/2013/lessac_blizzard2013/
- Blizzard 2013 Lessac licence: https://www.cstr.ed.ac.uk/projects/blizzard/2013/lessac_blizzard2013/license.html
- OHF voice datasets (README, CITATION.cff): https://github.com/OHF-Voice/voice-datasets (branch `master`)
- Piper voices doc: https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/VOICES.md
- Piper training doc: https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/TRAINING.md
- Piper checkpoints (base model, Dimitar checkpoint): https://huggingface.co/datasets/rhasspy/piper-checkpoints/tree/main/_base_model and https://huggingface.co/datasets/rhasspy/piper-checkpoints/tree/main/bg/bg_BG/dimitar/medium
- HF discussion #94 (lessac lineage, Polish voices): https://huggingface.co/rhasspy/piper-voices/discussions/94
- Piper discussion #271 (voices licensing): https://github.com/rhasspy/piper/discussions/271
- piper1-gpl issue #314: https://github.com/OHF-Voice/piper1-gpl/issues/314
- Bryce Beattie's voices page: https://brycebeattie.com/files/tts/
- LJ Speech: https://keithito.com/LJ-Speech-Dataset/
- LibriVox public domain: https://librivox.org/pages/public-domain/
- LibriTTS-R (OpenSLR 141): https://www.openslr.org/141/
- Lexia ADRs: `docs/adr/0003-gpl-3-licence.md`, `docs/adr/0004-piper-replaces-sapi.md`; plan task B6 in `docs/Lexia — Code Review & Agent Plan.md`

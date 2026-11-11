# Piper neural voices replace SAPI for all speech

Lexia speaks through Piper voices it ships, not Windows SAPI 5. SAPI only sees "Desktop" voices
(on a typical PC: English David and Zira); the installed Bulgarian voice (Microsoft Ivan) is a
OneCore voice SAPI cannot use, so Bulgarian text was read with an English voice. Piper has a
Bulgarian voice, sounds the same on every PC, and lets Lexia play the audio itself.

## Considered Options

- **SAPI reading the OneCore voice list** (registry-category trick): unofficial, may break with
  Windows updates, and quality depends on what each PC has installed.
- **WinRT `Windows.Media.SpeechSynthesis`**: sees OneCore voices officially, but still depends on
  per-PC voices and would also need Lexia to play the audio itself.
- **Piper only for Languages without a SAPI voice**: two engines, two audio paths, Announcement
  pause-and-resume written twice.

## Consequences

- Lexia owns audio playback, so pausing a Reading for an Announcement and resuming it is Lexia's job.
- About 60 MB per shipped medium-quality voice. (Lexia ships and downloads Medium Voices; since B11, Nov 11, a Voice the user adds
  from a file may also be X-low, Low or High.)

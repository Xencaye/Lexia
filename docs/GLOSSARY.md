# Lexia

Lexia reads text aloud from anywhere on the screen for people with dyslexia: the user freezes
the screen, points at text, and hears it.

## Session

**On / Off**:
Whether Lexia is listening for its keys. While Off, no keys are taken from other programs and no
Session can start.
_Avoid_: enable overlay, active, armed

**Session**:
The span from pressing the Read key until the user leaves the Overlay (Read key again, Esc,
right-click, or the Control bar's Help button, which also opens the Settings window on its Help page). One Session
has one Shot, one Analysis and any number of Picks. Stopping speech does not end it. Leaving does not stop the
Reading queue: it goes on playing (and, with read-along, its bar stays on the screen until the Readings are done).
_Avoid_: overlay session, selection mode

**Shot**:
The frozen copy of one monitor taken when a Session starts; everything in the Session refers to it,
not to the live screen.
_Avoid_: screenshot, snapshot, capture, freeze

**Active monitor**:
The monitor showing the foreground window; the one a Shot is taken of.
_Avoid_: current screen, main monitor

**Overlay**:
The full-screen window that shows the Shot during a Session and never takes focus.
_Avoid_: selector, selection overlay

**Control bar**:
The movable strip of buttons on the Overlay (Pause/Resume, Skip, Stop, Help, Size) that also
shows whether the Analysis is still running: Pause, Skip and Stop are disabled while it runs, and while nothing
speaks. Help ends the Session like a right-click and opens the Help page; Size switches between the two sizes,
Large (icons and words, with Help) and Small (icons only). It exists only during a Session, and it does not stay
with the read-along layer after the Session.
_Avoid_: toolbar, hint bar, controls

**Analysis**:
The whole-Shot text recognition that runs in the background at the start of a Session and produces
its Blocks. Switching Language during a Session runs it again on the same Shot.
_Avoid_: scan, pre-pass

**Settings window**:
The ordinary window, opened from the tray icon (or by starting Lexia again, or by the Control bar's Help), where
the user changes Lexia's settings and manages Voices. Unlike the Overlay it takes focus. It is native (Direct2D,
ADR-0009), a fixed 800 x 600 DIPs, with no Windows title bar (it has its own minimise and close buttons, K10);
closing it does not end Lexia. Its pages are home, voices, settings, report, help and about (the bold words of the
frame); the Master volume is in the frame, on every page.
_Avoid_: options, preferences, config

**Key card**:
The card on the Settings page that says what the selected Key or switch does (a Key, or one of the switches
read-along, run at startup, start minimized, run as admin, dark mode, left-handed). Its read button (a speaker)
reads the card aloud, and while that card is being read it is a stop button (K11).
_Avoid_: help panel, description box

## Text

**Line**:
One run of text the detector found on a single visual line. It may be only part of a printed line
(two columns give two Lines).
_Avoid_: fragment, box, detection

**Block**:
One or more Lines a reader would read as one unit: a paragraph, a heading, a single name. The
smallest thing a Click reads.
_Avoid_: paragraph, region, group

**Reading order**:
The order a person would read text in: down each column before moving across to the next, and the
Lines within a Block top to bottom.
_Avoid_: sort order, layout order

## Picking

**Pick**:
One user action on the Overlay that asks for text to be read: a Click or a Drag. A Pick may find
nothing to read.
_Avoid_: selection, choice

**Click**:
A Pick that reads the Block under the pointer.

**Drag**:
A Pick that reads the text inside a rectangle the user draws. It recognises that rectangle afresh,
independently of the Analysis.
_Avoid_: area select, lasso

**Hover**:
The Block a Click would read right now, shown on the Overlay before the user clicks.
_Avoid_: preview

**Picked area**:
What stays highlighted on the Overlay after a Pick that found text, for the rest of the Session.
A Pick that found nothing leaves none.
_Avoid_: read area, selection, highlight

## Speech

**Reading**:
The speaking of one Pick's text. Readings wait in the Reading queue and play one after another in
Pick order, across Sessions.
_Avoid_: utterance, playback

**Reading queue**:
The Readings not yet finished, in Pick order. It is emptied only by Stop, or by starting a new
Session while it is paused; starting a Session while it is playing keeps it, and new Picks join
the end.
_Avoid_: playlist, backlog, buffer

**Read-along**:
The feature that lets the user follow a Reading on the screen: a bar under the word being heard. It exists
only with the setting "enable read-along" (`readAlong` in settings.json, Off by default: then nothing is drawn
at all, as in 0.5). It has three parts: the Heard word (what is being heard), the Read-along bar (what is drawn)
and the Read-along layer (what keeps drawing it after the Overlay is left). Built in G1 (the bar on the Overlay)
and G2 (the layer).
_Avoid_: word mark, word highlight, karaoke, follow-along

**Heard word**:
The word of a Reading that can be heard right now (the one a Pause holds). None while an
Announcement plays, and after Skip or Stop. Lexia knows it from the timing of Piper's phonemes, not
from the screen. It is the input of the Read-along bar.
_Avoid_: current word, spoken word

**Read-along bar**:
The underline bar drawn under the Heard word, in the hover colour, for the Readings Picked in a
Session. With read-along On the Overlay draws it while the Session lasts, and the Read-along layer goes on
drawing it after the Session ends, until the Session's Readings are done. It sits under the part of its Line
that the word takes up, which is an estimate, because OCR finds Lines, not words.
_Avoid_: word mark, word highlight, cursor

**Read-along layer**:
The click-through window that draws the Read-along bar after the Overlay is left (only with read-along On). It
covers the monitor of the Session, draws only the bar, never takes focus or a click, and ends with the last
Reading of its Session, or on Stop, Off, Quit or a new Session. The Control bar does not stay with it.
_Avoid_: word mark layer, second overlay, highlight window

**Skip**:
Jumping to the next Block in the Reading queue; past the last Block of a Reading it moves on to the
next Reading. If paused, it resumes from there. During an Announcement it ends only the
Announcement.
_Avoid_: next, skip line

**Stop**:
Cutting off the current Announcement and emptying the Reading queue.
_Avoid_: cancel, mute

**Announcement**:
A short status message spoken by Lexia itself, as opposed to a Reading of on-screen text. It
pauses the current Reading, which carries on once the Announcement is done.
_Avoid_: notification, prompt, message

**Language**:
The language the user has chosen; it decides how text is recognised. Only an offered Language can
be chosen; for now that is English (ADR-0007).
_Avoid_: OCR language, locale

**Alphabet**:
The characters a Language allows recognition to produce: its own letters plus Latin letters,
digits and punctuation.
_Avoid_: charset, dictionary

**Lookalike**:
A letter that looks the same in Latin and in Cyrillic (Latin o and Cyrillic о). The recogniser
sometimes returns the wrong one inside a word; the Lookalike fix converts the Lookalikes of a
mixed-script word to the script the word mostly is.
_Avoid_: homoglyph, confusable

**Interface language**:
The language Lexia speaks and writes its own words in (Announcements, the Control bar, the tray menu and the
Settings window). Chosen separately from the Language (`interfaceLanguage` in settings.json, no control in the UI
yet: English only, ADR-0007), from the same set of languages.
_Avoid_: UI language, app language, display language

**Voice**:
A neural voice that speaks one language. Each Language has the Voice the user picked for it; a
stretch of text is spoken by the Voice of the Language its script matches, not necessarily the
current Language.
_Avoid_: TTS language, speaker, narrator

**Stretch**:
Part of a Block's text in one script that is spoken by one Voice. A run of two or more words in a script is a
Stretch of its own; a single word of another script stays in the Stretch around it. The recogniser stays the
Language; only the Voice changes from Stretch to Stretch.
_Avoid_: segment, chunk (a chunk is a piece of audio)

**Voice controls**:
The three settings of a Voice: Speed, Pitch (in semitones; the speech keeps its length) and Volume. Each
has a range and a default, and Reset puts it back to the default. Lexia keeps the current controls of every
Voice in settings.json.
_Avoid_: voice settings, sliders

**Preset file**:
A small `.json` file the user saves from the Voices page ("save preset") to keep one Voice's controls, and loads
again later ("load preset"). A loaded file replaces the controls of the Voice it names: the active Voice, or
another installed Voice (then the active Voice stays); for a Voice that is not installed nothing changes. The
dialogs start in `%LOCALAPPDATA%\Lexia\presets`. There are no named Presets inside Lexia any more; a Preset file
is just a file.
_Avoid_: profile, style, preset (alone: it means the file)

**Master volume**:
The one volume for everything Lexia says, 0 to 100, in the frame of the Settings window (on every page, at the bottom
right; UI words "Lexia volume"). It is multiplied into the
Volume of every Voice. Saved as `speech.volume`.
_Avoid_: system volume, main volume

**Voice library**:
The Voices Lexia can speak with: the ones that ship in `voices\` next to the exe and the ones the user
downloaded or added, which live in `%LOCALAPPDATA%\Lexia\voices`. Each Voice is attached to the Language its
config says it speaks. Only Medium Voices for a Language Lexia can read are accepted. Downloading happens
only when the user asks (ADR-0005).
_Avoid_: voice store, voice pack, voice manager

## Keys

Keys are named by role; the physical key is a setting (defaults in brackets).

**Read key** (F8):
Starts a Session, or leaves the current one.
_Avoid_: hotkey, F8

**Pause key** (F7):
Pauses the current Reading, or resumes it. Only active while something is being spoken or paused.
_Avoid_: play/pause, F7

**Speech key** (F6):
Tap to Skip (two taps in a row skip two Blocks), triple-tap to Stop. Only active while something is being spoken or paused.
_Avoid_: Stop key, Skip key, skip/stop, F6

**Language key** (unbound):
Switches to the next offered Language whose models are installed (with only English offered, it says that no
other language is installed).
_Avoid_: switch key

**Left-handed**:
A setting that swaps the mouse buttons on the Overlay: the right button Picks (click or drag) and the left button
Leaves. Takes effect at the next Session. It is Lexia's own; Windows' "swap primary and secondary buttons" is
applied by Windows first, and both together give the normal buttons again.
_Avoid_: mirrored, swapped mouse

**Run as admin**:
A setting that makes Lexia start elevated (with administrator rights), so that it works over games that run as
administrator (Windows blocks a normal program from them). Off by default. Turning it on, or starting Lexia with
it on, shows the Windows UAC prompt; if the user says no, Lexia goes on without it. Setup has the same choice
as a task (K8). See ADR-0008.
_Avoid_: elevated mode, admin mode

Lexia has no quit key; quitting is done from the tray menu (D2). Closing the Settings window does not quit.
(With `lexia.exe --console`, Ctrl+C also ends it, but not cleanly: speech is cut off.)

The UI words are American English (`strings/table_en.cpp`: "minimize", "analyzes", "colors"); this glossary and
the other docs keep the spelling of the project (minimise, colour).

## Testing

**Fixture**:
A saved Shot together with the Blocks and Reading order a person expects for it.
_Avoid_: sample, test image, golden

**Fixture score**:
How closely Lexia's Blocks and Reading order for a Fixture match what it expects.
_Avoid_: accuracy, layout score

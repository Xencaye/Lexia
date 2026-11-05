LEXIA @LEXIA_VERSION@ - reads text on your screen out loud
================================================

Lexia reads text aloud from anywhere on the screen: web pages, images, videos and
games (including full-screen games). It works offline - nothing is sent anywhere.

HOW TO START
  1. Unzip this folder anywhere you like (for example on the Desktop).
  2. Double-click lexia.exe. A black window opens - leave it open while you use Lexia.
     You do not need to install anything or be an administrator.
  3. Turn your speakers on.

HOW TO USE (default keys)
  F8   Freeze the screen and open the selection overlay.
         - Hover over text to preview what would be read.
         - Click on text to read that whole paragraph.
         - Click and drag to read an area.
       You can select as many things as you like. They are read one after another.
       Press F8 again, or right-click, to leave the overlay.
  F7   Pause or resume reading (only while Lexia is speaking).
  F6   Skip to the next paragraph; press twice quickly to stop reading (only while speaking).

  To close Lexia, close its black window (or press Ctrl+C in it).


  The overlay appears on the monitor you were last working on.

SETTINGS
  The first time you start Lexia it creates a file called settings.json in the folder
  %LOCALAPPDATA%\Lexia (type that into the Explorer address bar). If a settings.json
  already sits next to lexia.exe, Lexia keeps using that one. Open it with Notepad to change:
    - the keys (for example "F8" or "Ctrl+Alt+R"; letters need Ctrl, Alt or Shift)
    - the reading speed and volume
    - the highlight colours
  Save the file and Lexia applies the changes at once; there is no need to restart. If you
  break the file while Lexia is running, it says so and keeps the settings it had. If it is
  broken when Lexia starts, Lexia uses the default settings. Deleting settings.json creates
  a fresh one the next time Lexia starts.

GOOD TO KNOW
  - While Lexia is running, F8 only works for Lexia. F6 and F7 are Lexia's only while it
    is speaking or paused; the rest of the time they work in other programs and games.
    Close Lexia to get F8 back, or pick other keys in settings.json.
  - Windows may say "Windows protected your PC" the first time. This is because Lexia
    is a new program that is not digitally signed yet. Click "More info", then
    "Run anyway".
  - Lexia reads English only for now, and its own words are in English. More languages
    may come later.
  - If Windows reports a missing file such as VCRUNTIME140.dll, install the
    "Microsoft Visual C++ Redistributable (x64)" from microsoft.com and try again.

UNINSTALL
  Close Lexia and delete the folder.

LICENCE
  Lexia is free software under the GNU General Public License, version 3 or later: see
  LICENSE. The programs and voices it is built on, and their licences, are listed in
  THIRD-PARTY-NOTICES.txt.

This is a test version (Lexia @LEXIA_VERSION@) - please tell me what works and what doesn't!

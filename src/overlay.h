#pragma once
// The Overlay: shows the frozen screenshot full-screen and lets the user hover / click / drag to
// choose what to read. The Control bar (controlbar.h) floats above it for the length of a Session.
#include <windows.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "capture.h"
#include "common.h"
#include "control_bar_layout.h"
#include "controlbar.h"
#include "jobqueue.h"
#include "read_along.h"
#include "recognize.h"

enum class SelKind { Cancel, Drag, Click };

struct Selection {
    SelKind kind = SelKind::Cancel;
    RECT rect = {};    // Drag: the chosen area
    POINT point = {};  // Click: where
    int block = -1;    // Click: index of the block under the click (-1 = nothing there)
};

// How the Overlay and its Control bar look / what they tell the user.
struct OverlayStyle {
    COLORREF hoverColor = RGB(255, 200, 0);   // block under the mouse, area being dragged
    COLORREF pickedColor = RGB(80, 200, 120); // areas already read
    std::string pauseKey = "F7";              // shown in a tooltip (UTF-8); "" = the action has no key
    std::string speechKey = "F6";
    std::string interfaceLanguage = "en";     // the language of the Control bar (strings/)
    std::string theme = "system";             // "system", "light" or "dark": the Control bar's
    BarSize barSize = BarSize::Large;         // the Control bar's size and place, as remembered
    int barX = -1, barY = -1;                 // offsets from the monitor's top left corner; -1 = default
    bool leftHanded = false;                  // the right button Picks and the left one Leaves (mouse_role.h); fixed for the Session
};

// Recognises an area of the Session's Shot. Called on a JobQueue thread, never the UI thread.
using Recognizer = std::function<Recognition(Rect area, bool isDrag)>;

// Called on the UI thread for every Pick, in Pick order. `picked` holds the Blocks the Pick
// covers: the one Block of a Click (with the Analysis's time), or what the Drag recognised; a
// Click on no Block (counted only once the Analysis is done) comes with none. The handler reads
// the text aloud, or announces that there is none, and says whether it found any (the area then
// stays highlighted) and, if it queued the text for speech, the id of that Reading (G1): the Session
// keeps the Pick's Blocks under that id and draws the word mark for the words of that Reading.
struct PickOutcome {
    bool found = false;        // there was text: keep the area highlighted
    uint64_t readingId = 0;    // Speaker::SpeakReading's id; 0 = nothing was queued (no word mark)
};
using SelectHandler = std::function<PickOutcome(const Selection&, const Recognition& picked)>;

// What the Session calls back into. All of them run on the UI thread.
struct OverlayCallbacks {
    SelectHandler onSelect;
    std::function<void()> onPause;           // the Pause key, or the Control bar's Pause / Resume
    std::function<void()> onSkip;            // the Control bar's Skip
    std::function<void()> onStop;            // the Control bar's Stop
    std::function<void()> onSpeechKey;       // the Speech key was pressed
    std::function<void()> onSwitchLanguage;  // the language key was pressed
    // The Control bar's Help: the Session ends like a right-click does, and only then, with the Overlay
    // and the bar gone, RunOverlay calls this (the app opens the Settings window on its Help page).
    std::function<void()> onHelp;
    std::function<SpeechState()> speechState;  // is anything being spoken or paused (the bar polls it)
    // The word that is heard now (Speaker::HeardWordNow), polled on a window timer (~30 ms) to draw the
    // word mark: an underline bar in `OverlayStyle::hoverColor` under the word, for Readings Picked in
    // this Session. Empty function = no word mark.
    std::function<std::optional<HeardWord>()> heardWord;
    // The Control bar's size or place changed: size, x, y (offsets from the monitor's corner). The
    // app remembers them.
    std::function<void(BarSize, int, int)> onBarChanged;
    // Goes up by one whenever the Language changes, from the language key or from anywhere else.
    // The Session sees it and runs the Analysis again on the same Shot; Picked areas stay.
    const unsigned* languageEpoch = nullptr;
};

// What a Session leaves behind when it ends (G2): the Readings it Picked with their Blocks as the Pick
// found them (Shot coordinates). With "keep the word mark" on, the App gives this to the click-through
// layer (read_along_layer.h) if one of the Readings is still being read.
struct OverlayResult {
    ReadAlongTrack track;
};

// Shows the Overlay and keeps it up until the user leaves (F8 again, Esc, right-click or the
// Control bar's Help). The Analysis and every Drag run as jobs on `jobs` while the UI thread keeps
// handling messages; a Click that comes before the Analysis is done is held until it finishes.
// Returns when the Overlay and the Control bar are gone.
OverlayResult RunOverlay(const Shot& shot, const Recognizer& recognize, JobQueue& jobs, const OverlayStyle& style,
                const OverlayCallbacks& cb);

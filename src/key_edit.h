#pragma once
// Changing one of Lexia's keys from the Settings window (D6): the check that decides whether the
// key the page heard may be kept. Pure: it looks at the Settings in force and at the text, nothing
// else. The host (App) saves, applies and answers with a new `state`.
#include <string>

#include "settings.h"

enum class KeyEditOutcome {
    Accepted,
    UnknownAction,  // not read | pause | speech
    Empty,          // "": the page never sends it, and Lexia has no "unbind" in 0.5
    NotAKey,        // ParseHotkey does not know the text
    NeedsModifier,  // a letter, digit or Space alone would break typing
    UsedByOther,    // another action already has this key
};

struct KeyEditResult {
    KeyEditOutcome outcome = KeyEditOutcome::NotAKey;
    Hotkey key;          // Accepted: the parsed key
    std::string usedBy;  // UsedByOther: the other action ("read" | "pause" | "speech" | "switchLanguage")
};

// Is `text` ("F8", "Ctrl+Alt+R") a key the action ("read" | "pause" | "speech") may have now?
// The action's own current key is allowed (setting it again changes nothing).
KeyEditResult CheckKeyEdit(const Settings& current, const std::string& action, const std::string& text);

// The settings.json key of an action's key: "hotkeys.read". "" for an unknown action.
std::string HotkeySettingName(const std::string& action);

// A short English reason for the log ("a letter or digit needs Ctrl, Alt, Shift or Windows").
std::string KeyEditOutcomeText(KeyEditOutcome outcome);

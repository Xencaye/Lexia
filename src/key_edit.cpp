#include "key_edit.h"

#include "common.h"

// The Keys editor of the Settings page (D6): checks a key that the user typed for an action before it
// is saved. Pure (no Win32), so the rules are tested.
namespace {

// An action that the editor can change, and the Settings field that holds its key (a pointer to
// member: current.*key is that action's Hotkey).
struct Slot {
    const char* action;
    const Hotkey Settings::*key;
};

const Slot kSlots[] = {
    {"read", &Settings::read},
    {"pause", &Settings::pause},
    {"speech", &Settings::speech},
};

}  // namespace

// The settings.json key of an action ("read" -> "hotkeys.read"); "" for an action without a key slot.
std::string HotkeySettingName(const std::string& action) {
    for (const Slot& s : kSlots) {
        if (action == s.action) return std::string("hotkeys.") + s.action;
    }
    return std::string();
}

// The checks, in order: a known action, some text, a key Lexia can register, and no other action
// (the Language key included, though 0.5 does not show it) holding the same key.
KeyEditResult CheckKeyEdit(const Settings& current, const std::string& action, const std::string& text) {
    KeyEditResult r;
    if (HotkeySettingName(action).empty()) {
        r.outcome = KeyEditOutcome::UnknownAction;
        return r;
    }
    const std::string t = Trim(text);
    if (t.empty()) {
        r.outcome = KeyEditOutcome::Empty;
        return r;
    }
    Hotkey k;
    if (!ParseHotkey(t, k)) {
        // A lone letter, digit or Space is a key, only not a safe one: say so, it is the common slip.
        Hotkey withCtrl;
        const bool lone = t.find('+') == std::string::npos;
        r.outcome = lone && ParseHotkey("Ctrl+" + t, withCtrl) ? KeyEditOutcome::NeedsModifier
                                                                 : KeyEditOutcome::NotAKey;
        return r;
    }
    for (const Slot& s : kSlots) {
        if (action == s.action) continue;
        const Hotkey& other = current.*(s.key);
        if (other.IsBound() && other.mods == k.mods && other.vk == k.vk) {
            r.outcome = KeyEditOutcome::UsedByOther;
            r.usedBy = s.action;
            return r;
        }
    }
    const Hotkey& lang = current.switchLanguage;
    if (lang.IsBound() && lang.mods == k.mods && lang.vk == k.vk) {
        r.outcome = KeyEditOutcome::UsedByOther;
        r.usedBy = "switchLanguage";
        return r;
    }
    r.outcome = KeyEditOutcome::Accepted;
    r.key = k;
    return r;
}

// Developer text for the log (the page shows its own words from strings/).
std::string KeyEditOutcomeText(KeyEditOutcome outcome) {
    switch (outcome) {
    case KeyEditOutcome::Accepted: return "accepted";
    case KeyEditOutcome::UnknownAction: return "not an action with a key";
    case KeyEditOutcome::Empty: return "no key was given";
    case KeyEditOutcome::NotAKey: return "not a key Lexia knows";
    case KeyEditOutcome::NeedsModifier: return "a letter, digit or Space needs Ctrl, Alt, Shift or Windows";
    case KeyEditOutcome::UsedByOther: return "another action already has that key";
    }
    return "refused";
}

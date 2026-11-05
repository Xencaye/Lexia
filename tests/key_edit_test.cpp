// Unit tests for key_edit.h: which key the Settings window may give an action.
#include <doctest/doctest.h>

#include "key_edit.h"

TEST_CASE("Key edit: a function key is accepted") {
    const Settings s;
    const KeyEditResult r = CheckKeyEdit(s, "read", "F9");
    CHECK(r.outcome == KeyEditOutcome::Accepted);
    CHECK(r.key.vk == static_cast<UINT>(VK_F9));
    CHECK(r.key.mods == 0u);
    CHECK(r.key.text == "F9");
}

TEST_CASE("Key edit: a letter with a modifier is accepted, with the modifiers kept") {
    const Settings s;
    const KeyEditResult r = CheckKeyEdit(s, "pause", "Ctrl+Alt+R");
    CHECK(r.outcome == KeyEditOutcome::Accepted);
    CHECK(r.key.mods == static_cast<UINT>(MOD_CONTROL | MOD_ALT));
    CHECK(r.key.vk == static_cast<UINT>('R'));
}

TEST_CASE("Key edit: the Windows key the page names is understood") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "speech", "Windows+Shift+S").outcome == KeyEditOutcome::Accepted);
}

TEST_CASE("Key edit: a letter, a digit or Space alone needs a modifier") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "read", "R").outcome == KeyEditOutcome::NeedsModifier);
    CHECK(CheckKeyEdit(s, "read", "5").outcome == KeyEditOutcome::NeedsModifier);
    CHECK(CheckKeyEdit(s, "read", "Space").outcome == KeyEditOutcome::NeedsModifier);
    CHECK(CheckKeyEdit(s, "read", "Shift+R").outcome == KeyEditOutcome::Accepted);
}

TEST_CASE("Key edit: nonsense is not a key") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "read", "Ctrl+").outcome == KeyEditOutcome::NotAKey);
    CHECK(CheckKeyEdit(s, "read", "Ctrl+Banana").outcome == KeyEditOutcome::NotAKey);
    CHECK(CheckKeyEdit(s, "read", "F8+F9").outcome == KeyEditOutcome::NotAKey);
}

TEST_CASE("Key edit: an empty text is refused, not an unbind") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "read", "").outcome == KeyEditOutcome::Empty);
    CHECK(CheckKeyEdit(s, "read", "   ").outcome == KeyEditOutcome::Empty);
}

TEST_CASE("Key edit: a key another action has is refused, and the other action is named") {
    const Settings s;  // read F8, pause F7, speech F6
    const KeyEditResult r = CheckKeyEdit(s, "read", "F7");
    CHECK(r.outcome == KeyEditOutcome::UsedByOther);
    CHECK(r.usedBy == "pause");
}

TEST_CASE("Key edit: the same key in other spelling is still the same key") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "read", "f6").outcome == KeyEditOutcome::UsedByOther);
}

TEST_CASE("Key edit: the Language key counts when it is bound, not when it is not") {
    Settings s;
    CHECK(CheckKeyEdit(s, "read", "Ctrl+Alt+L").outcome == KeyEditOutcome::Accepted);
    REQUIRE(ParseHotkey("Ctrl+Alt+L", s.switchLanguage));
    const KeyEditResult r = CheckKeyEdit(s, "read", "Ctrl+Alt+L");
    CHECK(r.outcome == KeyEditOutcome::UsedByOther);
    CHECK(r.usedBy == "switchLanguage");
}

TEST_CASE("Key edit: an action may keep its own key") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "read", "F8").outcome == KeyEditOutcome::Accepted);
}

TEST_CASE("Key edit: an action without a key does not block anything") {
    Settings s;
    s.pause = Hotkey{};  // unbound
    CHECK(CheckKeyEdit(s, "read", "F7").outcome == KeyEditOutcome::Accepted);
}

TEST_CASE("Key edit: an unknown action is refused") {
    const Settings s;
    CHECK(CheckKeyEdit(s, "quit", "F9").outcome == KeyEditOutcome::UnknownAction);
    CHECK(CheckKeyEdit(s, "switchLanguage", "F9").outcome == KeyEditOutcome::UnknownAction);
}

TEST_CASE("Key edit: the settings.json name of an action's key") {
    CHECK(HotkeySettingName("read") == "hotkeys.read");
    CHECK(HotkeySettingName("pause") == "hotkeys.pause");
    CHECK(HotkeySettingName("speech") == "hotkeys.speech");
    CHECK(HotkeySettingName("nope").empty());
}

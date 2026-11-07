// Unit tests for tray_menu.h: the rows of the tray icon's menu (spec: old ui/dev/tray.html).
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "strings/string_ids.h"
#include "strings/strings.h"
#include "tray_menu.h"

namespace {
std::string Label(const TrayMenuItem& item) { return strings::Text("en", item.textId); }
}  // namespace

TEST_CASE("Tray menu: On, Off, a separator, Open Lexia, Quit, in that order") {
    const std::vector<TrayMenuItem> items = TrayMenuItems(true);
    REQUIRE(items.size() == 5);
    CHECK(items[0].command == TrayCommand::On);
    CHECK(items[1].command == TrayCommand::Off);
    CHECK(items[2].kind == TrayMenuItem::Kind::Separator);
    CHECK(items[3].command == TrayCommand::Open);
    CHECK(items[4].command == TrayCommand::Quit);
    CHECK(Label(items[0]) == "On");
    CHECK(Label(items[1]) == "Off");
    CHECK(Label(items[3]) == "Open Lexia");
    CHECK(Label(items[4]) == "Quit");
}

TEST_CASE("Tray menu: On and Off are a radio pair and the current one is checked") {
    const std::vector<TrayMenuItem> on = TrayMenuItems(true);
    CHECK(on[0].kind == TrayMenuItem::Kind::Radio);
    CHECK(on[1].kind == TrayMenuItem::Kind::Radio);
    CHECK(on[0].checked);
    CHECK_FALSE(on[1].checked);

    const std::vector<TrayMenuItem> off = TrayMenuItems(false);
    CHECK_FALSE(off[0].checked);
    CHECK(off[1].checked);
}

TEST_CASE("Tray menu: only Open Lexia is the default item, and nothing else is checked") {
    for (bool on : {true, false}) {
        int defaults = 0;
        int checked = 0;
        for (const TrayMenuItem& item : TrayMenuItems(on)) {
            if (item.isDefault) {
                ++defaults;
                CHECK(item.command == TrayCommand::Open);
            }
            if (item.checked) ++checked;
        }
        CHECK(defaults == 1);
        CHECK(checked == 1);
    }
}

TEST_CASE("Tray menu: every command row has a distinct non-zero command id") {
    std::vector<unsigned> ids;
    for (const TrayMenuItem& item : TrayMenuItems(true)) {
        if (item.kind == TrayMenuItem::Kind::Separator) continue;
        const unsigned id = static_cast<unsigned>(item.command);
        CHECK(id != 0);
        for (unsigned seen : ids) CHECK(seen != id);
        ids.push_back(id);
    }
}

TEST_CASE("Tray tooltip: says whether Lexia is On or Off") {
    CHECK(strings::Text("en", TrayTooltipId(true)) == "Lexia is On");
    CHECK(strings::Text("en", TrayTooltipId(false)) == "Lexia is Off");
}

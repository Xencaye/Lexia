// Unit tests for command_line.h: what lexia.exe's arguments mean.
#include <doctest/doctest.h>

#include "command_line.h"

TEST_CASE("Command line: no arguments means a normal start") {
    const CommandLine c = ParseCommandLine({});
    CHECK_FALSE(c.console);
    CHECK_FALSE(c.minimised);
    CHECK(c.unknown.empty());
}

TEST_CASE("Command line: --console asks for a console window") {
    const CommandLine c = ParseCommandLine({"--console"});
    CHECK(c.console);
    CHECK_FALSE(c.minimised);
}

TEST_CASE("Command line: --minimised starts without the Settings window") {
    CHECK(ParseCommandLine({"--minimised"}).minimised);
}

TEST_CASE("Command line: switches may come in any order, in any case") {
    const CommandLine c = ParseCommandLine({"--MINIMISED", "--Console"});
    CHECK(c.console);
    CHECK(c.minimised);
}

TEST_CASE("Command line: an argument that is not known is listed, not fatal") {
    const CommandLine c = ParseCommandLine({"--console", "--bogus", "file.png"});
    CHECK(c.console);
    REQUIRE(c.unknown.size() == 2);
    CHECK(c.unknown[0] == "--bogus");
    CHECK(c.unknown[1] == "file.png");
}

TEST_CASE("Command line: an argument is a switch only as a whole, not as a part") {
    CHECK_FALSE(ParseCommandLine({"--minimised-not"}).minimised);
    CHECK_FALSE(ParseCommandLine({"--minimised x"}).minimised);
    CHECK_FALSE(ParseCommandLine({}).minimised);
}

TEST_CASE("Command line: --elevated-relaunch marks the instance that an elevated relaunch started") {
    CHECK_FALSE(ParseCommandLine({}).elevatedRelaunch);
    CHECK(ParseCommandLine({"--elevated-relaunch"}).elevatedRelaunch);
    CHECK(ParseCommandLine({"--ELEVATED-Relaunch", "--minimised"}).elevatedRelaunch);
    CHECK(ParseCommandLine({"--elevated-relaunch"}).unknown.empty());
    CHECK_FALSE(ParseCommandLine({"--elevated-relaunch-not"}).elevatedRelaunch);
    CHECK(std::string(kElevatedRelaunchSwitch) == "--elevated-relaunch");
}

TEST_CASE("Command line: the relaunch arguments carry --minimised only when asked") {
    CHECK(RelaunchArgs(false) == "--elevated-relaunch");
    CHECK(RelaunchArgs(true) == "--elevated-relaunch --minimised");
}

TEST_CASE("Command line: the relaunch arguments read back as the same switches") {
    const CommandLine plain = ParseCommandLine({"--elevated-relaunch"});
    CHECK(plain.elevatedRelaunch);
    CHECK_FALSE(plain.minimised);
    const CommandLine min = ParseCommandLine({"--elevated-relaunch", "--minimised"});
    CHECK(min.elevatedRelaunch);
    CHECK(min.minimised);
}

TEST_CASE("Command line: --set-run-as-admin takes on or off as the next argument") {
    CHECK(std::string(kSetRunAsAdminSwitch) == "--set-run-as-admin");
    const CommandLine on = ParseCommandLine({"--set-run-as-admin", "on"});
    REQUIRE(on.setRunAsAdmin.has_value());
    CHECK(*on.setRunAsAdmin);
    CHECK_FALSE(on.setRunAsAdminInvalid);
    CHECK(on.unknown.empty());
    const CommandLine off = ParseCommandLine({"--Set-Run-As-Admin", "OFF", "--minimised"});
    REQUIRE(off.setRunAsAdmin.has_value());
    CHECK_FALSE(*off.setRunAsAdmin);
    CHECK_FALSE(off.setRunAsAdminInvalid);
    CHECK(off.minimised);
}

TEST_CASE("Command line: without the switch there is no run-as-admin change") {
    const CommandLine c = ParseCommandLine({"--minimised"});
    CHECK_FALSE(c.setRunAsAdmin.has_value());
    CHECK_FALSE(c.setRunAsAdminInvalid);
}

TEST_CASE("Command line: --set-run-as-admin with a missing or bad value is invalid, never a guess") {
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{
             {"--set-run-as-admin"}, {"--set-run-as-admin", "yes"}, {"--set-run-as-admin", "true"},
             {"--set-run-as-admin", ""}, {"--set-run-as-admin", "--minimised"}}) {
        const CommandLine c = ParseCommandLine(args);
        CHECK_FALSE(c.setRunAsAdmin.has_value());
        CHECK(c.setRunAsAdminInvalid);
    }
    // A switch after the bare one is not swallowed as its value.
    CHECK(ParseCommandLine({"--set-run-as-admin", "--minimised"}).minimised);
}
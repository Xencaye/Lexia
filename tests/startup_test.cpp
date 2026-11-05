// Unit tests for startup.h: the Run value.
#include <doctest/doctest.h>

#include "command_line.h"
#include "startup.h"

TEST_CASE("Startup: the Run value is the quoted exe, and --minimised with Start minimised") {
    CHECK(RunValueText("C:/Program Files/Lexia/lexia.exe", true) == "\"C:/Program Files/Lexia/lexia.exe\" --minimised");
    CHECK(RunValueText("C:/Program Files/Lexia/lexia.exe", false) == "\"C:/Program Files/Lexia/lexia.exe\"");
}

TEST_CASE("Startup: the Run value's switch is one that ParseCommandLine reads as minimised") {
    const std::string v = RunValueText("C:/x y/lexia.exe", true);
    CHECK(v.find(kMinimisedSwitch) != std::string::npos);
    CHECK(ParseCommandLine({kMinimisedSwitch}).minimised);
}

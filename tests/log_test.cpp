// Unit tests for log.h: what reaches the log file. The file is a temporary one (SetLogFile).
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "log.h"

TEST_CASE("Log file: a message cannot forge a line: later lines are indented, other controls escaped") {
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "lexia_log_test.log";
    std::error_code ec;
    std::filesystem::remove(file, ec);
    SetLogFile(file);
    LogError("\nAdded the Voice a\n2026-01-01 00:00:00.000 [ERROR] forged\r\tb\x01" "c");
    // Point the logger elsewhere: that closes this file so it can be read and removed.
    SetLogFile(std::filesystem::temp_directory_path() / "lexia_tests.log");

    std::ifstream in(file, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove(file, ec);

    // The leading newline is dropped; the forged entry is indented, so it can't pass for a real one.
    REQUIRE(!text.empty());
    CHECK(text.find('\r') == std::string::npos);
    CHECK(text.find('\t') == std::string::npos);
    CHECK(text.find("\n2026") == std::string::npos);
    CHECK(text.find("] Added the Voice a\n    2026-01-01 00:00:00.000 [ERROR] forged\\r\\tb\\x01c\n") != std::string::npos);
}

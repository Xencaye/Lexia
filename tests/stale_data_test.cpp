// The stale WebView2 profile is deleted once at start (src/stale_data.h). The safety rule is what matters:
// a junction or symbolic link inside it must not be followed, so nothing outside the folder is deleted.
#include <doctest/doctest.h>

#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "stale_data.h"

namespace fs = std::filesystem;

namespace {

// A fresh folder under the system temp folder, deleted at the end of the test.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() / ("lexia_stale_test_" + std::to_string(GetCurrentProcessId()) + "_" +
                                            std::to_string(++counter));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void Touch(const fs::path& file, const std::string& text = "x") {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

// A directory junction (mklink /J needs no admin right). false when it cannot be made.
bool MakeJunction(const fs::path& link, const fs::path& target) {
    const std::string cmd = "mklink /J \"" + link.string() + "\" \"" + target.string() + "\" >nul 2>nul";
    return std::system(cmd.c_str()) == 0 && fs::exists(link);
}

}  // namespace

TEST_CASE("RemoveStaleWebView2Data: nothing there is not an error") {
    TempDir data;
    CHECK(RemoveStaleWebView2Data(data.path) == StaleRemoval::Absent);
    CHECK(RemoveStaleWebView2Data(fs::path()) == StaleRemoval::Absent);
}

TEST_CASE("RemoveStaleWebView2Data: a normal tree is removed, a read-only file too, and nothing else") {
    TempDir data;
    Touch(data.path / "WebView2" / "EBWebView" / "Default" / "Cache" / "a.bin");
    Touch(data.path / "WebView2" / "top.txt");
    Touch(data.path / "WebView2" / "locked.txt");
    SetFileAttributesW((data.path / "WebView2" / "locked.txt").c_str(), FILE_ATTRIBUTE_READONLY);
    Touch(data.path / "settings.json");
    Touch(data.path / "voices" / "v.onnx");

    CHECK(RemoveStaleWebView2Data(data.path) == StaleRemoval::Removed);
    CHECK_FALSE(fs::exists(data.path / "WebView2"));
    CHECK(fs::exists(data.path / "settings.json"));  // the user's own data stays
    CHECK(fs::exists(data.path / "voices" / "v.onnx"));
    CHECK(RemoveStaleWebView2Data(data.path) == StaleRemoval::Absent);  // once
}

TEST_CASE("RemoveStaleWebView2Data: a junction inside is not followed") {
    TempDir data;
    TempDir outside;
    Touch(outside.path / "precious" / "keep.txt");
    Touch(data.path / "WebView2" / "a.txt");
    if (!MakeJunction(data.path / "WebView2" / "link", outside.path / "precious")) {
        MESSAGE("cannot make a junction here: the not-followed case is not tested");
        return;
    }
    REQUIRE(fs::exists(data.path / "WebView2" / "link" / "keep.txt"));

    CHECK(RemoveStaleWebView2Data(data.path) == StaleRemoval::Removed);
    CHECK_FALSE(fs::exists(data.path / "WebView2"));
    CHECK(fs::exists(outside.path / "precious" / "keep.txt"));  // the target was not touched
}

TEST_CASE("RemoveStaleWebView2Data: WebView2 itself being a junction removes the link, not its target") {
    TempDir data;
    TempDir outside;
    Touch(outside.path / "profile" / "keep.txt");
    if (!MakeJunction(data.path / "WebView2", outside.path / "profile")) {
        MESSAGE("cannot make a junction here: the not-followed case is not tested");
        return;
    }
    CHECK(RemoveStaleWebView2Data(data.path) == StaleRemoval::Removed);
    CHECK_FALSE(fs::exists(data.path / "WebView2"));
    CHECK(fs::exists(outside.path / "profile" / "keep.txt"));
}

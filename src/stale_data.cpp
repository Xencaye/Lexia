#include "stale_data.h"

#include <windows.h>

#include <string>

namespace fs = std::filesystem;

namespace {

bool IsReparsePoint(DWORD attributes) { return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }

// Removes one entry that is not a real directory: a file, a symbolic link to a file, or a directory
// junction / symbolic link. RemoveDirectoryW on a directory link deletes the link only, never its target.
bool RemoveEntry(const fs::path& path, DWORD attributes) {
    // A read-only attribute stops the delete; it is Lexia's own stale data, so clearing it is fine. Not on a
    // link: SetFileAttributesW may act on the link's target, which is outside the folder we may touch.
    if ((attributes & FILE_ATTRIBUTE_READONLY) && !IsReparsePoint(attributes)) {
        SetFileAttributesW(path.c_str(), attributes & ~DWORD{FILE_ATTRIBUTE_READONLY});
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) return RemoveDirectoryW(path.c_str()) != 0;
    return DeleteFileW(path.c_str()) != 0;
}

// Deletes the tree at `dir` (a real directory, not a link). Children are listed with FindFirstFileExW, which
// reports each one's attributes without opening it, so a link is recognised before anything follows it.
// Returns false when something stayed; the rest is still removed.
bool RemoveTree(const fs::path& dir) {
    bool all = true;
    WIN32_FIND_DATAW found;
    const std::wstring pattern = (dir / L"*").wstring();
    HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = found.cFileName;
            if (name == L"." || name == L"..") continue;
            const fs::path child = dir / name;
            const bool realDirectory =
                (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !IsReparsePoint(found.dwFileAttributes);
            if (realDirectory) {
                if (!RemoveTree(child)) all = false;
            } else if (!RemoveEntry(child, found.dwFileAttributes)) {
                all = false;
            }
        } while (FindNextFileW(h, &found));
        FindClose(h);
    }
    // The directory itself, now empty (or not, if a child stayed: then this fails and says so).
    const DWORD attributes = GetFileAttributesW(dir.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return all;
    return RemoveEntry(dir, attributes) && all;
}

}  // namespace

const wchar_t* StaleWebView2FolderName() { return L"WebView2"; }

StaleRemoval RemoveStaleWebView2Data(const fs::path& dataDir) {
    if (dataDir.empty()) return StaleRemoval::Absent;
    const fs::path target = dataDir / StaleWebView2FolderName();
    const DWORD attributes = GetFileAttributesW(target.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return StaleRemoval::Absent;
    // The top folder itself may be a link (someone moved the profile elsewhere): remove the link only.
    const bool realDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) && !IsReparsePoint(attributes);
    const bool ok = realDirectory ? RemoveTree(target) : RemoveEntry(target, attributes);
    return ok ? StaleRemoval::Removed : StaleRemoval::Failed;
}

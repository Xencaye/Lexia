#include "assets.h"

#include <windows.h>

#include <mutex>
#include <set>
#include <string>

#include "log.h"

namespace {

// The prefix of the resource name of each kind. It must be the same as in the generated assets.rc
// (CMakeLists.txt, "assets"): "<prefix>/<stem>".
const char* PrefixOf(AssetKind kind) {
    switch (kind) {
    case AssetKind::Icon: return "ICON";
    case AssetKind::Picture: return "PICTURE";
    case AssetKind::Font: return "FONT";
    }
    return "";
}

// A plain file stem: letters, digits, '-', '_' and '.'. Anything else (a path separator, "..", a
// colon) is not a name of ours, and is refused before it is used to look anything up, so a caller
// that passes text from outside cannot reach another resource by accident.
bool IsPlainName(std::string_view name) {
    if (name.empty() || name.size() > 100) return false;
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                        c == '_' || c == '.';
        if (!ok) return false;
    }
    return true;
}

// One log line per missing name, not one per draw (the bar asks every time it paints).
void LogMissingOnce(AssetKind kind, std::string_view name) {
    static std::mutex mutex;
    static std::set<std::string> logged;
    const std::string key = std::string(PrefixOf(kind)) + "/" + std::string(name);
    std::lock_guard<std::mutex> lock(mutex);
    if (logged.insert(key).second) LogWarn("[assets] No resource ", key, " in the exe.");
}

}  // namespace

std::string_view AssetBytes(AssetKind kind, std::string_view name) {
    if (!IsPlainName(name)) {
        LogMissingOnce(kind, "(bad name)");
        return {};
    }
    // The name is ASCII (IsPlainName), so a byte-wise widening is exact.
    const std::string full = std::string(PrefixOf(kind)) + "/" + std::string(name);
    const std::wstring wide(full.begin(), full.end());
    // nullptr = the module that made the process, the exe. Resources of another DLL are never looked at.
    const HMODULE exe = GetModuleHandleW(nullptr);
    const HRSRC res = FindResourceW(exe, wide.c_str(), RT_RCDATA);
    const HGLOBAL loaded = res ? LoadResource(exe, res) : nullptr;
    const void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = res ? SizeofResource(exe, res) : 0;
    if (!data || size == 0) {
        LogMissingOnce(kind, name);
        return {};
    }
    return std::string_view(static_cast<const char*>(data), size);
}

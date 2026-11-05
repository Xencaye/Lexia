// The Win32-backed helpers declared in common.h, and the std-only ReadFileToString.
#include "common.h"

#include <windows.h>
#include <shellapi.h>

#include <fstream>
#include <iterator>
#include <system_error>

// The whole file into *out, as bytes. With a maxBytes limit, a larger file is refused before it is
// read (for a file that is not Lexia's own and could be very large).
bool ReadFileToString(const std::filesystem::path& file, std::string* out, std::uintmax_t maxBytes) {
    if (maxBytes != std::numeric_limits<std::uintmax_t>::max()) {
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(file, ec);
        if (ec || size > maxBytes) return false;
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return !in.bad();
}

std::filesystem::path ExeDir() {
    // A return equal to the buffer size means the path was cut short: try again with a bigger
    // buffer, up to the longest path Windows has (32768 characters).
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD size = static_cast<DWORD>(buf.size());
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), size);
        if (n < size || buf.size() >= 32768) {
            buf.resize(n < size ? n : 0);
            return std::filesystem::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2 > 32768 ? 32768 : buf.size() * 2);
    }
}

// UTF-8 -> UTF-16 for Win32 calls. The -1 length makes Windows count the closing '\0' too, which is
// then taken off. A bad byte becomes U+FFFD.
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// The -1 length makes Windows count the closing '\0' too: n is the size with it, and the string gets
// n - 1 characters. n <= 1 means an empty string, or text that cannot be converted.
std::string WideToUtf8(const wchar_t* w) {
    if (!w || !*w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    out.resize(static_cast<size_t>(n) - 1);  // drop the '\0' that Windows wrote
    return out;
}

std::vector<std::string> ProgramArguments() {
    std::vector<std::string> args;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return args;
    for (int i = 1; i < argc; ++i) args.push_back(WideToUtf8(argv[i]));  // argv[0] is the program
    LocalFree(argv);
    return args;
}

// %LOCALAPPDATA%\Lexia: settings.json, lexia.log, the user's Voices and Presets. Not created here.
std::filesystem::path LexiaDataDir() {
    wchar_t buf[MAX_PATH * 2];
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, static_cast<DWORD>(std::size(buf)));
    if (n == 0 || n >= std::size(buf)) return {};
    return std::filesystem::path(std::wstring(buf, n)) / L"Lexia";
}

std::filesystem::path UserVoicesDir() {
    const std::filesystem::path dir = LexiaDataDir();
    return dir.empty() ? dir : dir / L"voices";
}

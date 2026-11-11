#include "net/winhttp_fetch.h"

#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <memory>
#include <vector>

#include "common.h"

// The real FetchFn: one HTTPS GET with WinHTTP, written to a file. The rules are strict on purpose,
// because a Voice model is code-like data that Lexia loads: HTTPS only (also after a redirect),
// TLS 1.2 or newer, a certificate revocation check, HTTP 200 only, and no more than maxBytes.
namespace {

// Closes a WinHTTP handle when the unique_ptr goes out of scope, on every return path.
struct HInternetDeleter {
    void operator()(void* h) const {
        if (h) WinHttpCloseHandle(h);
    }
};
using Handle = std::unique_ptr<void, HInternetDeleter>;

std::string Win32Error(const char* what) {
    const DWORD code = GetLastError();  // first: building the string may change it
    return std::string(what) + " (Windows error " + std::to_string(code) + ")";
}

// TLS 1.2 or newer only. An older Windows that does not know the TLS 1.3 flag refuses the whole
// value, so it is asked again for TLS 1.2 alone.
bool RequireModernTls(HINTERNET session) {
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    DWORD withTls13 = protocols | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &withTls13, sizeof(withTls13))) return true;
#endif
    return WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)) != FALSE;
}

}  // namespace

void FetchCancel::Cancel() {
    cancelled_ = true;
    std::lock_guard<std::mutex> lk(mutex_);
    if (request_) {
        // Aborts a call that is blocked on this handle in another thread; that call fails and the fetch
        // sees `cancelled_`. request_ is cleared so that nobody closes the handle a second time.
        WinHttpCloseHandle(request_);
        request_ = nullptr;
    }
}

// Ties the open request to a FetchCancel for the rest of the fetch. The destructor runs before the
// request's own Handle (it is declared after it) and decides who closes the handle: if Cancel() already
// did (request_ is null) the Handle is released so that it is not closed twice; else the Handle closes it.
struct FetchRequestRegistration {
    FetchCancel* cancel;
    Handle& request;
    FetchRequestRegistration(FetchCancel* c, Handle& r) : cancel(c), request(r) {
        if (!cancel) return;
        std::lock_guard<std::mutex> lk(cancel->mutex_);
        cancel->request_ = request.get();
    }
    ~FetchRequestRegistration() {
        if (!cancel) return;
        std::lock_guard<std::mutex> lk(cancel->mutex_);
        if (!cancel->request_) (void)request.release();
        cancel->request_ = nullptr;
    }
};

FetchResult WinHttpFetch(const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                         const std::function<bool(uint64_t)>& onBytes, std::string* error) {
    return WinHttpFetchCancellable(url, to, maxBytes, onBytes, error, nullptr);
}

FetchResult WinHttpFetchCancellable(const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                         const std::function<bool(uint64_t)>& onBytes, std::string* error, FetchCancel* cancel) {
    // A failure after a Cancel() is the cancel, not a network problem.
    const auto cancelled = [&] { return cancel && cancel->Cancelled(); };
    auto fail = [&](const std::string& why) {
        if (cancelled()) return FetchResult::Cancelled;
        if (error) *error = why;
        return FetchResult::Failed;
    };
    if (cancelled()) return FetchResult::Cancelled;
    if (url.rfind("https://", 0) != 0) return fail("only https:// addresses are fetched");

    // Split the address into host and path. A length of -1 asks WinHttpCrackUrl to find each part.
    const std::wstring wurl = Utf8ToWide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        return fail("not a valid https:// address");
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring path = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
                              std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    Handle session(WinHttpOpen(L"Lexia", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return fail(Win32Error("WinHttpOpen failed"));
    if (!RequireModernTls(session.get())) return fail(Win32Error("could not require TLS 1.2 or newer"));
    // ms: name lookup, connect, send, and each receive. A stalled download fails after 30 s.
    WinHttpSetTimeouts(session.get(), 15000, 15000, 30000, 30000);
    Handle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection) return fail(Win32Error("could not connect"));
    Handle request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) return fail(Win32Error("could not open the request"));
    // From here Cancel() may close the request from another thread (it is the handle every blocking call
    // below waits on: the connect and the name lookup happen inside SendRequest / ReceiveResponse).
    FetchRequestRegistration registration(cancel, request);
    if (cancelled()) return FetchResult::Cancelled;  // a Cancel() before the registration found nothing to close
    // Follow redirects (the files are served from a CDN) but never from HTTPS down to HTTP.
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    // A revoked certificate fails the request (WinHTTP does not check by default).
    DWORD features = WINHTTP_ENABLE_SSL_REVOCATION;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_ENABLE_FEATURE, &features, sizeof(features))) {
        return fail(Win32Error("could not turn on the certificate revocation check"));
    }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        return fail(Win32Error("the request failed"));
    }
    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        return fail(Win32Error("no status in the response"));
    }
    if (status != 200) return fail("the server answered HTTP " + std::to_string(status));

    // Read the body 64 KB at a time. onBytes reports progress and returns false to cancel. The caller
    // deletes a half-written file (its .part files are TempFiles).
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) return fail("cannot write " + to.u8string());
    std::vector<char> buffer(1 << 16);
    uint64_t received = 0;
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &got)) {
            return fail(Win32Error("the connection broke"));
        }
        if (got == 0) break;  // the end of the body
        received += got;
        if (received > maxBytes) return fail("the server sent more data than expected");
        out.write(buffer.data(), static_cast<std::streamsize>(got));
        if (!out) return fail("cannot write " + to.u8string() + " (disk full?)");
        if (onBytes && !onBytes(received)) return FetchResult::Cancelled;
    }
    out.close();
    if (!out) return fail("cannot write " + to.u8string());
    return FetchResult::Done;
}

#pragma once
// The real FetchFn (tts/voice_download.h): an HTTPS GET with WinHTTP, written to a file. HTTPS
// only: an http:// URL is refused, and so is a redirect from HTTPS to HTTP (WinHTTP's default).
// Sends only the request for the URL it is given and a "Lexia" User-Agent: no cookies, no
// identifiers, nothing about the screen (ADR-0005). Lives in lexia_net, not lexia_core, because it
// is Win32; blocks the calling thread, so call it off the UI thread.
#include <atomic>
#include <mutex>

#include "tts/voice_download.h"

// A way to stop a WinHttpFetch from another thread, also while it is blocked in a connect or a receive
// (which `onBytes` cannot reach: it runs only when data has come). Cancel() closes the request's WinHTTP
// handle, which is the documented way to abort a pending call; the blocked call then fails and the fetch
// returns FetchResult::Cancelled. One FetchCancel serves one fetch at a time; Reset() before the next.
// Thread-safe. The handle is closed exactly once, under `mutex_`, by whichever of Cancel() and the fetch's
// own clean-up gets there first; the fetch does not close it again.
class FetchCancel {
public:
    void Cancel();                                         // any thread, any time, any number of times
    bool Cancelled() const { return cancelled_.load(); }
    void Reset() { cancelled_ = false; }                   // only while no fetch uses this object

private:
    friend struct FetchRequestRegistration;
    std::atomic<bool> cancelled_{false};
    std::mutex mutex_;          // guards request_
    void* request_ = nullptr;   // the open request of the running fetch, nullptr when none (or already closed)
};

FetchResult WinHttpFetch(const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                         const std::function<bool(uint64_t)>& onBytes, std::string* error);

// The same, stoppable through `cancel` (may be nullptr). A fetch stopped this way returns Cancelled.
FetchResult WinHttpFetchCancellable(const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                         const std::function<bool(uint64_t)>& onBytes, std::string* error, FetchCancel* cancel);

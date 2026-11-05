#pragma once
// The real FetchFn (tts/voice_download.h): an HTTPS GET with WinHTTP, written to a file. HTTPS
// only: an http:// URL is refused, and so is a redirect from HTTPS to HTTP (WinHTTP's default).
// Sends only the request for the URL it is given and a "Lexia" User-Agent: no cookies, no
// identifiers, nothing about the screen (ADR-0005). Lives in lexia_net, not lexia_core, because it
// is Win32; blocks the calling thread, so call it off the UI thread.
#include "tts/voice_download.h"

FetchResult WinHttpFetch(const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                         const std::function<bool(uint64_t)>& onBytes, std::string* error);

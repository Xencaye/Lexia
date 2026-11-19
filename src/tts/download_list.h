#pragma once
// The Voices the Download sheet offers: a short list chosen by the owner and compiled into Lexia (owner,
// Nov 17), not the whole piper-voices catalogue. Opening the sheet makes no request (ADR-0005); the network
// is used only when the user downloads a Voice. Win32-free, in lexia_core, tested.
//
// To offer another Voice: add one row to the table at the top of download_list.cpp (name parts, size and MD5
// of the two files, SHA-256 in a comment) and follow docs/notes/voice-download-list.md ("Adding a Voice to the
// list"). Nothing else changes.
#include <vector>

#include "tts/voice_catalogue.h"

// The offered Voices, in the order the sheet shows them (the owner's order, not sorted). Built once; each entry
// has its `baseUrl` (https, pinned to a commit of rhasspy/piper-voices), both files with size and MD5, and passes
// CheckCatalogueVoice.
const std::vector<CatalogueVoice>& DownloadList();

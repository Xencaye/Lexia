#pragma once
// The UI font, Atkinson Hyperlegible Next (OFL), for every Win32 drawing path Lexia uses (K.1a, ADR-0009).
// The three woff2 files are resources of the exe (assets.h). They are unpacked once to OpenType with
// DirectWrite (IDWriteFactory5::UnpackFontFile) and the bytes are kept for the life of the process, so
//   (a) the Direct2D canvas gets a DirectWrite font collection holding the family (CreateAtkinsonCollection),
//   (b) GDI+ (Gdiplus::PrivateFontCollection::AddMemoryFont) and GDI (CreateGdiFont, which registers the
//       bytes with AddFontMemResourceEx) use the same font from the same bytes.
// Nothing is installed on the machine and no font file is read from a folder. If anything fails (an old
// Windows without woff2 unpacking, a missing resource) it is logged once and the callers fall back to
// Segoe UI, which every Windows has.
//
// lexia.exe only (Win32, DirectWrite). Any thread; the first call does the work.
#include <windows.h>

#include <dwrite_3.h>
#include <wrl/client.h>

#include <string_view>

namespace nui {

// The family name of the three files (name table ID 1 and the typographic family).
inline constexpr wchar_t kFontFamily[] = L"Atkinson Hyperlegible Next";
// What the callers use when the font is not available.
inline constexpr wchar_t kFallbackFontFamily[] = L"Segoe UI";

enum class FontWeight { Regular, SemiBold, Bold };

// The unpacked OpenType bytes of one weight, valid for the life of the process; empty if the font
// could not be loaded (logged once). GDI+ gets them with PrivateFontCollection::AddMemoryFont, which
// needs them to stay alive as long as the collection does.
std::string_view FontBytes(FontWeight weight);

// True if the three weights were unpacked.
bool FontsAvailable();

// A DirectWrite font collection with the family kFontFamily (all three weights), for the Direct2D
// canvas. `loader` must stay alive while `collection` is used (it serves the file bytes), so the two
// travel together. `collection` is null when the font is not available: the caller then uses the
// system font collection and kFallbackFontFamily. Each factory needs its own call (the loader is
// registered with the factory it was made for).
struct FontCollection {
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> loader;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection;
};
FontCollection CreateAtkinsonCollection(IDWriteFactory5* factory);

// A GDI font of the UI family: `heightPx` is the em height in pixels (it is passed to CreateFontW as
// a negative lfHeight, which means a character height, not a cell height), `gdiWeight` FW_NORMAL or
// FW_BOLD. Only Regular and Bold are registered with GDI (their family names are the plain one).
// Registers the bytes with GDI for this process on the first call. Falls back to Segoe UI. The caller
// owns the HFONT (DeleteObject).
HFONT CreateGdiFont(int heightPx, int gdiWeight);

}  // namespace nui

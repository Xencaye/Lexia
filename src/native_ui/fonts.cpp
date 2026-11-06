#include "native_ui/fonts.h"

#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "assets.h"
#include "log.h"

namespace nui {

namespace {

using Microsoft::WRL::ComPtr;

// The resource names (file stems in ui/fonts) in FontWeight order.
constexpr std::array<const char*, 3> kStems = {"AtkinsonHyperlegibleNext-Regular", "AtkinsonHyperlegibleNext-SemiBold",
                                               "AtkinsonHyperlegibleNext-Bold"};

// The unpacked fonts of the process. Filled once (std::call_once), then only read, so no lock is
// needed after that. The vectors are never resized again: DirectWrite and GDI+ keep pointers into them.
struct Loaded {
    std::array<std::vector<char>, 3> bytes;
    bool ok = false;
};

// Reads the whole unpacked stream into `out`.
bool ReadStream(IDWriteFontFileStream* stream, std::vector<char>& out) {
    UINT64 size = 0;
    if (FAILED(stream->GetFileSize(&size)) || size == 0 || size > (64u << 20)) return false;
    const void* start = nullptr;
    void* context = nullptr;
    if (FAILED(stream->ReadFileFragment(&start, 0, size, &context)) || !start) return false;
    out.assign(static_cast<const char*>(start), static_cast<const char*>(start) + size);
    stream->ReleaseFileFragment(context);
    return true;
}

Loaded Load() {
    Loaded l;
    // Any DirectWrite factory can unpack; this one is only used for that and then let go.
    ComPtr<IDWriteFactory5> factory;
    const HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory5),
                                           reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        LogWarn("[fonts] DirectWrite factory 5 is not there (0x", std::hex, static_cast<unsigned>(hr), std::dec,
                "); the UI uses ", "Segoe UI.");
        return l;
    }
    for (size_t i = 0; i < kStems.size(); ++i) {
        const std::string_view packed = AssetBytes(AssetKind::Font, kStems[i]);
        if (packed.empty() || packed.size() > (32u << 20)) {
            LogWarn("[fonts] The font resource ", kStems[i], " is missing; the UI uses Segoe UI.");
            return l;
        }
        const DWRITE_CONTAINER_TYPE type =
            factory->AnalyzeContainerType(packed.data(), static_cast<UINT32>(packed.size()));
        if (type != DWRITE_CONTAINER_TYPE_WOFF2 && type != DWRITE_CONTAINER_TYPE_WOFF) {
            LogWarn("[fonts] ", kStems[i], " is not a woff or woff2 file; the UI uses Segoe UI.");
            return l;
        }
        ComPtr<IDWriteFontFileStream> stream;
        const HRESULT uh = factory->UnpackFontFile(type, packed.data(), static_cast<UINT32>(packed.size()), &stream);
        if (FAILED(uh) || !ReadStream(stream.Get(), l.bytes[i])) {
            LogWarn("[fonts] Could not unpack ", kStems[i], " (0x", std::hex, static_cast<unsigned>(uh), std::dec,
                    "); the UI uses Segoe UI.");
            return l;
        }
    }
    l.ok = true;
    return l;
}

const Loaded& Fonts() {
    static Loaded loaded;
    static std::once_flag once;
    std::call_once(once, [] { loaded = Load(); });
    return loaded;
}

// Registers the Regular and Bold bytes with GDI for this process (private: other programs do not
// see them, and they go when the process ends). True if GDI took them.
bool RegisterWithGdi() {
    static const bool ok = [] {
        const Loaded& l = Fonts();
        if (!l.ok) return false;
        bool all = true;
        for (const size_t i : {size_t{0}, size_t{2}}) {
            DWORD count = 0;
            if (!AddFontMemResourceEx(const_cast<char*>(l.bytes[i].data()), static_cast<DWORD>(l.bytes[i].size()),
                                      nullptr, &count) ||
                count == 0) {
                LogWarn("[fonts] GDI would not take ", kStems[i], "; GDI text uses Segoe UI.");
                all = false;
            }
        }
        return all;
    }();
    return ok;
}

}  // namespace

std::string_view FontBytes(FontWeight weight) {
    const Loaded& l = Fonts();
    if (!l.ok) return {};
    const std::vector<char>& b = l.bytes[static_cast<size_t>(weight)];
    return std::string_view(b.data(), b.size());
}

bool FontsAvailable() { return Fonts().ok; }

FontCollection CreateAtkinsonCollection(IDWriteFactory5* factory) {
    FontCollection out;
    if (!factory || !Fonts().ok) return out;
    ComPtr<IDWriteInMemoryFontFileLoader> loader;
    ComPtr<IDWriteFontSetBuilder1> builder;
    HRESULT hr = factory->CreateInMemoryFontFileLoader(&loader);
    if (SUCCEEDED(hr)) hr = factory->RegisterFontFileLoader(loader.Get());
    if (SUCCEEDED(hr)) hr = factory->CreateFontSetBuilder(&builder);
    for (size_t i = 0; SUCCEEDED(hr) && i < kStems.size(); ++i) {
        const std::vector<char>& b = Fonts().bytes[i];
        ComPtr<IDWriteFontFile> file;
        // ownerObject null: DirectWrite does not copy the bytes, they stay in the process-wide `Loaded`.
        hr = loader->CreateInMemoryFontFileReference(factory, b.data(), static_cast<UINT32>(b.size()), nullptr, &file);
        if (SUCCEEDED(hr)) hr = builder->AddFontFile(file.Get());
    }
    ComPtr<IDWriteFontSet> set;
    if (SUCCEEDED(hr)) hr = builder->CreateFontSet(&set);
    ComPtr<IDWriteFontCollection1> collection;
    if (SUCCEEDED(hr)) hr = factory->CreateFontCollectionFromFontSet(set.Get(), &collection);
    if (FAILED(hr)) {
        LogWarn("[fonts] DirectWrite would not make the font collection (0x", std::hex, static_cast<unsigned>(hr),
                std::dec, "); the canvas uses Segoe UI.");
        if (loader) factory->UnregisterFontFileLoader(loader.Get());
        return out;
    }
    out.loader = std::move(loader);
    out.collection = std::move(collection);
    return out;
}

HFONT CreateGdiFont(int heightPx, int gdiWeight) {
    const bool own = RegisterWithGdi();
    HFONT font = CreateFontW(-heightPx, 0, 0, 0, gdiWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             own ? kFontFamily : kFallbackFontFamily);
    if (own) {
        // GDI picks a substitute silently when it does not know the name; ask which face it chose, so
        // a family-name mismatch is logged once instead of showing a wrong font.
        static std::once_flag checked;
        std::call_once(checked, [font] {
            HDC dc = GetDC(nullptr);
            const HGDIOBJ old = SelectObject(dc, font);
            wchar_t face[LF_FACESIZE] = {};
            GetTextFaceW(dc, LF_FACESIZE, face);
            SelectObject(dc, old);
            ReleaseDC(nullptr, dc);
            if (std::wstring(face) != kFontFamily) {
                LogWarn("[fonts] GDI chose another face than ", "Atkinson Hyperlegible Next for the UI font.");
            }
        });
    }
    return font;
}

}  // namespace nui

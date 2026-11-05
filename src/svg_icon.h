#pragma once
// SvgIconCache (lexia.exe): draws an SVG icon file into a premultiplied BGRA picture of a given pixel
// size and colour, for the Control bar. Direct2D draws it: ID2D1DeviceContext5::DrawSvgDocument
// (Windows 10 1703 and later) into a bitmap of a Direct3D 11 device, read back to memory. The colour
// of the file (#6E504A in ui/icons) is replaced before it is parsed (RecolourSvg), so one file serves
// every state. Pictures are kept per (file, size, colour) for the life of the process; the Direct2D
// objects are made on the first picture that is not kept yet and let go with the cache object, so a
// Session that needs no new picture makes none.
//
// UI thread only (the Control bar's). COM must be initialised on it.
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

struct IconPixels {
    int size = 0;              // width = height, in pixels
    std::vector<uint32_t> px;  // size * size premultiplied BGRA (0xAARRGGBB), top row first
};

class SvgIconCache {
public:
    SvgIconCache();
    ~SvgIconCache();
    SvgIconCache(const SvgIconCache&) = delete;
    SvgIconCache& operator=(const SvgIconCache&) = delete;

    // The icon in `file` as `size` x `size` pixels in colour `rgb` (0xRRGGBB), or null if the file
    // cannot be read or Direct2D has no SVG support here (logged once; the caller then shows words
    // only). The pointer stays valid for the life of the process.
    const IconPixels* Get(const std::filesystem::path& file, int size, uint32_t rgb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

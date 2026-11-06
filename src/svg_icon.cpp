#include "svg_icon.h"

#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <cstring>
#include <map>
#include <string>
#include <tuple>

#include "assets.h"
#include "log.h"
#include "svg_recolour.h"

// The Control bar's icons: an SVG resource of the exe (ui/icons, assets.h) drawn at one pixel size and
// colour with Direct2D, read back into memory as premultiplied BGRA, so the bar's GDI+ painting can blend it in. Each
// (name, size, colour) is drawn once and kept for the life of the process.
using Microsoft::WRL::ComPtr;

namespace {

// One map for the whole process: the pictures are plain memory, so they outlive a Session's objects.
// A std::map keeps its nodes where they are, which the returned pointers rely on.
using Key = std::tuple<std::string, int, uint32_t>;
std::map<Key, IconPixels>& Kept() {
    static std::map<Key, IconPixels> kept;
    return kept;
}

}  // namespace

struct SvgIconCache::Impl {
    bool tried = false;
    bool ok = false;
    ComPtr<ID3D11Device> d3d;
    ComPtr<ID2D1Factory1> factory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<ID2D1DeviceContext5> dc;

    // Makes the Direct2D objects; false (logged once) if this Windows cannot draw SVG. Direct2D draws
    // through a Direct3D 11 device: the graphics card, else WARP (Windows' software renderer).
    bool Init() {
        if (tried) return ok;
        tried = true;
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                       &d3d, nullptr, nullptr);
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &d3d,
                                   nullptr, nullptr);
        }
        ComPtr<IDXGIDevice> dxgi;
        if (SUCCEEDED(hr)) hr = d3d.As(&dxgi);
        if (SUCCEEDED(hr)) {
            hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr,
                                   reinterpret_cast<void**>(factory.GetAddressOf()));
        }
        if (SUCCEEDED(hr)) hr = factory->CreateDevice(dxgi.Get(), &d2dDevice);
        ComPtr<ID2D1DeviceContext> dc1;
        if (SUCCEEDED(hr)) hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc1);
        if (SUCCEEDED(hr)) hr = dc1.As(&dc);  // E_NOINTERFACE before Windows 10 1703: no SVG
        if (FAILED(hr)) {
            LogWarn("[controlbar] Direct2D cannot draw SVG here (0x", std::hex, static_cast<unsigned>(hr), std::dec,
                    "); the buttons show words only.");
            dc.Reset();
            d2dDevice.Reset();
            factory.Reset();
            d3d.Reset();
            return false;
        }
        ok = true;
        return true;
    }

    // Draws `svg` into a size x size picture. A bitmap that Direct2D draws into cannot be read by the
    // CPU, so the drawing is copied to a second, readable ("staging") bitmap and read row by row
    // (a row in the mapped bitmap may be longer than size * 4 bytes: mapped.pitch).
    bool Render(const std::string& svg, int size, std::vector<uint32_t>& out) {
        ComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), static_cast<UINT>(svg.size())));
        if (!stream) return false;
        const D2D1_SIZE_F viewport = D2D1::SizeF(static_cast<float>(size), static_cast<float>(size));
        ComPtr<ID2D1SvgDocument> doc;
        if (FAILED(dc->CreateSvgDocument(stream.Get(), viewport, &doc))) return false;
        // The files say width="24" height="24" viewBox="0 0 24 24": ask for the pixel size, and the
        // viewBox scales the drawing.
        ComPtr<ID2D1SvgElement> root;
        doc->GetRoot(&root);
        if (root) {
            const float px = static_cast<float>(size);
            root->SetAttributeValue(L"width", D2D1_SVG_ATTRIBUTE_POD_TYPE_FLOAT, &px, sizeof(px));
            root->SetAttributeValue(L"height", D2D1_SVG_ATTRIBUTE_POD_TYPE_FLOAT, &px, sizeof(px));
        }

        const D2D1_PIXEL_FORMAT format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        const D2D1_BITMAP_PROPERTIES1 target = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, format, 96.0f, 96.0f);
        const D2D1_BITMAP_PROPERTIES1 staging = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, format, 96.0f, 96.0f);
        const D2D1_SIZE_U pixels = D2D1::SizeU(static_cast<UINT32>(size), static_cast<UINT32>(size));
        ComPtr<ID2D1Bitmap1> drawn, readable;
        if (FAILED(dc->CreateBitmap(pixels, nullptr, 0, target, &drawn))) return false;
        if (FAILED(dc->CreateBitmap(pixels, nullptr, 0, staging, &readable))) return false;

        dc->SetTarget(drawn.Get());
        dc->BeginDraw();
        dc->Clear(D2D1::ColorF(0, 0.0f));
        dc->DrawSvgDocument(doc.Get());
        const HRESULT hr = dc->EndDraw();
        dc->SetTarget(nullptr);
        if (FAILED(hr)) return false;

        if (FAILED(readable->CopyFromBitmap(nullptr, drawn.Get(), nullptr))) return false;
        D2D1_MAPPED_RECT mapped = {};
        if (FAILED(readable->Map(D2D1_MAP_OPTIONS_READ, &mapped))) return false;
        out.assign(static_cast<size_t>(size) * static_cast<size_t>(size), 0u);
        for (int y = 0; y < size; ++y) {
            const BYTE* row = mapped.bits + static_cast<size_t>(y) * mapped.pitch;
            std::memcpy(&out[static_cast<size_t>(y) * static_cast<size_t>(size)], row, static_cast<size_t>(size) * 4);
        }
        readable->Unmap();
        return true;
    }
};

SvgIconCache::SvgIconCache() : impl_(std::make_unique<Impl>()) {}
SvgIconCache::~SvgIconCache() = default;

// The SVG text of an icon of the exe, in colour `rgb`. Icons are small (a few hundred bytes), so the
// copy is cheap. Empty if the exe has no such icon.
std::string RecolouredIconSvg(std::string_view name, uint32_t rgb) {
    const std::string_view bytes = AssetBytes(AssetKind::Icon, name);
    if (bytes.empty()) return {};
    return RecolourSvg(std::string(bytes), rgb);
}

// The picture of icon `name` at `size` px in colour rgb (0xRRGGBB), or nullptr if it cannot be drawn
// (the button then shows its word). The pointer stays valid for the life of the process.
const IconPixels* SvgIconCache::Get(std::string_view name, int size, uint32_t rgb) {
    if (size <= 0) return nullptr;
    const Key key{std::string(name), size, rgb & 0xFFFFFFu};
    auto& kept = Kept();
    const auto found = kept.find(key);
    if (found != kept.end()) return found->second.px.empty() ? nullptr : &found->second;

    // Not kept yet. An icon that is missing or cannot be drawn is remembered as empty, so it is tried
    // once (AssetBytes logs a missing one by itself, once per name).
    IconPixels icon;
    icon.size = size;
    const std::string svg = RecolouredIconSvg(name, rgb);
    if (!svg.empty() && impl_->Init()) {
        if (!impl_->Render(svg, size, icon.px)) {
            icon.px.clear();
            LogWarn("[controlbar] Could not draw the icon ", std::string(name), "; its button shows the word only.");
        }
    }
    const auto it = kept.emplace(key, std::move(icon)).first;
    return it->second.px.empty() ? nullptr : &it->second;
}

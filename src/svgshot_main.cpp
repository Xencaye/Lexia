// lexia_svgshot: a developer tool (not packaged; K12). Draws an SVG file at a square pixel size with
// Direct2D (the renderer the app uses for its icons: ID2D1DeviceContext5::DrawSvgDocument) on a
// transparent background and writes a PNG, so scripts\make_tray_icons.ps1 can make the .ico files from the
// Lexia logo SVGs (System.Drawing cannot read SVG).
//
//   lexia_svgshot --px 256 in.svg out.png
//
// The PNG is straight (not premultiplied) alpha, as .ico frames and Edge screenshots are. Self-contained
// on purpose: it reads the file named on the command line (a developer's own file), shares no code with
// lexia.exe and talks to no network.
#include <windows.h>

#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

// Reads a whole file; false if it cannot be opened.
bool ReadAll(const wchar_t* path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// Writes premultiplied BGRA pixels (top row first) as a PNG through WIC. WIC's PNG encoder wants
// straight alpha, so the pixels are un-premultiplied first.
bool WritePng(const wchar_t* path, int size, std::vector<uint32_t>& px) {
    for (uint32_t& p : px) {
        const uint32_t a = p >> 24;
        if (a == 0 || a == 255) continue;
        auto un = [a](uint32_t c) { return (c * 255u + a / 2) / a > 255u ? 255u : (c * 255u + a / 2) / a; };
        p = (a << 24) | (un((p >> 16) & 255u) << 16) | (un((p >> 8) & 255u) << 8) | un(p & 255u);
    }
    ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return false;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> enc;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> bag;
    if (FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE))) return false;
    if (FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc))) return false;
    if (FAILED(enc->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;
    if (FAILED(enc->CreateNewFrame(&frame, &bag)) || FAILED(frame->Initialize(bag.Get()))) return false;
    if (FAILED(frame->SetSize(static_cast<UINT>(size), static_cast<UINT>(size)))) return false;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA) return false;
    if (FAILED(frame->WritePixels(static_cast<UINT>(size), static_cast<UINT>(size) * 4,
                                  static_cast<UINT>(px.size() * 4), reinterpret_cast<BYTE*>(px.data())))) {
        return false;
    }
    return SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit());
}

// Draws `svg` into a size x size premultiplied BGRA picture (the same steps as SvgIconCache::Render).
bool Render(const std::string& svg, int size, std::vector<uint32_t>& out) {
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11Device> d3d;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                   &d3d, nullptr, nullptr);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &d3d,
                               nullptr, nullptr);
    }
    ComPtr<IDXGIDevice> dxgi;
    if (SUCCEEDED(hr)) hr = d3d.As(&dxgi);
    ComPtr<ID2D1Factory1> factory;
    if (SUCCEEDED(hr)) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr,
                               reinterpret_cast<void**>(factory.GetAddressOf()));
    }
    ComPtr<ID2D1Device> device;
    if (SUCCEEDED(hr)) hr = factory->CreateDevice(dxgi.Get(), &device);
    ComPtr<ID2D1DeviceContext> dc1;
    if (SUCCEEDED(hr)) hr = device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc1);
    ComPtr<ID2D1DeviceContext5> dc;
    if (SUCCEEDED(hr)) hr = dc1.As(&dc);
    if (FAILED(hr)) return false;

    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), static_cast<UINT>(svg.size())));
    if (!stream) return false;
    const float px = static_cast<float>(size);
    ComPtr<ID2D1SvgDocument> doc;
    if (FAILED(dc->CreateSvgDocument(stream.Get(), D2D1::SizeF(px, px), &doc))) return false;
    // The files say width="256" height="256" viewBox="0 0 256 256": ask for the pixel size and the
    // viewBox scales the drawing.
    ComPtr<ID2D1SvgElement> root;
    doc->GetRoot(&root);
    if (root) {
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
    hr = dc->EndDraw();
    dc->SetTarget(nullptr);
    if (FAILED(hr)) return false;

    if (FAILED(readable->CopyFromBitmap(nullptr, drawn.Get(), nullptr))) return false;
    D2D1_MAPPED_RECT mapped = {};
    if (FAILED(readable->Map(D2D1_MAP_OPTIONS_READ, &mapped))) return false;
    out.assign(static_cast<size_t>(size) * static_cast<size_t>(size), 0u);
    for (int y = 0; y < size; ++y) {
        std::memcpy(&out[static_cast<size_t>(y) * static_cast<size_t>(size)],
                    mapped.bits + static_cast<size_t>(y) * mapped.pitch, static_cast<size_t>(size) * 4);
    }
    readable->Unmap();
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    int px = 0;
    std::wstring in, out;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--px" && i + 1 < argc) {
            px = _wtoi(argv[++i]);
        } else if (in.empty()) {
            in = a;
        } else {
            out = a;
        }
    }
    if (px < 1 || px > 4096 || in.empty() || out.empty()) {
        std::fprintf(stderr, "usage: lexia_svgshot --px N in.svg out.png\n");
        return 2;
    }
    std::string svg;
    if (!ReadAll(in.c_str(), svg)) {
        std::fprintf(stderr, "Cannot read the SVG file.\n");
        return 1;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    int rc = 1;
    {
        std::vector<uint32_t> pixels;
        if (!Render(svg, px, pixels)) {
            std::fprintf(stderr, "Direct2D could not draw the SVG.\n");
        } else if (!WritePng(out.c_str(), px, pixels)) {
            std::fprintf(stderr, "The PNG could not be written.\n");
        } else {
            rc = 0;
        }
    }
    CoUninitialize();
    return rc;
}

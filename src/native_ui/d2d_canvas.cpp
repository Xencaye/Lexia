#include "native_ui/d2d_canvas.h"

#include <shlwapi.h>

#include <algorithm>
#include <cmath>

#include "assets.h"
#include "common.h"
#include "log.h"
#include "svg_icon.h"

namespace nui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr int kMaxPictureSide = 8192;     // a larger "engraving" is not one of ours: refuse it
constexpr size_t kMaxMeasureEntries = 4096;  // the measure cache is cleared when it grows past this
constexpr float kIconViewport = 24.0f;    // every file of ui/icons is drawn on a 24 x 24 grid

// A log-friendly form of an HRESULT.
std::string Hex(HRESULT hr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
    return buf;
}

// Device-loss errors: the device must be rebuilt, the frame is simply lost.
bool IsDeviceLost(HRESULT hr) {
    return hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
           hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

}  // namespace

D2dCanvas::D2dCanvas() = default;

D2dCanvas::~D2dCanvas() {
    // Release everything that belongs to the factories first, then take the in-memory font loader back
    // out of the DirectWrite factory it was registered with.
    inFrame_ = false;
    svgs_.clear();
    pictureBitmaps_.clear();
    brush_.Reset();
    target_.Reset();
    swap_.Reset();
    dc_.Reset();
    d2dDevice_.Reset();
    d3d_.Reset();
    formats_.clear();
    if (dwrite_ && fonts_.loader) dwrite_->UnregisterFontFileLoader(fonts_.loader.Get());
}

// ------------------------------------------------------------------------------ factories ---

bool D2dCanvas::EnsureFactories() {
    if (factoriesTried_) return factoriesOk_;
    factoriesTried_ = true;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr,
                                   reinterpret_cast<void**>(factory_.GetAddressOf()));
    if (SUCCEEDED(hr)) {
        // ISOLATED: the in-memory font loader is registered on this factory only.
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory5),
                                 reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()));
    }
    if (FAILED(hr)) {
        LogError("[d2d] Could not make the Direct2D or DirectWrite factory (", Hex(hr), "); the window cannot draw.");
        return false;
    }

    // Strokes: round caps and joins for glyph-like lines, and the dotted rule (1 on, 1 off in stroke widths).
    D2D1_STROKE_STYLE_PROPERTIES round = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
    factory_->CreateStrokeStyle(round, nullptr, 0, &roundStroke_);
    D2D1_STROKE_STYLE_PROPERTIES dotted = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f,
        D2D1_DASH_STYLE_CUSTOM, 0.0f);
    const float dashes[2] = {1.0f, 1.0f};
    factory_->CreateStrokeStyle(dotted, dashes, 2, &dottedStroke_);

    // Fonts: Atkinson from the exe's resources, else Segoe UI from the system (fonts.h logs why).
    fonts_ = CreateAtkinsonCollection(dwrite_.Get());
    if (fonts_.collection) {
        UINT32 index = 0;
        BOOL exists = FALSE;
        fonts_.collection->FindFamilyName(kFontFamily, &index, &exists);
        useAtkinson_ = exists != FALSE;
        if (!useAtkinson_) LogWarn("[d2d] The font collection has no family named Atkinson; the UI uses Segoe UI.");
    }

    // WIC decodes the engravings and writes the shots. A failure only costs those two.
    const HRESULT wicHr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&wic_));
    if (FAILED(wicHr)) LogWarn("[d2d] WIC is not available (", Hex(wicHr), "); pictures are not drawn.");

    factoriesOk_ = true;
    return true;
}

// -------------------------------------------------------------------------------- device ---

void D2dCanvas::DropDevice(const char* why, HRESULT hr) {
    if (!lostLogged_) {
        LogWarn("[d2d] Lost the graphics device (", why, ", ", Hex(hr), "); building it again at the next frame.");
        lostLogged_ = true;
    }
    inFrame_ = false;
    svgs_.clear();
    pictureBitmaps_.clear();
    brush_.Reset();
    if (dc_) dc_->SetTarget(nullptr);
    target_.Reset();
    swap_.Reset();
    dc_.Reset();
    d2dDevice_.Reset();
    d3d_.Reset();
}

bool D2dCanvas::EnsureDevice() {
    if (dc_) return true;
    if (!EnsureFactories()) return false;

    // Direct2D draws through a Direct3D 11 device: the graphics card, else WARP (Windows' software
    // renderer, which is also what a machine without a GPU or a CI runner uses).
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                   &d3d_, nullptr, nullptr);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &d3d_,
                               nullptr, nullptr);
    }
    ComPtr<IDXGIDevice> dxgi;
    if (SUCCEEDED(hr)) hr = d3d_.As(&dxgi);
    if (SUCCEEDED(hr)) hr = factory_->CreateDevice(dxgi.Get(), &d2dDevice_);
    ComPtr<ID2D1DeviceContext> dc1;
    if (SUCCEEDED(hr)) hr = d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc1);
    if (SUCCEEDED(hr)) hr = dc1.As(&dc_);  // E_NOINTERFACE before Windows 10 1703: no SVG
    if (SUCCEEDED(hr) && mode_ == Mode::Window) {
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> dxgiFactory;
        hr = dxgi->GetAdapter(&adapter);
        if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC1 desc = {};
            desc.Width = static_cast<UINT>(std::max(1, widthPx_));
            desc.Height = static_cast<UINT>(std::max(1, heightPx_));
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            // NONE: during a resize the old picture is not stretched, the new area is simply filled by
            // the next frame. Flip model: the current Windows presentation path.
            desc.Scaling = DXGI_SCALING_NONE;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            hr = dxgiFactory->CreateSwapChainForHwnd(d3d_.Get(), hwnd_, &desc, nullptr, nullptr, &swap_);
            // The window is not to become a full-screen exclusive window by Alt+Enter.
            if (SUCCEEDED(hr)) dxgiFactory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
        }
    }
    if (SUCCEEDED(hr)) hr = dc_->CreateSolidColorBrush(D2D1::ColorF(0, 0.0f), &brush_);
    if (SUCCEEDED(hr) && !CreateTarget()) hr = E_FAIL;
    if (FAILED(hr)) {
        if (!deviceFailedLogged_) {
            LogError("[d2d] Could not set up Direct2D drawing (", Hex(hr), ").");
            deviceFailedLogged_ = true;
        }
        svgs_.clear();
        pictureBitmaps_.clear();
        brush_.Reset();
        target_.Reset();
        swap_.Reset();
        dc_.Reset();
        d2dDevice_.Reset();
        d3d_.Reset();
        return false;
    }
    deviceFailedLogged_ = false;
    return true;
}

// The Direct2D target bitmap: the swap chain's back buffer, or a bitmap of the off-screen size.
bool D2dCanvas::CreateTarget() {
    if (!dc_) return false;
    HRESULT hr = E_FAIL;
    if (mode_ == Mode::Window && swap_) {
        ComPtr<IDXGISurface> surface;
        hr = swap_->GetBuffer(0, IID_PPV_ARGS(&surface));
        if (SUCCEEDED(hr)) {
            // The back buffer is opaque (alpha ignored). The context's DPI, not the bitmap's, scales the
            // DIPs, and BeginFrame sets it.
            const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi_, dpi_);
            hr = dc_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target_);
        }
    } else if (mode_ == Mode::Offscreen) {
        const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            dpi_, dpi_);
        hr = dc_->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(std::max(1, widthPx_)),
                                           static_cast<UINT32>(std::max(1, heightPx_))),
                               nullptr, 0, props, &target_);
    }
    if (FAILED(hr)) {
        LogError("[d2d] Could not make the drawing target (", Hex(hr), ").");
        target_.Reset();
        return false;
    }
    return true;
}

bool D2dCanvas::AttachWindow(HWND hwnd, int widthPx, int heightPx, float dpi) {
    if (mode_ != Mode::None || !hwnd) return false;
    mode_ = Mode::Window;
    hwnd_ = hwnd;
    widthPx_ = widthPx;
    heightPx_ = heightPx;
    dpi_ = dpi > 0 ? dpi : 96.0f;
    return EnsureDevice();
}

bool D2dCanvas::CreateOffscreen(int widthPx, int heightPx, float dpi) {
    if (mode_ != Mode::None || widthPx <= 0 || heightPx <= 0) return false;
    mode_ = Mode::Offscreen;
    widthPx_ = widthPx;
    heightPx_ = heightPx;
    dpi_ = dpi > 0 ? dpi : 96.0f;
    return EnsureDevice();
}

bool D2dCanvas::Resize(int widthPx, int heightPx) {
    if (inFrame_ || mode_ == Mode::None) return false;
    widthPx_ = std::max(1, widthPx);
    heightPx_ = std::max(1, heightPx);
    if (!dc_) return true;  // the next frame builds the device at this size
    dc_->SetTarget(nullptr);
    target_.Reset();
    if (mode_ == Mode::Window && swap_) {
        const HRESULT hr = swap_->ResizeBuffers(0, static_cast<UINT>(widthPx_), static_cast<UINT>(heightPx_),
                                                DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            if (IsDeviceLost(hr)) {
                DropDevice("resize", hr);
            } else {
                LogError("[d2d] Could not resize the swap chain (", Hex(hr), ").");
            }
            return false;
        }
    }
    return CreateTarget();
}

void D2dCanvas::SetDpi(float dpi) {
    if (dpi > 0) dpi_ = dpi;
}

// --------------------------------------------------------------------------------- frames ---

bool D2dCanvas::BeginFrame() {
    if (inFrame_) return true;
    if (mode_ == Mode::None) return false;
    if (!EnsureDevice()) return false;
    if (!target_ && !CreateTarget()) return false;
    lostLogged_ = false;
    dc_->SetTarget(target_.Get());
    dc_->SetDpi(dpi_, dpi_);
    // Text smoothing is left at the default: ClearType (the user's own setting) on this opaque target, as the
    // HTML reference it is compared with has it, and grey-scale inside a layer (an opacity group).
    dc_->BeginDraw();
    dc_->SetTransform(D2D1::Matrix3x2F::Identity());
    inFrame_ = true;
    return true;
}

bool D2dCanvas::EndFrame() {
    if (!inFrame_) return false;
    inFrame_ = false;
    HRESULT hr = dc_->EndDraw();
    if (FAILED(hr)) {
        if (IsDeviceLost(hr)) {
            DropDevice("EndDraw", hr);
        } else {
            LogWarn("[d2d] A frame failed (", Hex(hr), ").");
        }
        return false;
    }
    if (mode_ == Mode::Window && swap_) {
        hr = swap_->Present(1, 0);
        if (IsDeviceLost(hr)) {
            DropDevice("Present", hr);
            return false;
        }
        if (FAILED(hr)) {
            LogWarn("[d2d] Present failed (", Hex(hr), ").");
            return false;
        }
    }
    return true;
}

void D2dCanvas::Clear(Colour c) {
    if (Drawing()) dc_->Clear(ToColor(c));
}

// -------------------------------------------------------------------------- off-screen out ---

bool D2dCanvas::ReadPixels(std::vector<uint32_t>& out) {
    if (mode_ != Mode::Offscreen || inFrame_ || !dc_ || !target_) return false;
    // A bitmap that is drawn into cannot be read by the CPU: copy it to a "staging" bitmap and map that.
    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
    ComPtr<ID2D1Bitmap1> staging;
    HRESULT hr = dc_->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(widthPx_), static_cast<UINT32>(heightPx_)), nullptr,
                                   0, props, &staging);
    if (SUCCEEDED(hr)) hr = staging->CopyFromBitmap(nullptr, target_.Get(), nullptr);
    D2D1_MAPPED_RECT mapped = {};
    if (SUCCEEDED(hr)) hr = staging->Map(D2D1_MAP_OPTIONS_READ, &mapped);
    if (FAILED(hr)) {
        LogError("[d2d] Could not read the picture back (", Hex(hr), ").");
        return false;
    }
    const size_t w = static_cast<size_t>(widthPx_), h = static_cast<size_t>(heightPx_);
    out.assign(w * h, 0u);
    for (size_t y = 0; y < h; ++y) {
        // A mapped row may be longer than w * 4 bytes (pitch).
        std::memcpy(&out[y * w], mapped.bits + y * mapped.pitch, w * 4);
    }
    // The target ignores alpha (it is opaque, so ClearType can be used): what is in the fourth byte is
    // not defined, the picture is opaque.
    for (uint32_t& p : out) p |= 0xFF000000u;
    staging->Unmap();
    return true;
}

bool D2dCanvas::SavePng(const std::wstring& path) {
    std::vector<uint32_t> px;
    if (!ReadPixels(px) || !wic_) return false;
    // PNG holds straight alpha: undo the premultiplication (the shots are opaque, so this is exact there).
    for (uint32_t& p : px) {
        const uint32_t a = p >> 24;
        if (a == 0 || a == 255) continue;
        auto un = [a](uint32_t v) { return std::min<uint32_t>(255, (v * 255 + a / 2) / a); };
        p = (a << 24) | (un((p >> 16) & 0xFF) << 16) | (un((p >> 8) & 0xFF) << 8) | un(p & 0xFF);
    }
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> bag;
    HRESULT hr = wic_->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = wic_->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &bag);
    if (SUCCEEDED(hr)) hr = frame->Initialize(bag.Get());
    if (SUCCEEDED(hr)) hr = frame->SetSize(static_cast<UINT>(widthPx_), static_cast<UINT>(heightPx_));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    if (SUCCEEDED(hr) && format != GUID_WICPixelFormat32bppBGRA) hr = E_FAIL;  // the PNG encoder takes BGRA
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(static_cast<UINT>(heightPx_), static_cast<UINT>(widthPx_) * 4,
                                static_cast<UINT>(px.size() * 4), reinterpret_cast<BYTE*>(px.data()));
    }
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (FAILED(hr)) LogError("[d2d] Could not write the PNG (", Hex(hr), ").");
    return SUCCEEDED(hr);
}

// ------------------------------------------------------------------------------- helpers ---

float D2dCanvas::SnapDip(float v) const {
    const float s = Scale();
    return std::round(v * s) / s;
}

D2D1_COLOR_F D2dCanvas::ToColor(Colour c) const {
    return D2D1::ColorF(c.rgb & 0xFFFFFFu, c.alpha);
}

ID2D1SolidColorBrush* D2dCanvas::Brush(Colour c) {
    brush_->SetColor(ToColor(c));
    return brush_.Get();
}

// ---------------------------------------------------------------------------- shapes ---

void D2dCanvas::FillRect(const RectF& r, Colour c, float radius) {
    if (!Drawing()) return;
    // The edges sit on whole device pixels: a fill never has a blurred edge.
    const float l = SnapDip(r.x), t = SnapDip(r.y), rt = SnapDip(r.Right()), b = SnapDip(r.Bottom());
    if (rt <= l || b <= t) return;
    const D2D1_RECT_F rect = D2D1::RectF(l, t, rt, b);
    if (radius > 0) {
        const float rad = std::min({radius, (rt - l) / 2, (b - t) / 2});
        dc_->FillRoundedRectangle(D2D1::RoundedRect(rect, rad, rad), Brush(c));
    } else {
        dc_->FillRectangle(rect, Brush(c));
    }
}

void D2dCanvas::StrokeRect(const RectF& r, Colour c, float width, float radius) {
    if (!Drawing() || width <= 0) return;
    // Like a browser's border: the width is a whole number of device pixels (at least one), and the
    // outer edge of the stroke sits on the pixel grid. The stroke stays centred on the rectangle's edge.
    const float s = Scale();
    const float pw = std::max(1.0f, std::floor(width * s + 0.001f));
    const float outerL = std::round(r.x * s - pw / 2), outerT = std::round(r.y * s - pw / 2);
    const float outerR = std::round(r.Right() * s + pw / 2), outerB = std::round(r.Bottom() * s + pw / 2);
    const D2D1_RECT_F rect = D2D1::RectF((outerL + pw / 2) / s, (outerT + pw / 2) / s, (outerR - pw / 2) / s,
                                         (outerB - pw / 2) / s);
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    if (radius > 0) {
        const float rad = std::min({radius, (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2});
        dc_->DrawRoundedRectangle(D2D1::RoundedRect(rect, rad, rad), Brush(c), pw / s);
    } else {
        dc_->DrawRectangle(rect, Brush(c), pw / s);
    }
}

void D2dCanvas::FillEllipse(const RectF& bounds, Colour c) {
    if (!Drawing()) return;
    dc_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bounds.x + bounds.w / 2, bounds.y + bounds.h / 2), bounds.w / 2,
                                   bounds.h / 2),
                     Brush(c));
}

void D2dCanvas::StrokeEllipse(const RectF& bounds, Colour c, float width) {
    if (!Drawing()) return;
    dc_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(bounds.x + bounds.w / 2, bounds.y + bounds.h / 2), bounds.w / 2,
                                   bounds.h / 2),
                     Brush(c), width);
}

void D2dCanvas::Line(PointF a, PointF b, Colour c, float width) {
    if (!Drawing()) return;
    dc_->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(b.x, b.y), Brush(c), width, roundStroke_.Get());
}

void D2dCanvas::Polyline(const std::vector<PointF>& points, Colour c, float width) {
    if (!Drawing() || points.size() < 2) return;
    ComPtr<ID2D1PathGeometry> geometry;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&geometry)) || FAILED(geometry->Open(&sink))) return;
    sink->BeginFigure(D2D1::Point2F(points[0].x, points[0].y), D2D1_FIGURE_BEGIN_HOLLOW);
    for (size_t i = 1; i < points.size(); ++i) sink->AddLine(D2D1::Point2F(points[i].x, points[i].y));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    if (FAILED(sink->Close())) return;
    dc_->DrawGeometry(geometry.Get(), Brush(c), width, roundStroke_.Get());
}

void D2dCanvas::Arc(PointF centre, float radius, float startDeg, float sweepDeg, Colour c, float width) {
    if (!Drawing() || radius <= 0 || sweepDeg == 0) return;
    if (std::fabs(sweepDeg) >= 360.0f) {
        // An arc segment cannot close a full circle (start == end): draw the circle.
        StrokeEllipse({centre.x - radius, centre.y - radius, radius * 2, radius * 2}, c, width);
        return;
    }
    const float a0 = startDeg * kPi / 180.0f, a1 = (startDeg + sweepDeg) * kPi / 180.0f;
    ComPtr<ID2D1PathGeometry> geometry;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&geometry)) || FAILED(geometry->Open(&sink))) return;
    sink->BeginFigure(D2D1::Point2F(centre.x + radius * std::cos(a0), centre.y + radius * std::sin(a0)),
                      D2D1_FIGURE_BEGIN_HOLLOW);
    // y points down on the screen, so Direct2D's "clockwise" is the clockwise of the Canvas contract.
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(centre.x + radius * std::cos(a1), centre.y + radius * std::sin(a1)),
                                  D2D1::SizeF(radius, radius), 0.0f,
                                  sweepDeg > 0 ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                  std::fabs(sweepDeg) > 180.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    if (FAILED(sink->Close())) return;
    dc_->DrawGeometry(geometry.Get(), Brush(c), width, roundStroke_.Get());
}

void D2dCanvas::DottedLine(PointF a, PointF b, Colour c) {
    if (!Drawing()) return;
    // CSS `1px dotted`: dots one pixel square with one pixel between (a browser floors the border to
    // whole device pixels). The stroke covers whole pixels and the ends sit on the pixel grid; the dash
    // length is in stroke widths, so 1 on and 1 off is exactly one dot, one gap.
    const float s = Scale();
    const float pw = std::max(1.0f, std::floor(s + 0.001f));
    const bool horizontal = std::fabs(b.x - a.x) >= std::fabs(b.y - a.y);
    D2D1_POINT_2F p0, p1;
    if (horizontal) {
        const float y = (std::round(a.y * s) + pw / 2) / s;
        p0 = D2D1::Point2F(std::round(std::min(a.x, b.x) * s) / s, y);
        p1 = D2D1::Point2F(std::round(std::max(a.x, b.x) * s) / s, y);
    } else {
        const float x = (std::round(a.x * s) + pw / 2) / s;
        p0 = D2D1::Point2F(x, std::round(std::min(a.y, b.y) * s) / s);
        p1 = D2D1::Point2F(x, std::round(std::max(a.y, b.y) * s) / s);
    }
    dc_->DrawLine(p0, p1, Brush(c), pw / s, dottedStroke_.Get());
}

// ----------------------------------------------------------------------------------- text ---

IDWriteTextFormat* D2dCanvas::FormatFor(const TextStyle& style) {
    const std::pair<int, int> key{static_cast<int>(std::lround(style.size * 100)), static_cast<int>(style.weight)};
    const auto found = formats_.find(key);
    if (found != formats_.end()) return found->second.Get();
    if (!EnsureFactories()) return nullptr;
    ComPtr<IDWriteTextFormat> format;
    const HRESULT hr = dwrite_->CreateTextFormat(
        useAtkinson_ ? kFontFamily : kFallbackFontFamily, useAtkinson_ ? fonts_.collection.Get() : nullptr,
        static_cast<DWRITE_FONT_WEIGHT>(style.weight), DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        style.size, L"en-us", &format);
    if (FAILED(hr)) {
        LogError("[d2d] Could not make a text format (", Hex(hr), ").");
        return nullptr;
    }
    // One line per call: the widgets wrap text themselves.
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    return formats_.emplace(key, std::move(format)).first->second.Get();
}

ComPtr<IDWriteTextLayout> D2dCanvas::LayoutFor(std::string_view utf8, const TextStyle& style) {
    ComPtr<IDWriteTextLayout> layout;
    IDWriteTextFormat* format = FormatFor(style);
    if (!format) return layout;
    const std::wstring wide = Utf8ToWide(std::string(utf8));
    // Room enough that nothing wraps (the format says NO_WRAP anyway).
    if (FAILED(dwrite_->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), format, 100000.0f, 10000.0f,
                                         &layout))) {
        return nullptr;
    }
    if (style.underline && !wide.empty()) layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(wide.size())});
    if (style.letterSpacing != 0.0f && !wide.empty()) {
        // The space is added after each character (the trailing space of the last one too: harmless).
        ComPtr<IDWriteTextLayout1> layout1;
        if (SUCCEEDED(layout.As(&layout1))) {
            layout1->SetCharacterSpacing(0.0f, style.letterSpacing, 0.0f, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(wide.size())});
        }
    }
    return layout;
}

SizeF D2dCanvas::MeasureText(std::string_view utf8, const TextStyle& style) {
    // Wrapping asks for the same words over and over: keep the answers.
    const auto key = std::make_tuple(std::string(utf8), static_cast<int>(std::lround(style.size * 100)),
                                     static_cast<int>(style.weight), style.underline,
                                     static_cast<int>(std::lround(style.letterSpacing * 100)));
    const auto found = measured_.find(key);
    if (found != measured_.end()) return found->second;
    if (!EnsureFactories()) return {};
    const ComPtr<IDWriteTextLayout> layout = LayoutFor(utf8, style);
    if (!layout) return {};
    DWRITE_TEXT_METRICS m = {};
    if (FAILED(layout->GetMetrics(&m))) return {};
    const SizeF size{m.width, m.height};
    if (measured_.size() >= kMaxMeasureEntries) measured_.clear();
    measured_.emplace(key, size);
    return size;
}

void D2dCanvas::Text(const RectF& box, std::string_view utf8, const TextStyle& style, Colour c) {
    if (!Drawing() || utf8.empty()) return;
    const ComPtr<IDWriteTextLayout> layout = LayoutFor(utf8, style);
    if (!layout) return;
    DWRITE_TEXT_METRICS m = {};
    if (FAILED(layout->GetMetrics(&m))) return;
    float x = box.x;
    if (style.align == TextAlign::Centre) x = box.x + (box.w - m.width) / 2;
    else if (style.align == TextAlign::Right) x = box.Right() - m.width;
    // Vertically centred in the box; the baseline is put on a whole device pixel so the text is crisp.
    const float y = SnapDip(box.y + (box.h - m.height) / 2);
    dc_->DrawTextLayout(D2D1::Point2F(x, y), layout.Get(), Brush(c), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

// ----------------------------------------------------------------------------- icons ---

ID2D1SvgDocument* D2dCanvas::SvgFor(std::string_view name, uint32_t rgb) {
    const std::pair<std::string, uint32_t> key{std::string(name), rgb & 0xFFFFFFu};
    const auto found = svgs_.find(key);
    if (found != svgs_.end()) return found->second.Get();  // a name that failed is kept as null
    ComPtr<ID2D1SvgDocument> doc;
    // The colour of the file (#6E504A) is replaced before parsing, so one file serves every state.
    const std::string svg = RecolouredIconSvg(name, key.second);
    if (!svg.empty()) {
        ComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), static_cast<UINT>(svg.size())));
        if (!stream || FAILED(dc_->CreateSvgDocument(stream.Get(), D2D1::SizeF(kIconViewport, kIconViewport), &doc))) {
            doc.Reset();
            LogWarn("[d2d] Could not parse the icon ", std::string(name), ".");
        }
    }
    return svgs_.emplace(key, std::move(doc)).first->second.Get();
}

void D2dCanvas::Icon(const RectF& box, std::string_view name, Colour c) {
    if (!Drawing() || box.w <= 0 || box.h <= 0) return;
    ID2D1SvgDocument* doc = SvgFor(name, c.rgb);
    if (!doc) return;
    // The file is drawn on its 24 x 24 grid; a transform scales that to the box (strokes scale with it).
    dc_->SetTransform(D2D1::Matrix3x2F::Scale(box.w / kIconViewport, box.h / kIconViewport) *
                      D2D1::Matrix3x2F::Translation(box.x, box.y));
    if (c.alpha < 1.0f) PushOpacity(c.alpha);
    dc_->DrawSvgDocument(doc);
    if (c.alpha < 1.0f) PopOpacity();
    dc_->SetTransform(D2D1::Matrix3x2F::Identity());
}

// -------------------------------------------------------------------------- pictures ---

const D2dCanvas::PictureAlpha* D2dCanvas::AlphaFor(std::string_view name) {
    const auto found = pictures_.find(std::string(name));
    if (found != pictures_.end()) return found->second.alpha.empty() ? nullptr : &found->second;
    PictureAlpha pic;  // stays empty if anything fails; kept so the file is tried once
    const std::string_view bytes = AssetBytes(AssetKind::Picture, name);
    if (!bytes.empty() && bytes.size() <= (32u << 20) && wic_ && EnsureFactories()) {
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        HRESULT hr = wic_->CreateStream(&stream);
        if (SUCCEEDED(hr)) {
            hr = stream->InitializeFromMemory(const_cast<BYTE*>(reinterpret_cast<const BYTE*>(bytes.data())),
                                              static_cast<DWORD>(bytes.size()));
        }
        if (SUCCEEDED(hr)) hr = wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        UINT w = 0, h = 0;
        if (SUCCEEDED(hr)) hr = frame->GetSize(&w, &h);
        if (SUCCEEDED(hr) && (w == 0 || h == 0 || w > kMaxPictureSide || h > kMaxPictureSide)) hr = E_FAIL;
        if (SUCCEEDED(hr)) hr = wic_->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr)) {
            hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                       WICBitmapPaletteTypeCustom);
        }
        std::vector<uint8_t> bgra;
        if (SUCCEEDED(hr)) {
            bgra.resize(static_cast<size_t>(w) * h * 4);
            hr = converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(bgra.size()), bgra.data());
        }
        if (SUCCEEDED(hr)) {
            // alpha = 1 - luminance, with the CSS luminance coefficients on the sRGB values: the black
            // lines show, the white paper does not (the old `mask-mode: luminance`, inverted).
            pic.w = static_cast<int>(w);
            pic.h = static_cast<int>(h);
            pic.alpha.resize(static_cast<size_t>(w) * h);
            for (size_t i = 0; i < pic.alpha.size(); ++i) {
                const float lum = 0.2126f * bgra[i * 4 + 2] + 0.7152f * bgra[i * 4 + 1] + 0.0722f * bgra[i * 4];
                pic.alpha[i] = static_cast<uint8_t>(std::clamp(std::lround(255.0f - lum), 0L, 255L));
            }
        } else {
            LogWarn("[d2d] Could not decode the picture ", std::string(name), " (", Hex(hr), ").");
        }
    }
    const auto it = pictures_.emplace(std::string(name), std::move(pic)).first;
    return it->second.alpha.empty() ? nullptr : &it->second;
}

ID2D1Bitmap1* D2dCanvas::PictureBitmap(std::string_view name, uint32_t rgb) {
    const std::pair<std::string, uint32_t> key{std::string(name), rgb & 0xFFFFFFu};
    const auto found = pictureBitmaps_.find(key);
    if (found != pictureBitmaps_.end()) return found->second.Get();
    ComPtr<ID2D1Bitmap1> bitmap;
    if (const PictureAlpha* pic = AlphaFor(name)) {
        // Premultiplied BGRA: the tint scaled by the alpha of each pixel.
        const uint32_t tr = (key.second >> 16) & 0xFF, tg = (key.second >> 8) & 0xFF, tb = key.second & 0xFF;
        std::vector<uint32_t> px(pic->alpha.size());
        for (size_t i = 0; i < px.size(); ++i) {
            const uint32_t a = pic->alpha[i];
            px[i] = (a << 24) | (((tr * a + 127) / 255) << 16) | (((tg * a + 127) / 255) << 8) | ((tb * a + 127) / 255);
        }
        const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f);
        const HRESULT hr = dc_->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(pic->w), static_cast<UINT32>(pic->h)),
                                             px.data(), static_cast<UINT32>(pic->w) * 4, props, &bitmap);
        if (FAILED(hr)) {
            bitmap.Reset();
            LogWarn("[d2d] Could not make the bitmap of the picture ", std::string(name), " (", Hex(hr), ").");
        }
    }
    return pictureBitmaps_.emplace(key, std::move(bitmap)).first->second.Get();
}

void D2dCanvas::Picture(const RectF& box, std::string_view name, Colour tint, float opacity) {
    if (!Drawing() || box.w <= 0 || box.h <= 0) return;
    ID2D1Bitmap1* bitmap = PictureBitmap(name, tint.rgb);
    if (!bitmap) return;
    const D2D1_RECT_F dest = D2D1::RectF(box.x, box.y, box.Right(), box.Bottom());
    // The mask is stretched to the box (the old `mask-size: 100% 100%`); the page gives the box the
    // picture's own proportions. HIGH_QUALITY_CUBIC filters well when the picture is drawn smaller.
    dc_->DrawBitmap(bitmap, &dest, std::clamp(opacity, 0.0f, 1.0f) * tint.alpha,
                    D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr, nullptr);
}

// ------------------------------------------------------------------------ clip, opacity ---

void D2dCanvas::PushClip(const RectF& r) {
    if (!Drawing()) return;
    // Snapped, so the clipped edge is crisp. An empty clip hides everything (width 0).
    dc_->PushAxisAlignedClip(D2D1::RectF(SnapDip(r.x), SnapDip(r.y), std::max(SnapDip(r.x), SnapDip(r.Right())),
                                         std::max(SnapDip(r.y), SnapDip(r.Bottom()))),
                             D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void D2dCanvas::PopClip() {
    if (Drawing()) dc_->PopAxisAlignedClip();
}

void D2dCanvas::PushOpacity(float opacity) {
    if (!Drawing()) return;
    // A layer with an opacity and no layer object: Direct2D makes the temporary one.
    dc_->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                          D2D1::IdentityMatrix(), std::clamp(opacity, 0.0f, 1.0f), nullptr,
                                          D2D1_LAYER_OPTIONS1_NONE),
                   nullptr);
}

void D2dCanvas::PopOpacity() {
    if (Drawing()) dc_->PopLayer();
}

}  // namespace nui

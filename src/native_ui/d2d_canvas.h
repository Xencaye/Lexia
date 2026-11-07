#pragma once
// D2dCanvas: the Direct2D + DirectWrite implementation of nui::Canvas (K.1c, ADR-0009). lexia.exe and
// the dev tools only (Win32); the widgets in lexia_core see just the Canvas interface.
//
// One class, two kinds of target:
//   - a WINDOW: a Direct3D 11 device (the graphics card, else WARP), a flip-model DXGI swap chain for the
//     HWND, its back buffer as the Direct2D target. AttachWindow, Resize, SetDpi, then per paint
//     BeginFrame ... draw ... EndFrame (which presents).
//   - OFF-SCREEN: a bitmap of a given pixel size, for `lexia_settingsshot` and tests. CreateOffscreen,
//     BeginFrame ... EndFrame, then ReadPixels / SavePng.
//
// All numbers of the Canvas calls are DIPs; the context's DPI does the scaling, so the widgets never see
// pixels. Rectangle edges and 1 DIP borders are snapped to whole device pixels (like a browser does) so
// the paper look stays crisp at 125 % and 150 %. The target is opaque (alpha ignored, so ClearType works).
//
// Device loss: when Direct2D says D2DERR_RECREATE_TARGET or DXGI says the device was removed or reset,
// every device resource (D3D device, swap chain, target, brushes, SVG documents, picture bitmaps) is
// dropped, one line is logged, and the next BeginFrame builds them again. The device-independent things
// (factories, fonts, text formats, the decoded pictures) stay. A Canvas call outside BeginFrame..EndFrame,
// or while no device could be made, draws nothing. MeasureText works at any time (it needs no device).
//
// UI thread only. COM must be initialised on it by the caller. Assets come from the exe's resources
// (assets.h) and never from a file.
#include <windows.h>

#include <d2d1_3.h>
#include <d3d11.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "native_ui/canvas.h"
#include "native_ui/fonts.h"

namespace nui {

class D2dCanvas : public Canvas {
public:
    D2dCanvas();
    ~D2dCanvas() override;
    D2dCanvas(const D2dCanvas&) = delete;
    D2dCanvas& operator=(const D2dCanvas&) = delete;

    // ---- targets. Each returns false (and logs) if the device or the target could not be made. A
    // canvas is attached to one target for its life.
    bool AttachWindow(HWND hwnd, int widthPx, int heightPx, float dpi);
    bool CreateOffscreen(int widthPx, int heightPx, float dpi);
    // The client area changed (window) or the picture should be another size (off-screen), in pixels.
    bool Resize(int widthPx, int heightPx);
    // The window moved to a monitor with another DPI, or the user changed the scale. 96 = 1 DIP per pixel.
    void SetDpi(float dpi);
    float Dpi() const { return dpi_; }
    int WidthPx() const { return widthPx_; }
    int HeightPx() const { return heightPx_; }

    // ---- frames
    // Starts drawing; false if there is no device (it could not be made: the frame is skipped).
    bool BeginFrame();
    // Ends drawing; a window is presented. False if the frame was lost (the next BeginFrame retries).
    bool EndFrame();
    // Fills the whole target with a colour, alpha included (the first call of a frame).
    void Clear(Colour c);

    // ---- off-screen results (after EndFrame)
    // The target as premultiplied BGRA (0xAARRGGBB), top row first, widthPx x heightPx.
    bool ReadPixels(std::vector<uint32_t>& out);
    // The target as a PNG file (through WIC). The pixels are un-premultiplied.
    bool SavePng(const std::wstring& path);

    // ---- Canvas
    void FillRect(const RectF& r, Colour c, float radius = 0) override;
    void StrokeRect(const RectF& r, Colour c, float width, float radius = 0) override;
    void FillEllipse(const RectF& bounds, Colour c) override;
    void StrokeEllipse(const RectF& bounds, Colour c, float width) override;
    void Line(PointF a, PointF b, Colour c, float width) override;
    void Polyline(const std::vector<PointF>& points, Colour c, float width) override;
    void Arc(PointF centre, float radius, float startDeg, float sweepDeg, Colour c, float width) override;
    void DottedLine(PointF a, PointF b, Colour c) override;
    void Text(const RectF& box, std::string_view utf8, const TextStyle& style, Colour c) override;
    SizeF MeasureText(std::string_view utf8, const TextStyle& style) override;
    void Icon(const RectF& box, std::string_view name, Colour c) override;
    void Picture(const RectF& box, std::string_view name, Colour tint, float opacity) override;
    void PushClip(const RectF& r) override;
    void PopClip() override;
    void PushOpacity(float opacity) override;
    void PopOpacity() override;
    void PushRotation(PointF centre, float degrees) override;
    void PopRotation() override;

private:
    // The decoded engraving: alpha = 1 - luminance, one byte per pixel. Device independent, so a lost
    // device does not make the file decode again.
    struct PictureAlpha {
        int w = 0, h = 0;
        std::vector<uint8_t> alpha;
    };

    bool EnsureFactories();
    bool EnsureDevice();
    bool CreateTarget();
    void DropDevice(const char* why, HRESULT hr);
    bool Drawing() const { return dc_ && inFrame_; }
    float Scale() const { return dpi_ / 96.0f; }
    // Edges on whole device pixels.
    float SnapDip(float v) const;
    D2D1_COLOR_F ToColor(Colour c) const;
    ID2D1SolidColorBrush* Brush(Colour c);

    IDWriteTextFormat* FormatFor(const TextStyle& style);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> LayoutFor(std::string_view utf8, const TextStyle& style);
    ID2D1SvgDocument* SvgFor(std::string_view name, uint32_t rgb);
    const PictureAlpha* AlphaFor(std::string_view name);
    ID2D1Bitmap1* PictureBitmap(std::string_view name, uint32_t rgb);

    // ---- device-independent (kept for the life of the canvas)
    bool factoriesTried_ = false;
    bool factoriesOk_ = false;
    Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory5> dwrite_;
    FontCollection fonts_;
    bool useAtkinson_ = false;  // the family is in fonts_.collection; else Segoe UI from the system
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> roundStroke_;   // round caps and joins
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> dottedStroke_;  // 1 on, 1 off (in stroke widths), flat caps
    std::map<std::pair<int, int>, Microsoft::WRL::ComPtr<IDWriteTextFormat>> formats_;
    std::map<std::string, PictureAlpha> pictures_;
    std::map<std::tuple<std::string, int, int, bool, int>, SizeF> measured_;  // text, size, weight, underline, spacing

    // ---- device resources (dropped and rebuilt on device loss)
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext5> dc_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    std::map<std::pair<std::string, uint32_t>, Microsoft::WRL::ComPtr<ID2D1SvgDocument>> svgs_;
    std::map<std::pair<std::string, uint32_t>, Microsoft::WRL::ComPtr<ID2D1Bitmap1>> pictureBitmaps_;
    bool lostLogged_ = false;
    bool deviceFailedLogged_ = false;

    // ---- target
    enum class Mode { None, Window, Offscreen };
    Mode mode_ = Mode::None;
    HWND hwnd_ = nullptr;
    int widthPx_ = 0, heightPx_ = 0;
    float dpi_ = 96.0f;
    bool inFrame_ = false;
    // The rotation of PushRotation (identity otherwise). Icon sets its own transform for each icon, so it
    // composes this one in and puts it back, instead of resetting to the identity.
    D2D1_MATRIX_3X2_F userTransform_ = D2D1::Matrix3x2F::Identity();
};

}  // namespace nui

// CP40: WIC-backed raster capture for ChartView. Reuses the chart's
// WM_PRINTCLIENT handler (already wired in ChartView::handle_message) so
// the exported image matches the on-screen render exactly, including the
// interaction overlay and the antialiased stroke path. This TU is the
// only place that links windowscodecs and ole32; both stay out of the
// rest of the nfui_charts surface.

#include <nfui/Charts.hpp>

#include <wrl/client.h>
#include <wincodec.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

enum class ImageFormat { png, bmp };

// RAII for HDC acquired via GetDC(hwnd) so a return path always releases.
class WindowDC {
public:
    explicit WindowDC(HWND h) noexcept : hwnd_(h), dc_(GetDC(h)) {}
    ~WindowDC() noexcept { if (dc_) ReleaseDC(hwnd_, dc_); }
    WindowDC(const WindowDC&) = delete;
    WindowDC& operator=(const WindowDC&) = delete;

    [[nodiscard]] HDC get() const noexcept { return dc_; }
private:
    HWND hwnd_{};
    HDC dc_{};
};

// RAII for memory HDC + selected DIB. The DIB is selected into the DC at
// construction; both are deleted in reverse order at destruction.
class MemorySurface {
public:
    MemorySurface() noexcept = default;
    ~MemorySurface() noexcept {
        if (dc_) {
            if (prev_bitmap_) SelectObject(dc_, prev_bitmap_);
            if (dib_) DeleteObject(dib_);
            DeleteDC(dc_);
        }
    }
    MemorySurface(const MemorySurface&) = delete;
    MemorySurface& operator=(const MemorySurface&) = delete;

    [[nodiscard]] bool create(HDC source, int width, int height) noexcept {
        dc_ = CreateCompatibleDC(source);
        if (dc_ == nullptr) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;  // top-down so DIB rows match PrintWindow
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        info.bmiHeader.biSizeImage = static_cast<DWORD>(width * height * 4);
        // Allocate the DIB and select it into the DC so we can write
        // BGRA pixels and PrintWindow straight into it. Using
        // DIB_RGB_COLORS keeps the color table reference stable.
        void* bits = nullptr;
        dib_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib_ == nullptr) {
            DeleteDC(dc_);
            dc_ = nullptr;
            return false;
        }
        prev_bitmap_ = static_cast<HBITMAP>(SelectObject(dc_, dib_));
        return true;
    }

    [[nodiscard]] HDC dc() const noexcept { return dc_; }

private:
    HDC dc_{};
    HBITMAP dib_{};
    HGDIOBJ prev_bitmap_{};
};

[[nodiscard]] bool export_chart_internal(HWND hwnd,
                                         const std::wstring& path,
                                         ImageFormat format) noexcept {
    if (hwnd == nullptr || path.empty() || !IsWindow(hwnd)) return false;

    RECT client{};
    if (!GetClientRect(hwnd, &client)) return false;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return false;

    // Local COM apartment bring-up so WIC works without an outer
    // CoInitialize call. Treat RPC_E_CHANGED_MODE as "another apartment
    // already initialized COM" — keep using WIC without calling
    // CoUninitialize.
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool must_uninitialize = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;

    bool ok = false;

    WindowDC window_dc(hwnd);
    if (window_dc.get() == nullptr) goto cleanup_com;

    {
        MemorySurface surface;
        if (!surface.create(window_dc.get(), width, height)) goto cleanup_com;

        // Drive the chart's WM_PRINTCLIENT path so the exported pixels
        // match what the user sees (axis chrome, AA strokes, legend).
        // PW_CLIENTONLY skips the non-client chrome (title bar etc.).
        if (!PrintWindow(hwnd, surface.dc(), PW_CLIENTONLY)) goto cleanup_com;

        // The chart leaves the DIB's reserved alpha bytes at 0 (the
        // default for CreateDIBSection). PNG expects fully opaque BGRA
        // so rewrite every alpha byte to 0xFF. BitBlt then stream the
        // resulting DIB into the WIC encoder.
        const std::size_t pixel_count =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height);
        BITMAPINFO read_info{};
        read_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        read_info.bmiHeader.biWidth = width;
        read_info.bmiHeader.biHeight = -height;
        read_info.bmiHeader.biPlanes = 1;
        read_info.bmiHeader.biBitCount = 32;
        read_info.bmiHeader.biCompression = BI_RGB;
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
        if (GetDIBits(surface.dc(), /*hbm=*/nullptr, 0, height,
                      pixels.data(), &read_info, DIB_RGB_COLORS) == 0) {
            goto cleanup_com;
        }
        // GetDIBits ignored hbm (queried via the DC's selected bitmap).
        // Restore alpha bytes so PNG/BMP save opaque pixels.
        for (std::size_t i = 0; i < pixel_count; ++i) {
            pixels[i * 4 + 3] = 0xFF;
        }

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) goto cleanup_com;

        Microsoft::WRL::ComPtr<IWICStream> stream;
        hr = factory->CreateStream(&stream);
        if (FAILED(hr)) goto cleanup_com;
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
        if (FAILED(hr)) goto cleanup_com;

        const GUID container = (format == ImageFormat::png)
            ? GUID_ContainerFormatPng
            : GUID_ContainerFormatBmp;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(container, nullptr, &encoder);
        if (FAILED(hr)) goto cleanup_com;
        hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) goto cleanup_com;

        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(&frame, nullptr);
        if (FAILED(hr)) goto cleanup_com;
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) goto cleanup_com;
        hr = frame->SetSize(static_cast<UINT>(width),
                            static_cast<UINT>(height));
        if (FAILED(hr)) goto cleanup_com;

        const WICPixelFormatGUID pixel_format =
            (format == ImageFormat::png)
                ? GUID_WICPixelFormat32bppBGRA
                : GUID_WICPixelFormat32bppBGR;
        hr = frame->SetPixelFormat(const_cast<WICPixelFormatGUID*>(&pixel_format));
        if (FAILED(hr)) goto cleanup_com;
        hr = frame->WritePixels(height, width * 4,
                                static_cast<UINT>(pixels.size()),
                                pixels.data());
        if (FAILED(hr)) goto cleanup_com;
        hr = frame->Commit();
        if (FAILED(hr)) goto cleanup_com;
        hr = encoder->Commit();
        if (FAILED(hr)) goto cleanup_com;

        ok = true;
    }

cleanup_com:
    if (must_uninitialize) CoUninitialize();
    return ok;
}

} // namespace

namespace nfui {

bool ChartView::export_to_png(const std::wstring& path) const noexcept {
    return export_chart_internal(hwnd(), path, ImageFormat::png);
}

bool ChartView::export_to_bmp(const std::wstring& path) const noexcept {
    return export_chart_internal(hwnd(), path, ImageFormat::bmp);
}

} // namespace nfui

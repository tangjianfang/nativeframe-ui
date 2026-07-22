#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <cstdio>
#include <string>
#include <vector>

namespace nfui {

namespace {

// Class name registered with Window::register_window_class. Must be unique per
// Window subclass because the base class refuses to register an existing class
// whose proc does not match Window::window_proc.
constexpr const wchar_t* kChartViewClassName = L"NativeFrameUIChartView";

void draw_default_placeholder(HWND hwnd,
                              HDC target,
                              const RECT& bounds,
                              const ThemePalette& palette,
                              FontCache* fonts) noexcept {
    fill_rect(target, bounds, palette.background);

    // Plot area: inset from the client area so the frame and axes fit cleanly.
    const int legend_h = 24;
    const int axis_pad = 32;
    RECT plot{};
    plot.left = bounds.left + axis_pad;
    plot.top = bounds.top + legend_h;
    plot.right = bounds.right - 8;
    plot.bottom = bounds.bottom - axis_pad;
    if (plot.right <= plot.left || plot.bottom <= plot.top) {
        return;
    }

    // Plot frame.
    draw_line(target, POINT{plot.left, plot.top}, POINT{plot.right, plot.top}, palette.border, 1);
    draw_line(target, POINT{plot.left, plot.bottom}, POINT{plot.right, plot.bottom}, palette.border, 1);
    draw_line(target, POINT{plot.left, plot.top}, POINT{plot.left, plot.bottom}, palette.border, 1);
    draw_line(target, POINT{plot.right, plot.top}, POINT{plot.right, plot.bottom}, palette.border, 1);

    // Legend box at the top.
    RECT legend{plot.left, bounds.top + 4, plot.left + 96, bounds.top + 4 + 16};
    fill_rect(target, legend, palette.surface);
    draw_line(target, POINT{legend.left, legend.top}, POINT{legend.right, legend.top}, palette.border, 1);
    draw_line(target, POINT{legend.left, legend.bottom}, POINT{legend.right, legend.bottom}, palette.border, 1);
    draw_line(target, POINT{legend.left, legend.top}, POINT{legend.left, legend.bottom}, palette.border, 1);
    draw_line(target, POINT{legend.right, legend.top}, POINT{legend.right, legend.bottom}, palette.border, 1);

    // Mono tick labels along x (bottom) and y (left). Real axis ticks land in C3/C4.
    // Point 9 matches the C3/C4 tick font so the default placeholder reads with the
    // same weight as the real axis labels once a subclass overrides on_paint.
    const int dpi = (hwnd != nullptr) ? dpi_of(hwnd) : 96;
    HFONT tick_font = (fonts != nullptr) ? fonts->mono(dpi, 9) : nullptr;
    wchar_t buf[32]{};
    for (int i = 0; i <= 4; ++i) {
        const int x = plot.left + (plot.right - plot.left) * i / 4;
        std::swprintf(buf, std::size(buf), L"%d", i);
        RECT label{x - 16, plot.bottom + 4, x + 16, plot.bottom + 18};
        draw_text(target, label, buf, tick_font, palette.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    }
    for (int i = 0; i <= 4; ++i) {
        const int y = plot.bottom - (plot.bottom - plot.top) * i / 4;
        std::swprintf(buf, std::size(buf), L"%d", i);
        RECT label{plot.left - 28, y - 8, plot.left - 4, y + 8};
        draw_text(target, label, buf, tick_font, palette.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

} // namespace

bool ChartView::create(const WindowCreateParams& params) noexcept {
    // Override the requested class with our private chart class so the proc
    // registration succeeds. WindowCreateParams is a plain struct (no setters);
    // we copy and substitute before forwarding.
    WindowCreateParams owned = params;
    owned.class_name = kChartViewClassName;
    if (!Window::create(owned)) {
        return false;
    }
    palette_ = nullptr;
    fonts_ = nullptr;
    return true;
}

void ChartView::set_kind(ChartKind kind) noexcept {
    kind_ = kind;
}

void ChartView::set_series(std::vector<ChartSeries> series) noexcept {
    series_ = std::move(series);
}

void ChartView::set_axis_x(ChartAxisRange axis) noexcept {
    axis_x_ = axis;
}

void ChartView::set_axis_y(ChartAxisRange axis) noexcept {
    axis_y_ = axis;
}

void ChartView::set_palette(const ThemePalette* palette) noexcept {
    palette_ = palette;
}

void ChartView::set_font_cache(FontCache* fonts) noexcept {
    fonts_ = fonts;
}

LRESULT ChartView::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_ERASEBKGND:
        // We self-paint the background; suppress the default COLOR_WINDOW erase
        // so the area between refreshes stays ours (matches Controls gallery).
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hwnd(), &paint);
        RECT client{};
        GetClientRect(hwnd(), &client);
        // Flicker-free offscreen buffer over the full client area. The
        // MemoryDC destructor BitBlts back to the target rect origin while the
        // BeginPaint DC is still valid, so the buffer flush MUST happen before
        // EndPaint (R6 fix from SettingsDemo).
        {
            nfui::MemoryDC mem(hdc, client);
            HDC target = mem.valid() ? mem.dc() : hdc;
            on_paint(target, client);
        }
        EndPaint(hwnd(), &paint);
        return 0;
    }
    case WM_PRINTCLIENT: {
        // PrintWindow / owner-draw snapshot path. The parent supplies an HDC in
        // wparam and we render directly to it (no BeginPaint/EndPaint required).
        // C2 keeps the path wired so a future OCM_DRAWITEM integration has a
        // drop-in target without changing the paint code.
        HDC target_dc = reinterpret_cast<HDC>(wparam);
        if (target_dc == nullptr) {
            return 0;
        }
        RECT client{};
        GetClientRect(hwnd(), &client);
        on_paint(target_dc, client);
        return 0;
    }
    default:
        break;
    }
    return Window::handle_message(message, wparam, lparam);
}

void ChartView::on_paint(HDC hdc, const RECT& bounds) {
    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);
    draw_default_placeholder(hwnd(), hdc, bounds, pal, fonts_);
}

} // namespace nfui

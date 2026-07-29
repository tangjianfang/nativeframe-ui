#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include "internal/ChartsPaint.hpp"

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
                              FontCache* fonts,
                              const ChartSettings& settings) noexcept {
    fill_rect(target, bounds, palette.background);

    // Plot area: inset so the frame, axes, and axis-title bands all fit.
    // The gutters mirror compute_chart_layout / draw_axis_titles so a bare
    // ChartView reads as a coordinate system with titled axes, matching the
    // real renderers once a subclass overrides on_paint.
    constexpr int legend_h = 24;
    const int left_pad   = charts_internal::kAxisGutter;       // y tick labels
    const int top_pad    = legend_h + charts_internal::kAxisTitleHeight;  // legend + y-title band
    const int right_pad  = 8;
    const int bottom_pad = charts_internal::kBottomGutter +
                           charts_internal::kAxisTitleHeight;  // x ticks + x-title band
    RECT plot{};
    plot.left = bounds.left + left_pad;
    plot.top = bounds.top + top_pad;
    plot.right = bounds.right - right_pad;
    plot.bottom = bounds.bottom - bottom_pad;
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
    HFONT tick_font = (fonts != nullptr) ? fonts->mono(dpi, font_pt::chart_tick) : nullptr;
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

    // Axis titles in the bands reserved above the plot (y) and below the x
    // ticks (x). Empty labels suppress that axis. Uses the same regular sm
    // title font as the real renderers so the placeholder chrome matches.
    HFONT title_font = (fonts != nullptr) ? fonts->regular(dpi, font_pt::sm) : nullptr;
    ChartLayout layout{};
    layout.plot_bounds = plot;
    charts_internal::draw_axis_titles(target, layout,
                                      settings.x_axis_label,
                                      settings.y_axis_label,
                                      title_font, palette);
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

void ChartView::set_series_visible(std::size_t idx, bool visible) noexcept {
    if (idx >= series_.size()) return;
    if (series_[idx].visible == visible) return;
    series_[idx].visible = visible;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

bool ChartView::is_series_visible(std::size_t idx) const noexcept {
    if (idx >= series_.size()) return false;
    return series_[idx].visible;
}

LRESULT ChartView::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    // Route interaction messages first when the controller is active.
    if (interaction_enabled()) {
        const LRESULT handled = handle_interaction_message(message, wparam, lparam);
        if (handled != -1) {
            return handled;
        }
    }

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
    const ThemePalette& pal = effective_palette();
    draw_default_placeholder(hwnd(), hdc, bounds, pal, fonts_, settings_);
    paint_interaction_overlay(hdc, bounds);
}

// CP39: central palette resolver. Renderers (and ChartView::on_paint)
// call this instead of duplicating the host-pointer fallback so a chart
// configured with ChartSettings::theme = dark still paints dark when
// the host forgot to rebind palette_ after apply_settings().
const ThemePalette& ChartView::effective_palette() const noexcept {
    if (palette_ != nullptr) return *palette_;
    return fallback_palette_;
}

// CP39: refresh `fallback_palette_` from the current settings_.theme.
// theme_palette() returns by value so we copy into our own storage so
// renderers can take a reference that outlives the helper's temporary.
void ChartView::refresh_fallback_palette() noexcept {
    const ThemeMode mode = settings_.theme == ChartSettings::ThemeMode::dark
                              ? ThemeMode::dark
                              : ThemeMode::light;
    fallback_palette_ = theme_palette(mode);
}

} // namespace nfui

// CP40: minimal line-only chart. Reuses Charts.cpp's normalize_points
// helper for the coordinate transform and calls draw_polyline_aa from
// the AA library to draw the stroke. No axes, grid, or legend.

#include <nfui/Sparkline.hpp>
#include <nfui/Charts.hpp>
#include <nfui/Paint.hpp>

#include "internal/ChartsPaint.hpp"

namespace nfui {

namespace {

constexpr const wchar_t* kSparklineClassName = L"NativeFrameUISparkline";

// Epsilon used to expand degenerate extents so normalize_points never
// divides by zero on a flat series. 1e-6 is small enough to be invisible
// but big enough to dodge the worst of the float artifacts at 100% DPI.
constexpr double kExtentEpsilon = 1e-6;

struct Extent {
    double min_x{};
    double max_x{};
    double min_y{};
    double max_y{};
};

[[nodiscard]] Extent calculate_extent(const std::vector<ChartPoint>& pts) noexcept {
    Extent e{};
    if (pts.empty()) return e;
    e.min_x = e.max_x = pts.front().x;
    e.min_y = e.max_y = pts.front().y;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double x = pts[i].x;
        const double y = pts[i].y;
        if (x < e.min_x) e.min_x = x;
        if (x > e.max_x) e.max_x = x;
        if (y < e.min_y) e.min_y = y;
        if (y > e.max_y) e.max_y = y;
    }
    // Symmetric epsilon expansion when an axis is degenerate.
    if (e.max_x - e.min_x < kExtentEpsilon) {
        const double mid = (e.min_x + e.max_x) * 0.5;
        e.min_x = mid - kExtentEpsilon;
        e.max_x = mid + kExtentEpsilon;
    }
    if (e.max_y - e.min_y < kExtentEpsilon) {
        const double mid = (e.min_y + e.max_y) * 0.5;
        e.min_y = mid - kExtentEpsilon;
        e.max_y = mid + kExtentEpsilon;
    }
    return e;
}

} // namespace

bool Sparkline::create(const WindowCreateParams& params) noexcept {
    WindowCreateParams owned = params;
    owned.class_name = kSparklineClassName;
    return Window::create(owned);
}

void Sparkline::set_points(std::vector<ChartPoint> points) {
    points_ = std::move(points);
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void Sparkline::set_color(Color color) noexcept {
    color_ = color;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void Sparkline::set_palette(const ThemePalette* palette) noexcept {
    palette_ = palette;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void Sparkline::set_line_width(int logical_px) noexcept {
    line_width_px_ = (logical_px < 1) ? 1 : logical_px;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

LRESULT Sparkline::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd(), &ps);
        RECT client{};
        GetClientRect(hwnd(), &client);
        on_paint(hdc, client);
        EndPaint(hwnd(), &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;  // we self-paint the background; suppress default
    default:
        break;
    }
    return Window::handle_message(message, wparam, lparam);
}

void Sparkline::on_paint(HDC hdc, const RECT& bounds) noexcept {
    const ThemePalette fallback = theme_palette(ThemeMode::light);
    const ThemePalette& pal = palette_ ? *palette_ : fallback;
    fill_rect(hdc, bounds, pal.surface);

    if (points_.size() < 2) return;

    // 4 px inset keeps the AA endpoints fully inside the child window
    // so the rounded corners don't clip the smooth stroke.
    RECT plot = bounds;
    InflateRect(&plot, -4, -4);
    if (plot.right <= plot.left || plot.bottom <= plot.top) return;

    const Extent ext = calculate_extent(points_);
    ChartLayout layout{};
    layout.plot_bounds = plot;
    const std::vector<POINT> pixels = normalize_points(
        points_, layout,
        ChartAxisRange{ext.min_x, ext.max_x},
        ChartAxisRange{ext.min_y, ext.max_y});

    const Color stroke = color_.rgb
        ? color_
        : Color{pal.accent.rgb};
    const int width = (line_width_px_ < 1) ? 1 : line_width_px_;
    charts_internal::draw_polyline_aa(hdc, pixels.data(),
                                      static_cast<int>(pixels.size()),
                                      stroke, width);
}

} // namespace nfui

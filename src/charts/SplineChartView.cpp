#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include "internal/ChartsPaint.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace nfui {

namespace {

// Tick visual constants match LineChartView / BarChartView so the three
// renders stay visually consistent when shown side-by-side. Mono tick label
// point size uses the shared font_pt token (font_pt::chart_tick). Legend
// constants live in internal/ChartsPaint.cpp.
constexpr int kTickCount = 5;
constexpr int kAxisLabelGutter = 28;
// Spline uses a thinner stroke than the polyline renderer — a heavy stroke
// fights the smoothness of the curve and produces a muddy line.
constexpr int kSplineLineWidthPx = 3;
// CP30: data-point marker radius. Drawn over the spline so readers can
// see where the samples actually sit on the curve. Kept small (3 px) so
// the marker dots decorate without dominating the line.
constexpr int kSplineMarkerRadiusPx = 3;

// (Tick labels are formatted inline via nfui::format_axis_tick so callers can
// pick the precision per axis via ChartAxisRange::label_format.)

void draw_plot_frame(HDC hdc, const ChartLayout& layout, const ThemePalette& pal) noexcept {
    const RECT& pb = layout.plot_bounds;
    draw_line(hdc, POINT{pb.left, pb.top},     POINT{pb.right, pb.top},     pal.border, 1);
    draw_line(hdc, POINT{pb.left, pb.bottom},  POINT{pb.right, pb.bottom},  pal.border, 1);
    draw_line(hdc, POINT{pb.left, pb.top},     POINT{pb.left, pb.bottom},   pal.border, 1);
    draw_line(hdc, POINT{pb.right, pb.top},    POINT{pb.right, pb.bottom},  pal.border, 1);
}

void draw_value_axis_ticks_v(HDC hdc,
                             const ChartLayout& layout,
                             const ChartAxisRange& axis_y,
                             HFONT font,
                             const ThemePalette& pal) noexcept {
    const RECT& pb = layout.plot_bounds;
    const int plot_h = pb.bottom - pb.top;
    if (plot_h <= 0) return;
    for (int i = 0; i <= kTickCount; ++i) {
        const double t = static_cast<double>(i) / kTickCount;
        const double value = axis_y.min + t * (axis_y.max - axis_y.min);
        const int py = pb.top + static_cast<int>((1.0 - t) * static_cast<double>(plot_h) + 0.5);
        const std::wstring text = format_axis_tick(value, axis_y.label_format);
        RECT label{pb.left - kAxisLabelGutter - 4, py - 8, pb.left - 4, py + 8};
        draw_text(hdc, label, text, font, pal.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        draw_line(hdc, POINT{pb.left - 3, py}, POINT{pb.left, py}, pal.border, 1);
    }
}

void draw_index_axis_ticks_v(HDC hdc,
                             const ChartLayout& layout,
                             std::size_t point_count,
                             HFONT font,
                             const ThemePalette& pal) noexcept {
    if (point_count == 0) return;
    const RECT& pb = layout.plot_bounds;
    const int plot_w = pb.right - pb.left;
    wchar_t buf[32]{};
    const std::size_t n = std::min<std::size_t>(point_count, kTickCount);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (n == 1) ? 0.5
                                  : static_cast<double>(i) / static_cast<double>(n - 1);
        const int px = pb.left + static_cast<int>(t * static_cast<double>(plot_w) + 0.5);
        std::swprintf(buf, std::size(buf), L"%zu",
                      1 + i * (point_count - 1) / std::max<std::size_t>(1, n - 1));
        RECT label{px - 24, pb.bottom + 4, px + 24, pb.bottom + kAxisLabelGutter};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        draw_line(hdc, POINT{px, pb.bottom}, POINT{px, pb.bottom + 3}, pal.border, 1);
    }
}

// Column of color swatches + series names on the right of the plot. The
// implementation lives in internal/ChartsPaint.cpp so all four chart views
// (BarChartView / HBarChartView / LineChartView / SplineChartView) render
// the legend identically.

} // namespace

void SplineChartView::set_tension(double t) noexcept {
    tension_ = std::clamp(t, 0.0, 1.0);
}

void SplineChartView::on_paint(HDC hdc, const RECT& bounds) {
    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);

    fill_rect(hdc, bounds, pal.background);

    const std::size_t series_count = series_.size();
    ChartLayout layout = compute_chart_layout(bounds, ChartKind::spline, series_count);
    if (layout.plot_bounds.right <= layout.plot_bounds.left ||
        layout.plot_bounds.bottom <= layout.plot_bounds.top) {
        return;
    }

    std::size_t point_count = 0;
    for (const auto& s : series_) {
        point_count = std::max(point_count, s.points.size());
    }

    // Light inner grid behind the spline so sample points read against
    // horizontal/vertical alignment guides instead of a blank background.
    const Color grid_color = charts_internal::derive_grid_color(pal);
    charts_internal::draw_grid_hlines(hdc, layout.plot_bounds, grid_color, kTickCount);
    charts_internal::draw_grid_vlines(hdc, layout.plot_bounds, grid_color, kTickCount);

    draw_plot_frame(hdc, layout, pal);

    if (point_count == 0) {
        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                            layout.legend_width_px, series_,
                                            palette_, fonts_, dpi);
        return;
    }

    for (const ChartSeries& series : series_) {
        if (series.points.size() < 2) continue;  // need at least two anchors
        const std::vector<POINT> pts =
            normalize_points(series.points, layout, axis_x_, axis_y_);
        if (pts.size() < 2) continue;
        const std::vector<POINT> bez = catmull_rom_to_bezier(pts, tension_);
        if (bez.empty()) continue;
        // CP28: catmull_rom_to_bezier emits 1 + 3*(n-1) chained-cubic points
        // (start anchor + 3 per segment); both PolyBezier and Graphics::
        // DrawBeziers consume that layout. The previous version emitted
        // 4*(n-1) — that count was wrong by 3n-1 and was the reason Win32
        // silently dropped the spline (the empty chart CP27 caught).
        const int bez_count = static_cast<int>(bez.size());
        if (bez_count < 4) continue;

        // CP28: catmull_rom_to_bezier emits 1 + 3*(n-1) chained-cubic points
        // (start anchor + 3 per segment); both PolyBezier and Graphics::
        // DrawBeziers consume that layout. The previous version emitted
        // 4*(n-1) — that count was wrong by 3n-1 and was the reason Win32
        // silently dropped the spline (the empty chart CP27 caught). The
        // AA helper now falls back to pure-GDI PolyBezier on Graphics
        // construction failure (e.g. the memory DCs PrintWindow produces),
        // so the spline always renders even when GDI+ cannot bind.
        charts_internal::draw_beziers_aa(
            hdc, bez.data(), bez_count, series.color, kSplineLineWidthPx);
        // CP30: marker dots at every sample so readers can see where the
        // curve actually anchors. Drawn after the stroke so the dot sits
        // on top of the line; fill_circles_aa goes through the same
        // GDI+->GDI overdraw fallback as the line, so the markers always
        // appear even on memory DCs where Graphics::FillEllipse no-ops.
        charts_internal::fill_circles_aa(
            hdc, pts.data(), static_cast<int>(pts.size()),
            kSplineMarkerRadiusPx, series.color);
    }

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT tick_font = (fonts_ != nullptr) ? fonts_->mono(dpi, font_pt::chart_tick) : nullptr;

    draw_value_axis_ticks_v(hdc, layout, axis_y_, tick_font, pal);
    draw_index_axis_ticks_v(hdc, layout, point_count, tick_font, pal);
    charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                        layout.legend_width_px, series_,
                                        palette_, fonts_, dpi);
}

} // namespace nfui
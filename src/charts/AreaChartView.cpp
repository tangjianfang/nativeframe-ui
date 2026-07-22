#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include "internal/ChartsPaint.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace nfui {

namespace {

constexpr int kTickCount = 5;
constexpr int kAxisLabelGutter = 18;
constexpr int kLineWidthPx = 2;

// CP14: shared with LineChartView. Could be lifted into internal/ChartsPaint
// if a third view (e.g. ScatterChartView) needs the same plot frame; for
// now the duplication is the minimum honest cost.
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
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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
        RECT label{px - 16, pb.bottom + 4, px + 16, pb.bottom + kAxisLabelGutter};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        draw_line(hdc, POINT{px, pb.bottom}, POINT{px, pb.bottom + 3}, pal.border, 1);
    }
}

} // namespace

void AreaChartView::set_outline(bool enabled) noexcept {
    outline_ = enabled;
}

void AreaChartView::set_fill_alpha(double alpha) noexcept {
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    fill_alpha_ = alpha;
}

void AreaChartView::on_paint(HDC hdc, const RECT& bounds) {
    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);

    fill_rect(hdc, bounds, pal.background);

    const std::size_t series_count = series_.size();
    ChartLayout layout = compute_chart_layout(bounds, ChartKind::area, series_count);
    if (layout.plot_bounds.right <= layout.plot_bounds.left ||
        layout.plot_bounds.bottom <= layout.plot_bounds.top) {
        return;
    }

    std::size_t point_count = 0;
    for (const auto& s : series_) {
        point_count = std::max(point_count, s.points.size());
    }

    draw_plot_frame(hdc, layout, pal);

    if (point_count == 0) {
        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                            layout.legend_width_px, series_,
                                            palette_, fonts_, dpi);
        return;
    }

    // Baseline = plot bottom in screen coords. We anchor the fill there so
    // the polygon spans [normalized_line, baseline] for each series.
    const int baseline_y = layout.plot_bounds.bottom;

    for (const ChartSeries& series : series_) {
        if (series.points.empty()) continue;
        std::vector<POINT> pts =
            normalize_points(series.points, layout, axis_x_, axis_y_);
        if (pts.size() < 2) {
            // Single-point series: nothing meaningful to fill. Skip rather
            // than render a degenerate triangle.
            continue;
        }

        // Build the polygon: line points followed by baseline anchor points
        // so Polygon() closes back to the start at the lower-left corner.
        std::vector<POINT> poly;
        poly.reserve(pts.size() + 3);
        for (const POINT& p : pts) {
            poly.push_back(p);
        }
        poly.push_back(POINT{pts.back().x,  baseline_y});
        poly.push_back(POINT{pts.front().x, baseline_y});

        // Fill color = series.color blended toward palette.surface by
        // (1 - fill_alpha_) so a configurable alpha reads as a lighter
        // tint; alpha == 1 returns the original color. The blend is
        // palette-aware so the fill always tracks the surface tint.
        Color fill = (fill_alpha_ >= 1.0)
            ? series.color
            : alpha_blend(series.color, pal.surface, static_cast<float>(1.0 - fill_alpha_));
        // When multiple series overlap, draw back-to-front so the upper
        // series wins visually. Single series still draws correctly.
        const HBRUSH brush = CreateSolidBrush(fill.rgb);
        if (brush != nullptr) {
            const HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            const HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, brush));
            Polygon(hdc, poly.data(), static_cast<int>(poly.size()));
            SelectObject(hdc, old_brush);
            SelectObject(hdc, old_pen);
            DeleteObject(brush);
        }

        if (outline_) {
            // Outline = the upper edge only, so the polygon does not draw
            // a stroke along the baseline (which would visually collide
            // with the plot frame).
            charts_internal::draw_polyline_aa(
                hdc, pts.data(), static_cast<int>(pts.size()),
                series.color, kLineWidthPx);
        }
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
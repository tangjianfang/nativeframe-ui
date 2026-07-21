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

// Tick and legend visual constants match BarChartView / HBarChartView so the
// three renders stay visually consistent when shown side-by-side.
constexpr int kTickFontPt = 9;
constexpr int kTickCount = 5;
constexpr int kLegendSwatch = 12;
constexpr int kLegendPadX = 8;
constexpr int kLegendPadY = 6;
constexpr int kLegendRowH = 18;
constexpr int kAxisLabelGutter = 18;
constexpr int kLineWidthPx = 2;

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
    // Up to kTickCount ticks under the x axis so dense series don't crowd.
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

void draw_legend_column(HDC hdc,
                        const ChartLayout& layout,
                        const RECT& bounds,
                        const std::vector<ChartSeries>& series,
                        HFONT font,
                        const ThemePalette& pal) noexcept {
    if (series.size() < 2 || layout.legend_width_px <= 0) return;

    const int col_w = layout.legend_width_px;
    const int col_left = bounds.left +
                         (layout.plot_bounds.right - bounds.left) +
                         (col_w > 0 ? 8 : 0);
    const int col_right = col_left + col_w - kLegendPadX;
    const int col_h = static_cast<int>(series.size()) * kLegendRowH + 2 * kLegendPadY;
    const int col_top = layout.plot_bounds.top +
                        ((layout.plot_bounds.bottom - layout.plot_bounds.top) - col_h) / 2;
    const int col_bottom = col_top + col_h;

    RECT col{col_left, col_top, col_right, col_bottom};
    fill_rect(hdc, col, pal.surface);
    draw_line(hdc, POINT{col.left, col.top}, POINT{col.right, col.top}, pal.border, 1);
    draw_line(hdc, POINT{col.left, col.bottom}, POINT{col.right, col.bottom}, pal.border, 1);
    draw_line(hdc, POINT{col.left, col.top}, POINT{col.left, col.bottom}, pal.border, 1);
    draw_line(hdc, POINT{col.right, col.top}, POINT{col.right, col.bottom}, pal.border, 1);

    for (std::size_t i = 0; i < series.size(); ++i) {
        const int row_y = col.top + kLegendPadY + static_cast<int>(i) * kLegendRowH;
        RECT swatch{col.left + kLegendPadX, row_y + 2,
                    col.left + kLegendPadX + kLegendSwatch,
                    row_y + 2 + kLegendSwatch};
        fill_rounded_rect(hdc, swatch, theme_metrics().corner_radius_control,
                          series[i].color, series[i].color);
        RECT text_rc{swatch.right + kLegendPadX, row_y, col.right - kLegendPadX, row_y + kLegendRowH};
        draw_text(hdc, text_rc, series[i].name, font, pal.text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

} // namespace

void LineChartView::set_point_radius(int logical_px) noexcept {
    point_radius_px_ = logical_px < 0 ? 0 : logical_px;
}

void LineChartView::on_paint(HDC hdc, const RECT& bounds) {
    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);

    fill_rect(hdc, bounds, pal.background);

    const std::size_t series_count = series_.size();
    ChartLayout layout = compute_chart_layout(bounds, ChartKind::line, series_count);
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
        HFONT font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;
        draw_legend_column(hdc, layout, bounds, series_, font, pal);
        return;
    }

    for (const ChartSeries& series : series_) {
        if (series.points.empty()) continue;
        const std::vector<POINT> pts =
            normalize_points(series.points, layout, axis_x_, axis_y_);
        if (pts.size() < 2) {
            // Single-point series: draw a single marker (if enabled) and continue.
            if (point_radius_px_ > 0) {
                charts_internal::fill_circles_aa(
                    hdc, &pts[0], 1, point_radius_px_, series.color);
            }
            continue;
        }
        charts_internal::draw_polyline_aa(
            hdc, pts.data(), static_cast<int>(pts.size()),
            series.color, kLineWidthPx);
        if (point_radius_px_ > 0) {
            charts_internal::fill_circles_aa(
                hdc, pts.data(), static_cast<int>(pts.size()),
                point_radius_px_, series.color);
        }
    }

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT tick_font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;

    draw_value_axis_ticks_v(hdc, layout, axis_y_, tick_font, pal);
    draw_index_axis_ticks_v(hdc, layout, point_count, tick_font, pal);
    draw_legend_column(hdc, layout, bounds, series_, tick_font, pal);
}

} // namespace nfui
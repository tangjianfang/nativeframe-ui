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

// Tick visual constants. Mono tick label point size mirrors the C2 default
// paint (ChartView::draw_default_placeholder uses point 8; we bump to 9 since
// real labels carry formatted floats and benefit from a touch more weight).
// Legend-specific constants live in internal/ChartsPaint.cpp.
constexpr int kTickFontPt = 9;
constexpr int kTickCount = 5;
constexpr int kAxisLabelGutter = 18;
// Half-width of the tick-label text rect (centered on the tick position).
// The category-axis labels are 1..N where N can be > 12 (e.g. 12 monthly
// bars). With a per-tick spacing of ~33px in a 400px plot, the previous
// 16px half-width caused adjacent labels to overlap. Bumping to 24px
// gives the ellipsized label room to breathe; if the chart is narrower
// than the label band, DT_END_ELLIPSIS truncates without bleeding into
// the neighbour's rect.
constexpr int kTickLabelHalfWidthPx = 24;

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
        // DT_END_ELLIPSIS keeps long formatted values (e.g. "1,234.5") from
        // bleeding into the next tick label's rect; the half-width below is
        // intentionally generous so the truncation only fires on truly wide
        // formats, not normal 2-4 character ticks.
        RECT label{pb.left - kAxisLabelGutter - 8, py - 8, pb.left - 4, py + 8};
        draw_text(hdc, label, text, font, pal.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Faint tick mark on the plot left edge.
        draw_line(hdc, POINT{pb.left - 3, py}, POINT{pb.left, py}, pal.border, 1);
    }
}

void draw_category_axis_ticks_v(HDC hdc,
                                const ChartLayout& layout,
                                std::size_t bar_count,
                                HFONT font,
                                const ThemePalette& pal) noexcept {
    if (bar_count == 0) return;
    const RECT& pb = layout.plot_bounds;
    const int plot_w = pb.right - pb.left;
    if (plot_w <= 0) return;
    wchar_t buf[32]{};
    // Subsample to kTickCount labels so dense categories (12 monthly bars in
    // a 400px plot = 33px per band) don't pile up on top of each other.
    // Mirrors LineChartView::draw_index_axis_ticks_v's behaviour so all
    // four chart views read with the same tick density.
    const std::size_t n = std::min<std::size_t>(bar_count, kTickCount);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (n == 1) ? 0.5
                                 : static_cast<double>(i) /
                                   static_cast<double>(n - 1);
        const int px = pb.left +
                       static_cast<int>(t * static_cast<double>(plot_w) + 0.5);
        std::swprintf(buf, std::size(buf), L"%zu",
                      1 + i * (bar_count - 1) /
                          std::max<std::size_t>(1, n - 1));
        RECT label{px - kTickLabelHalfWidthPx,
                   pb.bottom + 4,
                   px + kTickLabelHalfWidthPx,
                   pb.bottom + kAxisLabelGutter};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

// Column of color swatches + series names on the right of the plot. The
// implementation lives in internal/ChartsPaint.cpp so all four chart views
// (BarChartView / HBarChartView / LineChartView / SplineChartView) render
// the legend identically.

} // namespace

void BarChartView::set_stacked(bool stacked) noexcept {
    stacked_ = stacked;
}

void BarChartView::on_paint(HDC hdc, const RECT& bounds) {
    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);

    fill_rect(hdc, bounds, pal.background);

    const std::size_t series_count = series_.size();
    ChartLayout layout = compute_chart_layout(bounds, ChartKind::bar_vertical, series_count);
    if (layout.plot_bounds.right <= layout.plot_bounds.left ||
        layout.plot_bounds.bottom <= layout.plot_bounds.top) {
        return;
    }

    std::size_t bar_count = 0;
    for (const auto& s : series_) {
        bar_count = std::max(bar_count, s.points.size());
    }
    if (bar_count == 0) {
        // Even on an empty plot, draw the value-axis grid so the chrome
        // (frame + grid) is consistent with a populated chart — the user
        // immediately sees "this is a chart with no data" rather than a
        // blank rectangle.
        const Color grid_color = charts_internal::derive_grid_color(pal);
        charts_internal::draw_grid_hlines(hdc, layout.plot_bounds, grid_color, kTickCount);
        draw_plot_frame(hdc, layout, pal);
        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                            layout.legend_width_px, series_,
                                            palette_, fonts_, dpi);
        return;
    }

    // Slot rects: pass series_count = 1 so each slot uses the full cluster width;
    // multi-series sub-bars subdivide that cluster horizontally below.
    const std::vector<RECT> bands = compute_bar_geometry(layout, 1, bar_count, 0.2);
    if (bands.empty()) {
        return;
    }
    const int plot_h = layout.plot_bounds.bottom - layout.plot_bounds.top;
    const int band_slot_w = bands.front().right - bands.front().left;
    const int sub_w = std::max(1, band_slot_w / static_cast<int>(std::max<std::size_t>(1, series_count)));

    // In stacked mode the per-column sum governs the column's visual extent, so
    // we pre-compute column sums + the global max once and reuse them for the
    // y-axis range (passed to the tick renderer) and for the per-segment heights.
    std::vector<double> col_sums(bar_count, 0.0);
    double max_col_sum = 0.0;
    if (stacked_) {
        for (const auto& s : series_) {
            for (std::size_t i = 0; i < s.points.size() && i < bar_count; ++i) {
                // Negative contributions collapse to 0 so the stack still grows
                // upward from the baseline (no separate "below zero" channel).
                const double v = std::max(0.0, s.points[i].y);
                col_sums[i] += v;
            }
        }
        for (double cs : col_sums) {
            if (cs > max_col_sum) max_col_sum = cs;
        }
    }

    // Inner value-axis grid sits BELOW the plot frame but ABOVE the chart
    // background so it reads as alignment guides without competing with
    // the outer rectangle. CP7 polish pass: prior versions drew only the
    // 4-sided frame which forced readers to interpolate values between
    // ticks; the grid now lands at the same positions as the value-axis
    // tick labels (drawn further down) for one-to-one alignment. Drawn
    // BEFORE the plot frame so the frame's heavier border colour
    // overwrites the grid endpoints that coincide with pb.top /
    // pb.bottom.
    const Color grid_color = charts_internal::derive_grid_color(pal);
    charts_internal::draw_grid_hlines(hdc, layout.plot_bounds, grid_color, kTickCount);

    draw_plot_frame(hdc, layout, pal);

    if (stacked_) {
        // Stack segments vertically inside each band. The y-axis range is
        // [axis_y_.min, eff_max] where eff_max covers the largest column sum
        // so a max-sized stack reaches plot_top and smaller stacks scale
        // proportionally. Each segment occupies (v / (eff_max - axis_y_.min))
        // of plot_h above the previous cursor position.
        double eff_min = axis_y_.min;
        double eff_max = std::max(max_col_sum, axis_y_.max);
        if (!(eff_max > eff_min)) eff_max = eff_min + 1.0;
        const double y_range = eff_max - eff_min;

        for (std::size_t i = 0; i < bar_count; ++i) {
            const RECT& band = bands[i];
            if (col_sums[i] <= 0.0) continue;  // empty column; nothing to stack
            double cursor = eff_min;
            for (std::size_t s = 0; s < series_count; ++s) {
                const ChartSeries& series = series_[s];
                if (i >= series.points.size()) continue;
                const double v = std::max(0.0, series.points[i].y);
                if (v <= 0.0) continue;
                const double v_top = std::min(cursor + v, eff_max);
                // Map value-space [cursor, v_top] -> plot-y (inverted).
                const int plot_y_bot = layout.plot_bounds.bottom -
                    static_cast<int>(((cursor - eff_min) / y_range) *
                                      static_cast<double>(plot_h) + 0.5);
                const int plot_y_top = layout.plot_bounds.bottom -
                    static_cast<int>(((v_top - eff_min) / y_range) *
                                      static_cast<double>(plot_h) + 0.5);
                if (plot_y_top < plot_y_bot) {
                    RECT bar{band.left, plot_y_top, band.right, plot_y_bot};
                    fill_rounded_rect(hdc, bar, theme_metrics().corner_radius_control,
                                      series.color, series.color);
                }
                cursor = v_top;
                if (cursor >= eff_max) break;  // stack saturated; drop later segments
            }
        }
    } else {
        // Grouped: render each series; sub-bars sit side-by-side within each band.
        for (std::size_t s = 0; s < series_count; ++s) {
            const ChartSeries& series = series_[s];
            const int s_off = static_cast<int>(s) * sub_w;
            for (std::size_t i = 0; i < series.points.size() && i < bands.size(); ++i) {
                const ChartPoint& p = series.points[i];
                const RECT& band = bands[i];

                // Screen y is inverted. Clamp the value into the axis range so out-of-
                // range data points still draw rather than disappearing off-canvas.
                const double v = std::clamp(p.y, axis_y_.min, axis_y_.max);
                int bar_top_offset = 0;
                if (axis_y_.max > axis_y_.min) {
                    const double t = (v - axis_y_.min) / (axis_y_.max - axis_y_.min);
                    bar_top_offset = static_cast<int>(t * static_cast<double>(plot_h) + 0.5);
                }
                const int bar_top = layout.plot_bounds.bottom - bar_top_offset;
                const int bar_bottom = layout.plot_bounds.bottom;
                if (bar_top >= bar_bottom) {
                    continue;  // zero-height bar
                }
                RECT bar{band.left + s_off, bar_top, band.left + s_off + sub_w, bar_bottom};
                if (bar.right <= bar.left) {
                    continue;
                }
                fill_rounded_rect(hdc, bar, theme_metrics().corner_radius_control,
                                  series.color, series.color);
            }
        }
    }

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT tick_font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;

    // In stacked mode the tick labels should reflect the column-sum range, not
    // the per-series axis range, so the y-axis reads [axis_y_.min, max_col_sum].
    ChartAxisRange tick_axis_y = axis_y_;
    if (stacked_ && max_col_sum > tick_axis_y.max) {
        tick_axis_y.max = max_col_sum;
    }

    draw_value_axis_ticks_v(hdc, layout, tick_axis_y, tick_font, pal);
    draw_category_axis_ticks_v(hdc, layout, bar_count, tick_font, pal);
    charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                        layout.legend_width_px, series_,
                                        palette_, fonts_, dpi);
}

} // namespace nfui

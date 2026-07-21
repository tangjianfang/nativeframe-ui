#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace nfui {

namespace {

// Tick and legend visual constants. Mono tick label point size mirrors the C2
// default paint (ChartView::draw_default_placeholder uses point 8; we bump to 9
// since real labels carry formatted floats and benefit from a touch more weight).
constexpr int kTickFontPt = 9;
constexpr int kTickCount = 5;
constexpr int kLegendSwatch = 12;
constexpr int kLegendPadX = 8;
constexpr int kLegendPadY = 6;
constexpr int kLegendRowH = 18;
constexpr int kAxisLabelGutter = 18;

// Format one value into a wide-char buffer using printf-style. The axis range's
// label_format is a Rust-style placeholder (e.g. L"{:.1f}"); we ignore it here and
// always emit %.0f so integer tick numbers read cleanly. Single decimal place can
// be reintroduced when callers ask for it via a future axis knob.
void format_tick(wchar_t (&buf)[32], double value) noexcept {
    std::swprintf(buf, std::size(buf), L"%.0f", value);
    buf[std::size(buf) - 1] = L'\0';
}

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
    wchar_t buf[32]{};
    for (int i = 0; i <= kTickCount; ++i) {
        const double t = static_cast<double>(i) / kTickCount;
        const double value = axis_y.min + t * (axis_y.max - axis_y.min);
        const int py = pb.top + static_cast<int>((1.0 - t) * static_cast<double>(plot_h) + 0.5);
        format_tick(buf, value);
        RECT label{pb.left - kAxisLabelGutter - 4, py - 8, pb.left - 4, py + 8};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

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
    wchar_t buf[32]{};
    // Tick per category: 1..N labels under the bars.
    for (std::size_t i = 0; i < bar_count; ++i) {
        const int px = pb.left + static_cast<int>(
            (static_cast<double>(i) + 0.5) * static_cast<double>(plot_w) /
            static_cast<double>(bar_count));
        std::swprintf(buf, std::size(buf), L"%zu", i + 1);
        RECT label{px - 16, pb.bottom + 4, px + 16, pb.bottom + kAxisLabelGutter};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    }
}

// Column of color swatches + series names on the right of the plot.
void draw_legend_column(HDC hdc,
                        const ChartLayout& layout,
                        const RECT& bounds,
                        const std::vector<ChartSeries>& series,
                        HFONT font,
                        const ThemePalette& pal) noexcept {
    if (series.size() < 2 || layout.legend_width_px <= 0) return;

    // Legend column sits to the right of the plot, vertically centered in the
    // band defined by the plot's top/bottom.
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
        draw_plot_frame(hdc, layout, pal);
        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        HFONT font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;
        draw_legend_column(hdc, layout, bounds, series_, font, pal);
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
    draw_legend_column(hdc, layout, bounds, series_, tick_font, pal);
}

} // namespace nfui

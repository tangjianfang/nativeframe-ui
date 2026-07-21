#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <algorithm>
#include <cstdio>
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
    // TODO(c3-stacked): not yet implemented. Flag is exposed so callers can
    // prepare data assuming stacked semantics; subsequent paint pass still
    // renders as a grouped chart until the renderer adds stacking math.
    stacked_ = stacked;
}

void BarChartView::on_paint(HDC hdc, const RECT& bounds) {
    (void)stacked_;  // reserved for the stacking implementation below

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

    draw_plot_frame(hdc, layout, pal);

    // Render each series; sub-bars sit side-by-side within each band.
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

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT tick_font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;

    draw_value_axis_ticks_v(hdc, layout, axis_y_, tick_font, pal);
    draw_category_axis_ticks_v(hdc, layout, bar_count, tick_font, pal);
    draw_legend_column(hdc, layout, bounds, series_, tick_font, pal);
}

} // namespace nfui

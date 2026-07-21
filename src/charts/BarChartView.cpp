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

// Column of color swatches + series names on the right of the plot. The
// implementation lives in internal/ChartsPaint.cpp so all four chart views
// (BarChartView / HBarChartView / LineChartView / SplineChartView) render
// the legend identically.

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
    charts_internal::draw_legend_column(hdc, layout.plot_bounds,
                                        layout.legend_width_px, series_,
                                        palette_, fonts_, dpi);
}

} // namespace nfui

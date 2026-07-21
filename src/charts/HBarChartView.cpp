#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace nfui {

namespace {

// Tick and legend visual constants match BarChartView so the two renders stay
// visually consistent when shown side-by-side.
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

// For horizontal bars the value axis is along x (horizontal) and the category
// axis is along y. Tick labels therefore sit BELOW the plot along x and ALONGSIDE
// the plot along y.
void draw_value_axis_ticks_h(HDC hdc,
                             const ChartLayout& layout,
                             const ChartAxisRange& axis_x_reinterpreted,
                             HFONT font,
                             const ThemePalette& pal) noexcept {
    const RECT& pb = layout.plot_bounds;
    const int plot_w = pb.right - pb.left;
    if (plot_w <= 0) return;
    for (int i = 0; i <= kTickCount; ++i) {
        const double t = static_cast<double>(i) / kTickCount;
        const double value = axis_x_reinterpreted.min +
                             t * (axis_x_reinterpreted.max - axis_x_reinterpreted.min);
        const int px = pb.left + static_cast<int>(t * static_cast<double>(plot_w) + 0.5);
        const std::wstring text = format_axis_tick(value, axis_x_reinterpreted.label_format);
        RECT label{px - 16, pb.bottom + 4, px + 16, pb.bottom + kAxisLabelGutter};
        draw_text(hdc, label, text, font, pal.text_secondary,
                  DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        draw_line(hdc, POINT{px, pb.bottom}, POINT{px, pb.bottom + 3}, pal.border, 1);
    }
}

void draw_category_axis_ticks_h(HDC hdc,
                                const ChartLayout& layout,
                                std::size_t bar_count,
                                HFONT font,
                                const ThemePalette& pal) noexcept {
    if (bar_count == 0) return;
    const RECT& pb = layout.plot_bounds;
    const int plot_h = pb.bottom - pb.top;
    wchar_t buf[32]{};
    for (std::size_t i = 0; i < bar_count; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / static_cast<double>(bar_count);
        const int py = pb.top + static_cast<int>(t * static_cast<double>(plot_h) + 0.5);
        std::swprintf(buf, std::size(buf), L"%zu", i + 1);
        RECT label{pb.left - kAxisLabelGutter - 4, py - 8, pb.left - 4, py + 8};
        draw_text(hdc, label, buf, font, pal.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

void HBarChartView::set_stacked(bool stacked) noexcept {
    // TODO(c3-stacked): same caveat as BarChartView::set_stacked.
    stacked_ = stacked;
}

void HBarChartView::on_paint(HDC hdc, const RECT& bounds) {
    (void)stacked_;  // reserved for the stacking implementation below

    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);

    fill_rect(hdc, bounds, pal.background);

    // compute_chart_layout swaps plot_w/plot_h for bar_horizontal so the plot
    // area is naturally wide as a value axis (x) and tall for categories (y).
    const std::size_t series_count = series_.size();
    ChartLayout layout = compute_chart_layout(bounds, ChartKind::bar_horizontal, series_count);
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

    // Use compute_bar_geometry with series_count = 1 to get the per-category cluster
    // band RECTs in plot space (each occupies the full plot height). We then transpose
    // those bands into horizontal rows + reuse their reported cluster width as a hint
    // for the per-band row allocation below.
    const std::vector<RECT> bands = compute_bar_geometry(layout, 1, bar_count, 0.2);
    if (bands.empty()) {
        return;
    }
    // Sanity: compute_bar_geometry assumes the un-transposed orientation (vertical
    // bands). For HBar we re-derive rows in plot_w/plot_h space directly; the band
    // rects are only used to validate the helper survives a swapped layout.
    (void)bands;

    const int plot_w = layout.plot_bounds.right - layout.plot_bounds.left;
    const int plot_h = layout.plot_bounds.bottom - layout.plot_bounds.top;
    const int row_h = std::max(1, plot_h / static_cast<int>(bar_count));
    const int sub_h = std::max(1, row_h / static_cast<int>(std::max<std::size_t>(1, series_count)));
    // Vertical gap of 1px between rows so adjacent categories do not fuse.
    const int row_gap = std::min(2, std::max(0, row_h / 4));

    draw_plot_frame(hdc, layout, pal);

    for (std::size_t s = 0; s < series_count; ++s) {
        const ChartSeries& series = series_[s];
        for (std::size_t i = 0; i < series.points.size() && i < bar_count; ++i) {
            const ChartPoint& p = series.points[i];
            const int row_top = layout.plot_bounds.top +
                                static_cast<int>(i) * row_h + row_gap / 2;
            const int sub_top = row_top + static_cast<int>(s) * sub_h;
            const int sub_bottom = sub_top + sub_h - row_gap;
            if (sub_bottom <= sub_top) continue;

            // The value axis is x; bars grow rightward from the plot left baseline.
            const double v = std::clamp(p.y, axis_y_.min, axis_y_.max);
            int bar_len = 0;
            if (axis_y_.max > axis_y_.min) {
                const double t = (v - axis_y_.min) / (axis_y_.max - axis_y_.min);
                bar_len = static_cast<int>(t * static_cast<double>(plot_w) + 0.5);
            }
            const int bar_left = layout.plot_bounds.left;
            const int bar_right = bar_left + bar_len;
            if (bar_right <= bar_left) continue;

            RECT bar{bar_left, sub_top, bar_right, sub_bottom};
            fill_rounded_rect(hdc, bar, theme_metrics().corner_radius_control,
                              series.color, series.color);
        }
    }

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT tick_font = (fonts_ != nullptr) ? fonts_->mono(dpi, kTickFontPt) : nullptr;

    draw_value_axis_ticks_h(hdc, layout, axis_y_, tick_font, pal);
    draw_category_axis_ticks_h(hdc, layout, bar_count, tick_font, pal);
    draw_legend_column(hdc, layout, bounds, series_, tick_font, pal);
}

} // namespace nfui

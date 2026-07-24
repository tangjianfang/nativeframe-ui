// Shared legend-column drawing helper for the 4 chart views. Lifted from
// BarChartView / HBarChartView / LineChartView / SplineChartView so all four
// render the legend identically: a surface-coloured column with a 1px border,
// one rounded swatch and one series-name label per row, vertically centered
// in the plot band. Public callers go through the views; this file is
// internal to nfui_charts.

#include "ChartsPaint.hpp"

#include <nfui/Paint.hpp>

namespace nfui::charts_internal {

namespace {

// Column visual constants. Kept here (not in the header) so the legend
// shape can evolve without breaking source compatibility. They mirror the
// values the four views used to bake in themselves; if you change a number
// here, the four view outputs shift together. The point size reuses the
// shared font_pt::chart_legend token so the legend matches the UI body
// text everywhere else; bump that token to bump the legend.
constexpr int kLegendSwatch = 12;
constexpr int kLegendPadX = 8;
constexpr int kLegendPadY = 6;
constexpr int kLegendRowH = 18;
// Use Segoe UI (regular) for the series-name labels: these are prose
// strings like "Revenue" or "Monthly revenue (USDk)", not numeric data.
// Mono is reserved for the actual numeric tick labels so the two columns
// read with the typographic distinction the rest of the chrome uses.
constexpr bool kLegendUseMonoFont = false;
// Spacing between the plot's right edge and the legend column. The old
// per-view helpers passed `bounds` and computed this implicitly; we use
// `plot_bounds.right` directly here.
constexpr int kLegendGapPx = 8;
// Blend weight used by derive_grid_color() to push border toward
// text_secondary. 0.5 = halfway. Held here so the four views stay in sync
// if the value ever needs to shift.
constexpr float kGridBlend = 0.65f;
// Blend weight used by derive_legend_frame_color(). Heavier than
// kGridBlend so the legend column reads as a discrete container rather
// than a subtle alignment guide — the legend's outer edge is the only
// thing telling the reader where the series swatches end and the chart
// background begins, so it gets to be more emphatic than the inner grid.
constexpr float kLegendFrameBlend = 0.7f;

} // namespace

Color derive_grid_color(const ThemePalette& pal) noexcept {
    // alpha_blend(src=border, dst=text_secondary, alpha=0.5) returns the
    // midpoint of the two tones, which keeps the grid readable in both
    // light and dark themes without becoming as dominant as the outer
    // plot frame (which still uses raw `border`). Centralized here so
    // every chart view reads the same line weight.
    return alpha_blend(pal.border, pal.text_secondary, kGridBlend);
}

Color derive_legend_frame_color(const ThemePalette& pal) noexcept {
    // The legend column's frame is the only thing separating its surface
    // fill from the chart background in light theme (where both are
    // near-white beige tones). Push border toward text_secondary at 0.7
    // weight so the frame lands around WCAG 3:1 contrast in light and
    // well above in dark / HC themes.
    return alpha_blend(pal.border, pal.text_secondary, kLegendFrameBlend);
}

void draw_grid_hlines(HDC hdc,
                      const RECT& plot_bounds,
                      Color grid_color,
                      int value_tick_count) noexcept {
    if (hdc == nullptr) return;
    if (value_tick_count < 2) return;
    const int plot_h = plot_bounds.bottom - plot_bounds.top;
    if (plot_h <= 0) return;
    // Anchor first/last lines on the plot edges so the value axis ticks
    // land on a real grid line (matches what the user expects from a
    // axis-bound grid: top tick == top edge, bottom tick == bottom edge).
    for (int i = 0; i < value_tick_count; ++i) {
        const double t = static_cast<double>(i) /
                         static_cast<double>(value_tick_count - 1);
        const int py = plot_bounds.top +
                       static_cast<int>(t * static_cast<double>(plot_h) + 0.5);
        // Clip the endpoints to the plot's horizontal extent so a future
        // multi-panel layout doesn't draw grid lines into the legend
        // gutter by accident.
        draw_line(hdc,
                  POINT{plot_bounds.left, py},
                  POINT{plot_bounds.right, py},
                  grid_color,
                  1);
    }
}

void draw_grid_vlines(HDC hdc,
                      const RECT& plot_bounds,
                      Color grid_color,
                      int value_tick_count) noexcept {
    if (hdc == nullptr) return;
    if (value_tick_count < 2) return;
    const int plot_w = plot_bounds.right - plot_bounds.left;
    if (plot_w <= 0) return;
    for (int i = 0; i < value_tick_count; ++i) {
        const double t = static_cast<double>(i) /
                         static_cast<double>(value_tick_count - 1);
        const int px = plot_bounds.left +
                       static_cast<int>(t * static_cast<double>(plot_w) + 0.5);
        draw_line(hdc,
                  POINT{px, plot_bounds.top},
                  POINT{px, plot_bounds.bottom},
                  grid_color,
                  1);
    }
}

void draw_legend_column(HDC hdc,
                        const RECT& plot_bounds,
                        int legend_width_px,
                        const std::vector<ChartSeries>& series,
                        const ThemePalette* palette,
                        FontCache* fonts,
                        int dpi) noexcept {
    if (hdc == nullptr) return;
    if (series.size() < 2 || legend_width_px <= 0) return;

    // Resolve palette + font with graceful fallback so this helper mirrors
    // the in-view fallback behaviour the four subclasses used to do inline.
    const ThemePalette& pal =
        palette != nullptr ? *palette : theme_palette(ThemeMode::light);
    HFONT font = (fonts != nullptr)
        ? (kLegendUseMonoFont
            ? fonts->mono(dpi, font_pt::chart_legend)
            : fonts->regular(dpi, font_pt::chart_legend))
        : nullptr;

    const int col_w = legend_width_px;
    const int col_left = plot_bounds.right + kLegendGapPx;
    const int col_right = col_left + col_w - kLegendPadX;
    const int col_h = static_cast<int>(series.size()) * kLegendRowH + 2 * kLegendPadY;
    const int col_top = plot_bounds.top +
                        ((plot_bounds.bottom - plot_bounds.top) - col_h) / 2;
    const int col_bottom = col_top + col_h;

    RECT col{col_left, col_top, col_right, col_bottom};
    fill_rect(hdc, col, pal.surface);
    // Frame is derived (border blended toward text_secondary) so the
    // legend reads as a clear container against the chart background in
    // every theme; raw `pal.border` is too close to `pal.surface` in the
    // light theme for the column edges to be visible.
    const Color frame_color = derive_legend_frame_color(pal);
    draw_line(hdc, POINT{col.left, col.top}, POINT{col.right, col.top}, frame_color, 1);
    draw_line(hdc, POINT{col.left, col.bottom}, POINT{col.right, col.bottom}, frame_color, 1);
    draw_line(hdc, POINT{col.left, col.top}, POINT{col.left, col.bottom}, frame_color, 1);
    draw_line(hdc, POINT{col.right, col.top}, POINT{col.right, col.bottom}, frame_color, 1);

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

} // namespace nfui::charts_internal

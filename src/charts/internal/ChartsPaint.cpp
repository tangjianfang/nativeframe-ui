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
// here, the four view outputs shift together.
constexpr int kLegendFontPt = 9;
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

} // namespace

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
            ? fonts->mono(dpi, kLegendFontPt)
            : fonts->regular(dpi, kLegendFontPt))
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

} // namespace nfui::charts_internal

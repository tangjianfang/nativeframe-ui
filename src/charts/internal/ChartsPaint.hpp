#pragma once

// Internal AA-aware primitives for the chart line + spline renderers plus the
// shared legend-column helper that all four chart views (bar, hbar, line,
// spline) draw identically. None of this is part of the nfui public surface —
// only nfui_charts itself calls into these. The AA primitives fall back to
// pure GDI when the GDI+ runtime isn't active, so the renderer always paints.

#include <nfui/Charts.hpp>

#include <windows.h>

namespace nfui::charts_internal {

// Draws a multi-point polyline (count >= 2). When GDI+ is active the stroke
// is antialiased; otherwise falls back to the existing nfui::draw_polyline.
void draw_polyline_aa(HDC hdc,
                      const POINT* points,
                      int count,
                      Color color,
                      int width_px) noexcept;

// Draws a chained cubic Bezier spline. The points array follows the same
// shape as catmull_rom_to_bezier's output: 4*(segments) points, where
// consecutive segments share their endpoint.
void draw_beziers_aa(HDC hdc,
                     const POINT* points,
                     int count,
                     Color color,
                     int width_px) noexcept;

// Fills `count` circles of `radius_px` at the supplied screen centers with
// the same solid color. Used for the line chart marker dots so they read
// as crisp circles instead of aliased octagons.
void fill_circles_aa(HDC hdc,
                     const POINT* centers,
                     int count,
                     int radius_px,
                     Color color) noexcept;

// Draws a vertical legend column (color swatch + series name per row) on the
// right side of the chart plot. Lifted from the four ChartView subclasses
// so bar / hbar / line / spline read consistently when shown side-by-side.
// Falls back gracefully when `palette` or `fonts` is null.
void draw_legend_column(HDC hdc,
                        const RECT& plot_bounds,
                        int legend_width_px,
                        const std::vector<ChartSeries>& series,
                        const ThemePalette* palette,
                        FontCache* fonts,
                        int dpi) noexcept;

} // namespace nfui::charts_internal

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

// Draws horizontal grid lines (parallel to the x axis) inside `plot_bounds`
// at `value_tick_count + 1` evenly spaced positions (i == 0 anchors on the
// top edge, i == value_tick_count anchors on the bottom edge). Used by the
// vertical-bar / line / spline views to help readers align data points to
// the value axis. `grid_color` should be derived by the caller from the
// palette (the helper does NOT recompute it) so the value-axis grid reads
// in lockstep with the outer plot frame.
void draw_grid_hlines(HDC hdc,
                      const RECT& plot_bounds,
                      Color grid_color,
                      int value_tick_count) noexcept;

// Draws vertical grid lines (parallel to the y axis) inside `plot_bounds`
// at `value_tick_count + 1` evenly spaced positions. Used by the
// horizontal-bar view so bar lengths can be aligned to the value axis.
void draw_grid_vlines(HDC hdc,
                      const RECT& plot_bounds,
                      Color grid_color,
                      int value_tick_count) noexcept;

// Derives a grid color from a palette by blending `border` toward
// `text_secondary` at a fixed weight. Result is symmetric across themes:
// in light mode the grid sits between the muted border and the readable
// text_secondary so the chart frame remains the dominant edge, while in
// dark / HC mode the grid is still lighter than the background and stays
// visible against the panel. Exposed so all four views pick the same
// value without duplicating the blend factor.
[[nodiscard]] Color derive_grid_color(const ThemePalette& pal) noexcept;

// Derives the legend-column frame color. Uses a heavier blend weight
// than the inner grid so the legend reads as a discrete container
// against the chart background even in light theme (where raw border
// vs background contrast is ~1.3:1 and the surface fill itself is
// only ~1.2:1 against the panel). Symmetric across themes for the
// same reason as derive_grid_color.
[[nodiscard]] Color derive_legend_frame_color(const ThemePalette& pal) noexcept;

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

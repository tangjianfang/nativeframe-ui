#pragma once

// Internal AA-aware primitives for the chart line + spline renderers.
// These are NOT part of the nfui public surface — only nfui_charts itself
// needs them. Each function falls back to the equivalent GDI primitive
// when the GDI+ runtime is not active, so the renderer always paints.

#include <nfui/Theme.hpp>

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

} // namespace nfui::charts_internal

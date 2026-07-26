#pragma once

// Pure hit-testing and coordinate-conversion functions for the chart
// interaction system. All functions are HWND-free and unit-testable.
// Internal to nfui_charts — not part of the public API surface.

#include <nfui/ChartInteraction.hpp>
#include <nfui/Charts.hpp>

#include <cstddef>
#include <vector>

#include <windows.h>

namespace nfui::charts_internal {

// Converts a pixel position (relative to the chart client area) into
// data-space coordinates using the current layout and axis ranges.
[[nodiscard]] ChartPoint pixel_to_data(POINT px,
                                       const ChartLayout& layout,
                                       const ChartAxisRange& axis_x,
                                       const ChartAxisRange& axis_y) noexcept;

// Converts a data-space point into pixel coordinates (client-relative).
[[nodiscard]] POINT data_to_pixel(ChartPoint data,
                                  const ChartLayout& layout,
                                  const ChartAxisRange& axis_x,
                                  const ChartAxisRange& axis_y) noexcept;

// Finds the nearest data point within `tolerance_px` of the cursor.
// Searches all series; returns the closest match or hit==false.
[[nodiscard]] ChartHitResult hit_test_point(
    POINT cursor,
    const std::vector<ChartSeries>& series,
    const ChartLayout& layout,
    const ChartAxisRange& axis_x,
    const ChartAxisRange& axis_y,
    int tolerance_px) noexcept;

// Tests whether a pixel point lies inside the plot area.
[[nodiscard]] bool point_in_plot(POINT px, const ChartLayout& layout) noexcept;

// Clamps a data-space value to the axis range [min, max].
[[nodiscard]] double clamp_to_axis(double value, const ChartAxisRange& axis) noexcept;

// Computes a zoomed axis range centered on `center` with the given factor.
// factor > 1 zooms out; factor < 1 zooms in.
[[nodiscard]] ChartAxisRange zoom_axis(const ChartAxisRange& axis,
                                       double center,
                                       double factor) noexcept;

// Computes a panned axis range shifted by `delta` in data units.
[[nodiscard]] ChartAxisRange pan_axis(const ChartAxisRange& axis,
                                      double delta) noexcept;

} // namespace nfui::charts_internal

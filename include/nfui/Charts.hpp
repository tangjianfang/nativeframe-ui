#pragma once

#include <nfui/Theme.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include <windows.h>

namespace nfui {

enum class ChartKind {
    bar_vertical,
    bar_horizontal,
    line,
    spline,
};

struct ChartPoint {
    double x{};
    double y{};
};

struct ChartSeries {
    std::wstring_view name;       // borrowed; non-owning
    Color color{};
    std::vector<ChartPoint> points;
};

struct ChartAxisRange {
    double min{};
    double max{};
    std::wstring_view label_format = L"{:.1f}";  // printf-style placeholders
};

struct ChartLayout {
    RECT plot_bounds{};       // pixel coords, filled by compute_chart_layout
    ChartAxisRange x{};
    ChartAxisRange y{};
    int legend_width_px = 120; // reserved for legend column
};

// Pure helpers (HWND-free, unit-testable):
[[nodiscard]] ChartLayout compute_chart_layout(RECT content_bounds,
                                              ChartKind kind,
                                              std::size_t series_count) noexcept;

[[nodiscard]] std::vector<POINT> normalize_points(const std::vector<ChartPoint>& points,
                                                  const ChartLayout& layout,
                                                  const ChartAxisRange& x,
                                                  const ChartAxisRange& y) noexcept;

[[nodiscard]] std::vector<RECT> compute_bar_geometry(const ChartLayout& layout,
                                                    std::size_t series_count,
                                                    std::size_t bar_count,
                                                    double gap_ratio = 0.2) noexcept;

// Catmull-Rom -> cubic Bezier control points for spline rendering.
// Returns 4*(n-1) POINTs (cubic segments share endpoints with neighbors).
[[nodiscard]] std::vector<POINT> catmull_rom_to_bezier(const std::vector<POINT>& points,
                                                      double tension = 0.5) noexcept;

} // namespace nfui

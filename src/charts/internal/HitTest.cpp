#include "HitTest.hpp"

#include <algorithm>
#include <cmath>

namespace nfui::charts_internal {

ChartPoint pixel_to_data(POINT px,
                         const ChartLayout& layout,
                         const ChartAxisRange& axis_x,
                         const ChartAxisRange& axis_y) noexcept {
    const RECT& pb = layout.plot_bounds;
    const int plot_w = pb.right - pb.left;
    const int plot_h = pb.bottom - pb.top;

    ChartPoint result{};
    if (plot_w <= 0 || plot_h <= 0) {
        return result;
    }

    // x: left edge == axis_x.min, right edge == axis_x.max
    const double tx = static_cast<double>(px.x - pb.left) / static_cast<double>(plot_w);
    result.x = axis_x.min + tx * (axis_x.max - axis_x.min);

    // y: top edge == axis_y.max, bottom edge == axis_y.min (screen y inverted)
    const double ty = static_cast<double>(px.y - pb.top) / static_cast<double>(plot_h);
    result.y = axis_y.max - ty * (axis_y.max - axis_y.min);

    return result;
}

POINT data_to_pixel(ChartPoint data,
                    const ChartLayout& layout,
                    const ChartAxisRange& axis_x,
                    const ChartAxisRange& axis_y) noexcept {
    const RECT& pb = layout.plot_bounds;
    const int plot_w = pb.right - pb.left;
    const int plot_h = pb.bottom - pb.top;

    if (plot_w <= 0 || plot_h <= 0) {
        return POINT{pb.left, pb.top};
    }

    const double range_x = axis_x.max - axis_x.min;
    const double range_y = axis_y.max - axis_y.min;

    LONG px_x = pb.left;
    LONG px_y = pb.top;

    if (range_x > 0.0) {
        const double tx = (data.x - axis_x.min) / range_x;
        px_x = pb.left + static_cast<LONG>(tx * static_cast<double>(plot_w) + 0.5);
    }
    if (range_y > 0.0) {
        const double ty = (data.y - axis_y.min) / range_y;
        // Screen y is inverted: high data values are at the top.
        px_y = pb.bottom - static_cast<LONG>(ty * static_cast<double>(plot_h) + 0.5);
    }

    return POINT{px_x, px_y};
}

ChartHitResult hit_test_point(POINT cursor,
                              const std::vector<ChartSeries>& series,
                              const ChartLayout& layout,
                              const ChartAxisRange& axis_x,
                              const ChartAxisRange& axis_y,
                              int tolerance_px) noexcept {
    ChartHitResult best{};
    best.hit = false;
    best.distance_px = static_cast<double>(tolerance_px) + 1.0;

    const double tol = static_cast<double>(tolerance_px);

    for (std::size_t si = 0; si < series.size(); ++si) {
        if (!series[si].visible) continue;  // CP39: invisible series are unselectable
        const auto& pts = series[si].points;
        for (std::size_t pi = 0; pi < pts.size(); ++pi) {
            const POINT px = data_to_pixel(pts[pi], layout, axis_x, axis_y);
            const double dx = static_cast<double>(cursor.x - px.x);
            const double dy = static_cast<double>(cursor.y - px.y);
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= tol && dist < best.distance_px) {
                best.hit = true;
                best.series_index = si;
                best.point_index = pi;
                best.data_point = pts[pi];
                best.pixel_point = px;
                best.distance_px = dist;
            }
        }
    }

    if (!best.hit) {
        best.distance_px = 0.0;
    }
    return best;
}

bool point_in_plot(POINT px, const ChartLayout& layout) noexcept {
    const RECT& pb = layout.plot_bounds;
    return px.x >= pb.left && px.x <= pb.right &&
           px.y >= pb.top && px.y <= pb.bottom;
}

double clamp_to_axis(double value, const ChartAxisRange& axis) noexcept {
    if (value < axis.min) return axis.min;
    if (value > axis.max) return axis.max;
    return value;
}

ChartAxisRange zoom_axis(const ChartAxisRange& axis,
                         double center,
                         double factor) noexcept {
    // Clamp factor to prevent degenerate or inverted ranges.
    if (factor < 0.01) factor = 0.01;
    if (factor > 100.0) factor = 100.0;

    const double new_min = center - (center - axis.min) * factor;
    const double new_max = center + (axis.max - center) * factor;

    ChartAxisRange result = axis;
    result.min = new_min;
    result.max = new_max;

    // Guard against degenerate range.
    if (result.max - result.min < 1e-9) {
        result.max = result.min + 1e-9;
    }
    return result;
}

ChartAxisRange pan_axis(const ChartAxisRange& axis, double delta) noexcept {
    ChartAxisRange result = axis;
    result.min += delta;
    result.max += delta;
    return result;
}

} // namespace nfui::charts_internal

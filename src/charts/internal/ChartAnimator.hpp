#pragma once

// Internal animation state for chart data transitions. Interpolates between
// two sets of series points over a duration using the project's Easing curves.
// HWND-free; driven by WM_TIMER in ChartView.

#include <nfui/Charts.hpp>
#include <nfui/Clock.hpp>
#include <nfui/Easing.hpp>

#include <vector>

namespace nfui::charts_internal {

// Animates axis range transitions (zoom/pan).
struct AxisAnimation {
    ChartAxisRange from_x{};
    ChartAxisRange to_x{};
    ChartAxisRange from_y{};
    ChartAxisRange to_y{};
    unsigned long long start_ms{0};
    unsigned int duration_ms{0};
    bool active{false};

    void begin(ChartAxisRange fx, ChartAxisRange tx,
               ChartAxisRange fy, ChartAxisRange ty,
               unsigned long long now, unsigned int duration) noexcept {
        from_x = fx; to_x = tx;
        from_y = fy; to_y = ty;
        start_ms = now;
        duration_ms = (duration == 0) ? 1 : duration;
        active = true;
    }

    // Returns the interpolated axis ranges at time `now`.
    void sample(unsigned long long now,
                ChartAxisRange& out_x,
                ChartAxisRange& out_y) noexcept {
        if (!active) {
            out_x = to_x;
            out_y = to_y;
            return;
        }
        const unsigned long long elapsed = now - start_ms;
        if (elapsed >= duration_ms) {
            active = false;
            out_x = to_x;
            out_y = to_y;
            return;
        }
        const float t = ease_out_cubic(
            static_cast<float>(elapsed) / static_cast<float>(duration_ms));

        out_x = to_x;
        out_x.min = from_x.min + (to_x.min - from_x.min) * static_cast<double>(t);
        out_x.max = from_x.max + (to_x.max - from_x.max) * static_cast<double>(t);

        out_y = to_y;
        out_y.min = from_y.min + (to_y.min - from_y.min) * static_cast<double>(t);
        out_y.max = from_y.max + (to_y.max - from_y.max) * static_cast<double>(t);
    }

    void cancel() noexcept { active = false; }
};

} // namespace nfui::charts_internal

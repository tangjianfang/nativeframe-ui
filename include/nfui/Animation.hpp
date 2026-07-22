#pragma once

#include <nfui/Easing.hpp>
#include <nfui/Theme.hpp>   // Color, lerp_color (theme layer; avoids paint dep)

namespace nfui {

// CP17: a pure color cross-fade state machine. begin() stamps the start time
// and the from/to endpoints; sample() returns the interpolated color at a
// caller-supplied `now` and clears `active` once the duration has elapsed.
// No HWND, no timer — the OS-timer driver in Control::subclass_proc calls
// sample() on each tick; tests call it directly with a fake clock.
//
// `ease` is passed per sample so a single state machine can be reused with
// different curves (hover = ease_out_cubic, theme = ease_in_out_cubic).

struct ColorAnimation {
    Color from{};
    Color to{};
    unsigned long long start_ms{0};
    unsigned int duration_ms{0};
    bool active{false};

    void begin(Color from_color, Color to_color,
               unsigned long long now, unsigned int duration) noexcept {
        from = from_color;
        to = to_color;
        start_ms = now;
        duration_ms = duration == 0 ? 1 : duration;
        active = true;
    }

    // Returns the eased color at `now`. When the duration has fully elapsed the
    // machine snaps to `to` and deactivates, so the final paint is exact.
    [[nodiscard]] Color sample(unsigned long long now, float (&ease)(float)) noexcept {
        if (!active) return to;
        const unsigned long long elapsed = now - start_ms;
        if (elapsed >= duration_ms) {
            active = false;
            return to;
        }
        const float t = static_cast<float>(elapsed) / static_cast<float>(duration_ms);
        return lerp_color(from, to, ease(t));
    }

    [[nodiscard]] bool is_active() const noexcept { return active; }
    void cancel() noexcept { active = false; }
};

} // namespace nfui
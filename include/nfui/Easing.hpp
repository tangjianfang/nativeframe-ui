#pragma once

// CP17: pure easing curves. All functions clamp t to [0,1] and are noexcept /
// stateless so they are fully unit-testable without an HWND and safe to call
// from the noexcept subclass_proc paint path. Used by ColorAnimation and any
// leaf that interpolates a value over a duration.

namespace nfui {

[[nodiscard]] inline float clamp01(float t) noexcept {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

// Linear. Rarely the right choice for UI (feels mechanical) but useful as a
// baseline and for non-visual interpolations.
[[nodiscard]] inline float ease_linear(float t) noexcept {
    return clamp01(t);
}

// ease-out cubic: fast in, slow out. The default for hover/enter transitions
// where the element should "settle" into its resting state.
[[nodiscard]] inline float ease_out_cubic(float t) noexcept {
    t = clamp01(t);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// ease-in-out cubic: slow at both ends. The default for cross-fades between two
// steady states (e.g. a theme switch) so the transition has no visible
// "launch" or "landing" pop.
[[nodiscard]] inline float ease_in_out_cubic(float t) noexcept {
    t = clamp01(t);
    return t < 0.5f
        ? 4.0f * t * t * t
        : 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
}

// ease-out quint: snappier finish than cubic, for small elements that should
// feel responsive (press release, toggle snap).
[[nodiscard]] inline float ease_out_quint(float t) noexcept {
    t = clamp01(t);
    const float u = 1.0f - t;
    return 1.0f - u * u * u * u * u;
}

} // namespace nfui
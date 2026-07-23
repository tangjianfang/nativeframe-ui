#pragma once

#include <windows.h>

#include <array>
#include <cstddef>

namespace nfui {

enum class ThemeMode {
    system,
    light,
    dark,
    high_contrast,
};

struct Color {
    COLORREF rgb{};
};

// CP17: straight per-channel lerp between two colors (a at t=0, b at t=1).
// Lives with Color (theme layer) rather than in Paint so the theme layer can
// cross-fade palettes without depending upward on paint. t is clamped to
// [0,1] so callers can pass a raw eased value. Distinct from paint's
// alpha_blend, which composites src over dst.
[[nodiscard]] Color lerp_color(Color a, Color b, float t) noexcept;

struct ThemeTokens {           // back-compat, lighter view
    COLORREF window_background{};
    COLORREF window_text{};
    COLORREF accent{};
};

struct ThemePalette {
    Color background;          // window chrome
    Color surface;             // cards / panels
    Color surface_hover;
    Color border;
    Color text;
    Color text_secondary;
    Color accent;
    Color accent_hover;
    Color accent_text;         // text drawn on accent
    Color selection;
    Color selection_text;
    Color danger;
    Color success;
    Color warning;
    Color info;                // CP31: semantic blue/cyan, distinct from brand accent
    Color shadow;              // CP16: tint used by paint_drop_shadow; alpha is overridden by the helper
};

struct ThemeMetrics {
    int corner_radius_control{6};   // buttons, inputs
    int corner_radius_card{10};      // panels, cards
    int border_width{1};
    int spacing{8};                  // base spacing unit
    int row_height{28};              // list row baseline
};

[[nodiscard]] ThemeTokens  theme_tokens(ThemeMode mode) noexcept;   // back-compat
[[nodiscard]] ThemePalette theme_palette(ThemeMode mode) noexcept;
[[nodiscard]] ThemeMetrics theme_metrics() noexcept;

// CP31: categorical palette for data visualization. Returns 8 colorblind-safe
// hues that are intentionally independent of the brand accent so series in
// charts do not collide with UI emphasis. Wraps modulo count for any index.
[[nodiscard]] const std::array<Color, 8>& chart_series_palette(ThemeMode mode) noexcept;
[[nodiscard]] Color chart_series_color(ThemeMode mode, std::size_t index) noexcept;

// True when the palette behaves like the WCAG high-contrast profile:
// a near-black background paired with a near-white foreground and a
// bright, saturated accent. Callers use this to switch control paint
// formulas (Button pressed/disabled, etc.) into paths that satisfy the
// 3:1 UI-component and 7:1 text thresholds that the standard light/dark
// formulas cannot reach against extreme accents.
[[nodiscard]] bool is_high_contrast(const ThemePalette& p) noexcept;

// CP17: per-field lerp between two palettes (a at t=0, b at t=1). Used by
// samples to cross-fade a theme switch over ~200 ms: each tick builds an
// interpolated palette and hands it to Control::set_palette. Radius/spacing
// (ThemeMetrics) do not change across themes, so only colors interpolate.
[[nodiscard]] ThemePalette lerp_palette(const ThemePalette& a, const ThemePalette& b, float t) noexcept;

} // namespace nfui

#pragma once

#include <windows.h>

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

// True when the palette behaves like the WCAG high-contrast profile:
// a near-black background paired with a near-white foreground and a
// bright, saturated accent. Callers use this to switch control paint
// formulas (Button pressed/disabled, etc.) into paths that satisfy the
// 3:1 UI-component and 7:1 text thresholds that the standard light/dark
// formulas cannot reach against extreme accents.
[[nodiscard]] bool is_high_contrast(const ThemePalette& p) noexcept;

} // namespace nfui

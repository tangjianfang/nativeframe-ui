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
};

[[nodiscard]] ThemeTokens  theme_tokens(ThemeMode mode) noexcept;   // back-compat
[[nodiscard]] ThemePalette theme_palette(ThemeMode mode) noexcept;

} // namespace nfui

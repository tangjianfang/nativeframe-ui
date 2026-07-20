#include <nfui/Theme.hpp>

namespace nfui {

namespace {
Color to_color(COLORREF c) noexcept { return Color{c}; }
}

ThemePalette theme_palette(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemePalette{
            to_color(RGB(24, 24, 28)),   to_color(RGB(36, 36, 42)),   to_color(RGB(48, 48, 56)),
            to_color(RGB(64, 64, 72)),   to_color(RGB(240, 240, 245)), to_color(RGB(160, 160, 170)),
            to_color(RGB(96, 165, 250)), to_color(RGB(125, 185, 255)), to_color(RGB(255, 255, 255)),
            to_color(RGB(40, 80, 140)),  to_color(RGB(235, 240, 255)), to_color(RGB(239, 68, 68)),
            to_color(RGB(34, 197, 94)),  to_color(RGB(234, 179, 8)),
        };
    case ThemeMode::high_contrast:
        return ThemePalette{
            to_color(RGB(0, 0, 0)),      to_color(RGB(0, 0, 0)),       to_color(RGB(40, 40, 40)),
            to_color(RGB(255, 255, 255)),to_color(RGB(255, 255, 255)), to_color(RGB(200, 200, 200)),
            to_color(RGB(255, 235, 59)), to_color(RGB(255, 235, 59)),  to_color(RGB(0, 0, 0)),
            to_color(RGB(255, 235, 59)), to_color(RGB(0, 0, 0)),       to_color(RGB(255, 0, 0)),
            to_color(RGB(0, 255, 0)),    to_color(RGB(255, 235, 59)),
        };
    case ThemeMode::system:
    case ThemeMode::light:
    default:
        return ThemePalette{
            to_color(RGB(255, 255, 255)),to_color(RGB(247, 248, 250)), to_color(RGB(238, 240, 244)),
            to_color(RGB(214, 217, 222)), to_color(RGB(24, 24, 28)),   to_color(RGB(96, 100, 110)),
            to_color(RGB(0, 110, 215)),  to_color(RGB(0, 90, 180)),    to_color(RGB(255, 255, 255)),
            to_color(RGB(220, 232, 250)), to_color(RGB(24, 24, 28)),   to_color(RGB(200, 40, 40)),
            to_color(RGB(16, 124, 80)),  to_color(RGB(176, 130, 0)),
        };
    }
}

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    const ThemePalette p = theme_palette(mode);
    return ThemeTokens{p.background.rgb, p.text.rgb, p.accent.rgb};
}

} // namespace nfui

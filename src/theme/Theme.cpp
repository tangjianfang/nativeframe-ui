#include <nfui/Theme.hpp>

namespace nfui {

namespace {
Color rgb(COLORREF c) noexcept { return Color{c}; }
}

ThemePalette theme_palette(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemePalette{
            rgb(RGB(24, 24, 28)),   rgb(RGB(36, 36, 42)),   rgb(RGB(48, 48, 56)),
            rgb(RGB(64, 64, 72)),   rgb(RGB(240, 240, 245)), rgb(RGB(160, 160, 170)),
            rgb(RGB(96, 165, 250)), rgb(RGB(125, 185, 255)), rgb(RGB(255, 255, 255)),
            rgb(RGB(40, 80, 140)),  rgb(RGB(235, 240, 255)), rgb(RGB(239, 68, 68)),
            rgb(RGB(34, 197, 94)),  rgb(RGB(234, 179, 8)),
        };
    case ThemeMode::high_contrast:
        return ThemePalette{
            rgb(RGB(0, 0, 0)),      rgb(RGB(0, 0, 0)),       rgb(RGB(40, 40, 40)),
            rgb(RGB(255, 255, 255)),rgb(RGB(255, 255, 255)), rgb(RGB(200, 200, 200)),
            rgb(RGB(255, 235, 59)), rgb(RGB(255, 235, 59)),  rgb(RGB(0, 0, 0)),
            rgb(RGB(255, 235, 59)), rgb(RGB(0, 0, 0)),       rgb(RGB(255, 0, 0)),
            rgb(RGB(0, 255, 0)),    rgb(RGB(255, 235, 59)),
        };
    case ThemeMode::system:
    case ThemeMode::light:
    default:
        return ThemePalette{
            rgb(RGB(255, 255, 255)),rgb(RGB(247, 248, 250)), rgb(RGB(238, 240, 244)),
            rgb(RGB(214, 217, 222)), rgb(RGB(24, 24, 28)),   rgb(RGB(96, 100, 110)),
            rgb(RGB(0, 110, 215)),  rgb(RGB(0, 90, 180)),    rgb(RGB(255, 255, 255)),
            rgb(RGB(220, 232, 250)), rgb(RGB(24, 24, 28)),   rgb(RGB(200, 40, 40)),
            rgb(RGB(16, 124, 80)),  rgb(RGB(176, 130, 0)),
        };
    }
}

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    const ThemePalette p = theme_palette(mode);
    return ThemeTokens{p.background.rgb, p.text.rgb, p.accent.rgb};
}

} // namespace nfui

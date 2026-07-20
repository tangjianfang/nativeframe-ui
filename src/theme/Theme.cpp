#include <nfui/Theme.hpp>

namespace nfui {

namespace {
Color to_color(COLORREF c) noexcept { return Color{c}; }
}

ThemePalette theme_palette(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemePalette{
            to_color(RGB(31, 30, 29)),    to_color(RGB(42, 41, 39)),    to_color(RGB(53, 52, 47)),
            to_color(RGB(61, 60, 54)),    to_color(RGB(237, 237, 235)), to_color(RGB(168, 163, 154)),
            to_color(RGB(217, 119, 87)),  to_color(RGB(232, 148, 120)), to_color(RGB(255, 255, 255)),
            to_color(RGB(58, 46, 38)),   to_color(RGB(237, 237, 235)), to_color(RGB(229, 115, 111)),
            to_color(RGB(111, 170, 130)), to_color(RGB(217, 162, 58)),
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
            to_color(RGB(250, 249, 245)), to_color(RGB(240, 238, 230)), to_color(RGB(232, 229, 219)),
            to_color(RGB(219, 215, 204)), to_color(RGB(31, 30, 29)),    to_color(RGB(107, 104, 98)),
            to_color(RGB(217, 119, 87)),  to_color(RGB(193, 95, 63)),   to_color(RGB(255, 255, 255)),
            to_color(RGB(242, 214, 200)), to_color(RGB(31, 30, 29)),    to_color(RGB(199, 84, 80)),
            to_color(RGB(77, 124, 95)),   to_color(RGB(184, 130, 31)),
        };
    }
}

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    const ThemePalette p = theme_palette(mode);
    return ThemeTokens{p.background.rgb, p.text.rgb, p.accent.rgb};
}

ThemeMetrics theme_metrics() noexcept {
    return ThemeMetrics{};
}

} // namespace nfui

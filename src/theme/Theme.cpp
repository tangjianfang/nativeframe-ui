#include <nfui/Theme.hpp>

#include <cmath>

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
            to_color(RGB(0, 0, 0)),       // CP16: shadow tint (alpha baked by helper)
        };
    case ThemeMode::high_contrast:
        // WCAG AAA high-contrast profile. All foreground/background pairs
        // clear 7:1 text contrast except where noted below; UI components
        // clear 3:1. See docs/KNOWLEDGE/polish/2026-07-23-cp10-high-contrast.md
        // for the per-pair audit table.
        //
        // Notable design choices vs the prior palette:
        //   * surface is lifted to RGB(31,31,31) so cards/panels are not
        //     visually identical to the window chrome; the 1px white border
        //     carries the structural separation, the tonal lift helps when
        //     the border is suppressed.
        //   * surface_hover passes 3:1 vs background (was 1.42:1, invisible
        //     against pure black) so ListBox/ListView/ComboBox/Splitter
        //     hover state reads.
        //   * accent_hover is a hue-shifted cyan, not a tint of the yellow
        //     accent, so the Button hover state is visually distinct from
        //     rest (was identical to rest in the prior palette).
        //   * selection is white, not the same yellow as accent, so the
        //     selected row in a list does not collide with the Button hover
        //     fill — both can appear on the same screen and used to be
        //     indistinguishable.
        //   * warning is a warm orange, not the same yellow as accent, so
        //     semantic meaning ("be careful") does not overlap with the
        //     brand accent.
        //   * danger/success are desaturated toward white so the hue is
        //     obvious without forcing full-saturation primaries (which were
        //     below the 7:1 threshold against pure black).
        return ThemePalette{
            to_color(RGB(0, 0, 0)),       // background   — pure black
            to_color(RGB(31, 31, 31)),    // surface      — subtle lift, paired with the white border for card separation
            to_color(RGB(96, 96, 96)),    // surface_hover — clears 3:1 vs background (3.34:1)
            to_color(RGB(255, 255, 255)), // border       — white
            to_color(RGB(255, 255, 255)), // text         — white
            to_color(RGB(220, 220, 220)), // text_secondary — 13.43:1 vs surface
            to_color(RGB(255, 235, 59)),  // accent       — yellow (brand)
            to_color(RGB(0, 255, 255)),   // accent_hover — cyan, hue-shifted from accent for state distinction
            to_color(RGB(0, 0, 0)),       // accent_text  — black on yellow/cyan
            to_color(RGB(255, 255, 255)), // selection    — white, distinct from yellow accent
            to_color(RGB(0, 0, 0)),       // selection_text — black on white
            to_color(RGB(255, 132, 132)), // danger       — 8.89:1 AAA
            to_color(RGB(80, 255, 132)),  // success      — 15.98:1 AAA
            to_color(RGB(255, 176, 0)),   // warning      — 11.46:1 AAA, orange to separate from yellow accent
            to_color(RGB(255, 255, 255)), // CP16: shadow tint — HC uses a white halo so the shadow reads as a glow, not a smudge
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
            to_color(RGB(0, 0, 0)),       // CP16: shadow tint (alpha baked by helper)
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

bool is_high_contrast(const ThemePalette& p) noexcept {
    // We don't tag the palette with a mode field (that would be a public API
    // break), so detect the high-contrast profile by its defining trait: a
    // near-black background paired with a near-white foreground and a
    // saturated, very-bright accent. The luminance thresholds mirror the
    // contrast ratio targets used in the audit table — anything below the
    // 0.30 / 0.05 cut-offs is too dark/light to look like a high-contrast
    // chrome, and accent above 0.50 catches the "vivid yellow / cyan"
    // family that the standard light/dark formulas do not handle well.
    auto lum = [](COLORREF c) noexcept {
        auto chan = [](BYTE v) noexcept {
            const double x = static_cast<double>(v) / 255.0;
            return x <= 0.03928 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * chan(GetRValue(c)) + 0.7152 * chan(GetGValue(c)) + 0.0722 * chan(GetBValue(c));
    };
    const double bg_lum = lum(p.background.rgb);
    const double fg_lum = lum(p.text.rgb);
    const double acc_lum = lum(p.accent.rgb);
    return bg_lum <= 0.05 && fg_lum >= 0.30 && acc_lum >= 0.50;
}

} // namespace nfui

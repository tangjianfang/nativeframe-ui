#include <nfui/Theme.hpp>
#include <nfui/Easing.hpp>
#include "test_helpers.hpp"

using nfui_test::expect;

int wmain() {
    bool ok = true;

    // --- theme_tokens: light vs dark vs high_contrast ---
    {
        nfui::ThemeTokens light = nfui::theme_tokens(nfui::ThemeMode::light);
        nfui::ThemeTokens dark  = nfui::theme_tokens(nfui::ThemeMode::dark);
        nfui::ThemeTokens hc    = nfui::theme_tokens(nfui::ThemeMode::high_contrast);
        ok = expect(light.window_background != dark.window_background,
                    L"light and dark backgrounds differ") && ok;
        ok = expect(hc.window_text == RGB(255, 255, 255),
                    L"HC text is pure white") && ok;
    }

    // --- theme_palette: structural invariants ---
    {
        const nfui::ThemePalette light = nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::ThemePalette dark  = nfui::theme_palette(nfui::ThemeMode::dark);
        ok = expect(light.background.rgb != light.surface.rgb,
                    L"light background differs from surface") && ok;
        ok = expect(dark.background.rgb != dark.text.rgb,
                    L"dark background differs from text") && ok;
        ok = expect(light.accent.rgb != light.accent_hover.rgb,
                    L"light accent differs from accent_hover") && ok;
    }

    // --- CP-B16/B17: divider + surface_variant tokens (FOLLOW_UP C+D) ---
    {
        const nfui::ThemePalette light = nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::ThemePalette dark  = nfui::theme_palette(nfui::ThemeMode::dark);
        const nfui::ThemePalette hc    = nfui::theme_palette(nfui::ThemeMode::high_contrast);

        // divider is non-zero, distinct from border, distinct from surface.
        ok = expect(light.divider.rgb != 0, L"light divider is non-zero") && ok;
        ok = expect(dark.divider.rgb  != 0, L"dark divider is non-zero")  && ok;
        ok = expect(hc.divider.rgb    != 0, L"HC divider is non-zero")    && ok;
        ok = expect(light.divider.rgb != light.border.rgb,
                    L"light divider distinct from border") && ok;
        ok = expect(dark.divider.rgb  != dark.border.rgb,
                    L"dark divider distinct from border")  && ok;
        ok = expect(hc.divider.rgb    != hc.border.rgb,
                    L"HC divider distinct from border")    && ok;
        ok = expect(light.divider.rgb != light.surface.rgb,
                    L"light divider distinct from surface") && ok;

        // surface_variant is distinct from surface and surface_hover.
        ok = expect(light.surface_variant.rgb != 0, L"light surface_variant is non-zero") && ok;
        ok = expect(dark.surface_variant.rgb  != 0, L"dark surface_variant is non-zero")  && ok;
        ok = expect(hc.surface_variant.rgb    != 0, L"HC surface_variant is non-zero")    && ok;
        ok = expect(light.surface_variant.rgb != light.surface.rgb &&
                    light.surface_variant.rgb != light.surface_hover.rgb,
                    L"light surface_variant distinct from surface / surface_hover") && ok;
        ok = expect(dark.surface_variant.rgb  != dark.surface.rgb &&
                    dark.surface_variant.rgb  != dark.surface_hover.rgb,
                    L"dark surface_variant distinct from surface / surface_hover") && ok;
        ok = expect(hc.surface_variant.rgb    != hc.surface.rgb &&
                    hc.surface_variant.rgb    != hc.surface_hover.rgb,
                    L"HC surface_variant distinct from surface / surface_hover") && ok;
    }

    // --- theme_palette: Claude Code brand colors ---
    {
        const nfui::ThemePalette light = nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::ThemePalette dark  = nfui::theme_palette(nfui::ThemeMode::dark);
        ok = expect(light.accent.rgb == RGB(217, 119, 87),
                    L"light accent is coral #D97757") && ok;
        ok = expect(dark.accent.rgb == RGB(232, 142, 110),
                    L"dark accent is desaturated coral #E88E6E") && ok;
        ok = expect(light.background.rgb == RGB(250, 249, 245),
                    L"light background is warm cream #FAF9F5") && ok;
        ok = expect(dark.background.rgb == RGB(36, 36, 38),
                    L"dark background is neutral cool ink #242426") && ok;
    }

    // --- theme_tokens derives from palette ---
    {
        const nfui::ThemePalette dark = nfui::theme_palette(nfui::ThemeMode::dark);
        const nfui::ThemeTokens t = nfui::theme_tokens(nfui::ThemeMode::dark);
        ok = expect(t.window_background == dark.background.rgb,
                    L"theme_tokens window_background matches palette") && ok;
    }

    // --- theme_metrics ---
    {
        const nfui::ThemeMetrics m = nfui::theme_metrics();
        ok = expect(m.corner_radius_control == 6, L"control radius == 6") && ok;
        ok = expect(m.corner_radius_card == 10, L"card radius == 10") && ok;
        ok = expect(m.spacing == 8, L"spacing == 8") && ok;
        ok = expect(m.border_width == 1, L"border_width == 1") && ok;
        ok = expect(m.row_height == 28, L"row_height == 28") && ok;
    }

    // --- high contrast invariants ---
    {
        const nfui::ThemePalette hc = nfui::theme_palette(nfui::ThemeMode::high_contrast);
        ok = expect(hc.background.rgb == RGB(0, 0, 0), L"HC background is pure black") && ok;
        ok = expect(hc.text.rgb == RGB(255, 255, 255), L"HC text is pure white") && ok;
        ok = expect(hc.surface.rgb != hc.background.rgb, L"HC surface distinct from background") && ok;
        ok = expect(hc.surface_hover.rgb != hc.background.rgb, L"HC surface_hover distinct from background") && ok;
        ok = expect(hc.accent.rgb != hc.accent_hover.rgb, L"HC accent and accent_hover distinct") && ok;
        ok = expect(hc.selection.rgb != hc.accent.rgb, L"HC selection distinct from accent") && ok;
        ok = expect(hc.warning.rgb != hc.accent.rgb, L"HC warning distinct from accent") && ok;
        ok = expect(nfui::is_high_contrast(hc), L"is_high_contrast recognises HC palette") && ok;
        ok = expect(!nfui::is_high_contrast(nfui::theme_palette(nfui::ThemeMode::light)),
                    L"is_high_contrast rejects light") && ok;
        ok = expect(!nfui::is_high_contrast(nfui::theme_palette(nfui::ThemeMode::dark)),
                    L"is_high_contrast rejects dark") && ok;
    }

    // --- chart_series_palette ---
    {
        const auto& light_series = nfui::chart_series_palette(nfui::ThemeMode::light);
        const auto& dark_series  = nfui::chart_series_palette(nfui::ThemeMode::dark);
        ok = expect(light_series.size() == 8, L"light series palette has 8 colors") && ok;
        ok = expect(dark_series.size() == 8, L"dark series palette has 8 colors") && ok;
        // Adjacent series colors must be distinct for visual separation.
        bool all_distinct = true;
        for (std::size_t i = 1; i < 8; ++i) {
            if (light_series[i].rgb == light_series[i - 1].rgb) { all_distinct = false; break; }
        }
        ok = expect(all_distinct, L"light series adjacent colors are distinct") && ok;
        // chart_series_color wraps index.
        nfui::Color c0 = nfui::chart_series_color(nfui::ThemeMode::light, 0);
        nfui::Color c8 = nfui::chart_series_color(nfui::ThemeMode::light, 8);
        ok = expect(c0.rgb == c8.rgb, L"chart_series_color wraps at index 8") && ok;
    }

    // --- lerp_color ---
    {
        const nfui::Color black{RGB(0, 0, 0)};
        const nfui::Color white{RGB(255, 255, 255)};
        ok = expect(nfui::lerp_color(black, white, 0.0f).rgb == black.rgb,
                    L"lerp_color t=0 returns a") && ok;
        ok = expect(nfui::lerp_color(black, white, 1.0f).rgb == white.rgb,
                    L"lerp_color t=1 returns b") && ok;
        const nfui::Color mid = nfui::lerp_color(black, white, 0.5f);
        ok = expect(GetRValue(mid.rgb) >= 126 && GetRValue(mid.rgb) <= 129,
                    L"lerp_color t=0.5 is midpoint") && ok;
    }

    // --- lerp_palette ---
    {
        const nfui::ThemePalette pd = nfui::theme_palette(nfui::ThemeMode::dark);
        const nfui::ThemePalette pl = nfui::theme_palette(nfui::ThemeMode::light);
        ok = expect(nfui::lerp_palette(pd, pl, 0.0f).background.rgb == pd.background.rgb,
                    L"lerp_palette t=0 returns a") && ok;
        ok = expect(nfui::lerp_palette(pd, pl, 1.0f).background.rgb == pl.background.rgb,
                    L"lerp_palette t=1 returns b") && ok;
        const nfui::ThemePalette pm = nfui::lerp_palette(pd, pl, 0.5f);
        auto near_mid = [](COLORREF a, COLORREF b, COLORREF m) {
            return std::abs(static_cast<int>(GetRValue(m)) - (GetRValue(a) + GetRValue(b)) / 2) <= 1 &&
                   std::abs(static_cast<int>(GetGValue(m)) - (GetGValue(a) + GetGValue(b)) / 2) <= 1 &&
                   std::abs(static_cast<int>(GetBValue(m)) - (GetBValue(a) + GetBValue(b)) / 2) <= 1;
        };
        ok = expect(near_mid(pd.background.rgb, pl.background.rgb, pm.background.rgb),
                    L"lerp_palette background is midpoint") && ok;
        ok = expect(near_mid(pd.accent.rgb, pl.accent.rgb, pm.accent.rgb),
                    L"lerp_palette accent is midpoint") && ok;
    }

    // --- easing curves ---
    {
        using namespace nfui;
        ok = expect(ease_linear(0.0f) == 0.0f && ease_linear(1.0f) == 1.0f,
                    L"ease_linear endpoints") && ok;
        ok = expect(ease_out_cubic(0.0f) == 0.0f && ease_out_cubic(1.0f) == 1.0f,
                    L"ease_out_cubic endpoints") && ok;
        ok = expect(ease_in_out_cubic(0.0f) == 0.0f && ease_in_out_cubic(1.0f) == 1.0f,
                    L"ease_in_out_cubic endpoints") && ok;
        ok = expect(ease_out_quint(0.0f) == 0.0f && ease_out_quint(1.0f) == 1.0f,
                    L"ease_out_quint endpoints") && ok;
        ok = expect(ease_out_cubic(-1.0f) == 0.0f && ease_out_cubic(2.0f) == 1.0f,
                    L"ease curves clamp out-of-range") && ok;
        bool mono = true;
        float prev = ease_out_cubic(0.0f);
        for (int i = 1; i <= 5; ++i) {
            float v = ease_out_cubic(static_cast<float>(i) / 5.0f);
            if (v < prev) mono = false;
            prev = v;
        }
        ok = expect(mono, L"ease_out_cubic is monotonic") && ok;
    }

    return ok ? 0 : 1;
}

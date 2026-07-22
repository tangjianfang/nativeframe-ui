#include <nfui/Controls/ProgressBar.hpp>
#include <nfui/Paint.hpp>
#include <commctrl.h>

namespace nfui {
bool ProgressBar::create(const ControlCreateParams& params) noexcept {
    // P1.4: drop PBS_SMOOTH so the native bar chrome stops painting over our
    // self-paint output. WS_CLIPCHILDREN keeps neighbouring controls from
    // leaving paste marks during repaint storms.
    return create_native(PROGRESS_CLASSW, params, WS_CLIPCHILDREN);
}

void ProgressBar::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color track = style_.surface_brush.value_or(p.surface);
    // CP8: text_secondary clears WCAG AA against the surface track in light,
    // dark, and HC palettes (the prior 0.55 blend fell below 4.5:1 on light
    // and made the disabled fill nearly invisible). Disabled reads as "faded
    // secondary tone" instead of "tinted grey that blends into the track".
    const Color fill  = state.enabled
        ? style_.bar_color.value_or(p.accent)
        : p.text_secondary;
    // CP8: radius mirrors Button::corner_radius_control so the rounded track
    // sits in the same visual language as the rest of the chrome.
    const int radius = theme_metrics().corner_radius_control;
    const RECT& b = state.bounds;
    PBRANGE range{};
    // PBM_GETRANGE fills the low word of wParam via return; lParam is the PPBRANGE.
    SendMessageW(hwnd(), PBM_GETRANGE, FALSE, reinterpret_cast<LPARAM>(&range));
    const int pos = static_cast<int>(SendMessageW(hwnd(), PBM_GETPOS, 0, 0));
    const int span = range.iHigh - range.iLow;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    // CP8: RoundRect leaves outside-corner pixels untouched; clear them with
    // the current palette first so a theme swap cannot preserve a frame from
    // the previous palette in the rounded boundary (mirrors Button::on_paint).
    fill_rect(target, paint_bounds, p.background);
    fill_rounded_rect(target, paint_bounds, radius, track, p.border);
    if (span > 0 && pos > range.iLow && paint_bounds.right > paint_bounds.left) {
        const int fill_w = (paint_bounds.right - paint_bounds.left) * (pos - range.iLow) / span;
        RECT bar{ paint_bounds.left, paint_bounds.top, paint_bounds.left + fill_w, paint_bounds.bottom };
        fill_rect(target, bar, fill);
    }
}
} // namespace nfui

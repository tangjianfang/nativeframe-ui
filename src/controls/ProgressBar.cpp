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
    const Color fill  = style_.bar_color.value_or(p.accent);
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
    fill_rect(target, paint_bounds, track);
    if (span > 0 && pos > range.iLow && paint_bounds.right > paint_bounds.left) {
        const int fill_w = (paint_bounds.right - paint_bounds.left) * (pos - range.iLow) / span;
        RECT bar{ paint_bounds.left, paint_bounds.top, paint_bounds.left + fill_w, paint_bounds.bottom };
        fill_rect(target, bar, fill);
    }
}
} // namespace nfui

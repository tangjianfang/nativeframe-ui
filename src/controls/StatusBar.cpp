#include <nfui/Controls/StatusBar.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>

namespace nfui {
bool StatusBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams status_params = params;
    status_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(STATUSCLASSNAMEW, status_params, SBARS_SIZEGRIP);
}

void StatusBar::on_paint(HDC dc, const PaintState& state) noexcept {
    // P6.1: self-paint the bar background with palette.surface; ComCtl32 v6
    // ignores SB_SETBKCOLOR. See
    // docs/KNOWLEDGE/polish/2026-07-22-statusbar-theme-color.md.
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color fill = style_.surface_brush.value_or(p.surface);
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, fill);
    // The subclass proc falls through to DefSubclassProc after this returns,
    // so the status bar's own painting of text/grip still happens on top.
}
} // namespace nfui

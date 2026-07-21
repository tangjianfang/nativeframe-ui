#include <nfui/Controls/Panel.hpp>
#include <nfui/Paint.hpp>

namespace nfui {
bool Panel::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams panel_params = params;
    // P1.4: drop SS_WHITERECT; Panel paints itself with palette.surface via
    // on_paint (wants_self_paint() -> true). WS_CLIPCHILDREN prevents nested
    // children from leaving halo artefacts during repaint.
    panel_params.style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    return create_native(L"STATIC", panel_params, 0);
}

void Panel::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color bg = style_.surface_brush.value_or(p.surface);
    const Color border = style_.accent.value_or(p.border);
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, bg);
    // hairline border so a Panel can serve as a subtle grouping surface
    const int bw = (border.rgb == bg.rgb) ? 0 : 1;
    if (bw > 0) {
        RECT top{ paint_bounds.left, paint_bounds.top, paint_bounds.right, paint_bounds.top + bw };
        RECT bot{ paint_bounds.left, paint_bounds.bottom - bw, paint_bounds.right, paint_bounds.bottom };
        RECT lft{ paint_bounds.left, paint_bounds.top, paint_bounds.left + bw, paint_bounds.bottom };
        RECT rgt{ paint_bounds.right - bw, paint_bounds.top, paint_bounds.right, paint_bounds.bottom };
        fill_rect(target, top, border);
        fill_rect(target, bot, border);
        fill_rect(target, lft, border);
        fill_rect(target, rgt, border);
    }
}
} // namespace nfui

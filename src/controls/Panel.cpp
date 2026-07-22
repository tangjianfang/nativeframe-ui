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
    // CP16: optional elevation. 0 = flat (legacy), 1/2/3 = small / medium
    // / large drop shadow + vertical surface gradient. HC profiles keep
    // elevation == 0 — the depth language there is "flat, single tone",
    // not shadow + gradient.
    const int elevation = style_.elevation.value_or(0);
    const bool wants_depth = elevation > 0 && !is_high_contrast(p);
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;

    // CP16: drop shadow first so the inner rect overwrites it.
    if (wants_depth) {
        const int radius = (elevation == 1) ? 6 : (elevation == 2 ? 10 : 14);
        paint_drop_shadow(target, paint_bounds, radius, elevation, p.shadow);
    }
    // CP2: erase the surrounding background so the rounded shape does
    // not leave a halo of stale colour from a previous palette.
    fill_rect(target, paint_bounds, p.background);

    // CP16: when elevation is set, fill with a soft top→bottom gradient
    // so the panel reads as a card with light from above. Otherwise use
    // the flat surface fill that the rest of the framework already
    // expects.
    if (wants_depth) {
        const int radius = (elevation == 1) ? 6 : (elevation == 2 ? 10 : 14);
        const Color grad_top    = lighten(bg, 0.05f);
        const Color grad_bottom = bg;
        paint_linear_gradient(target, paint_bounds, radius, grad_top, grad_bottom);
    } else {
        fill_rect(target, paint_bounds, bg);
    }

    // Hairline border (auto-suppresses when border == fill).
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

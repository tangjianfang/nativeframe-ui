#include <nfui/Controls/IconView.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool IconView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style = WS_CHILD | WS_VISIBLE;
    // IconView is a passive image-display control: SS_OWNERDRAW STATIC, no
    // WS_TABSTOP, not selectable/clickable. CP5B deliberately rejected
    // hover halos, selection fills, and focus rings as inappropriate chrome
    // for a static image (see docs/KNOWLEDGE/polish/2026-07-23-cp5-iconview-
    // tooltip.md, audit item 4). We honour that decision here and in on_paint.
    return create_native(L"STATIC", p, SS_OWNERDRAW);
}

void IconView::set_icon(HICON icon) noexcept {
    icon_ = icon;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void IconView::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const RECT& b = state.bounds;
    const int cw = b.right - b.left;
    const int ch = b.bottom - b.top;
    if (cw <= 0 || ch <= 0) return;

    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, cw, ch}
        : b;
    const int base_x = mem.valid() ? 0 : b.left;
    const int base_y = mem.valid() ? 0 : b.top;

    // Always paint a background. Pre-CP8 left this blank when icon_ was null,
    // which exposed the system STATIC bg and looked broken against a custom
    // palette. The user can override via IconViewStyle::background.
    const Color bg = style_.background.value_or(p.background);
    fill_rect(target, paint_bounds, bg);

    if (icon_ == nullptr) return;

    // Determine the draw size. With no style set we preserve the historical
    // stretch-to-fill behaviour. The opt-in style fields switch to fixed-size
    // or aspect-preserving rendering; both are centred within the *padded*
    // client rect so an icon never sits flush against one edge.
    const DpiScale scale(dpi_of(hwnd()));
    const int pad = scale.logical_to_pixels(style_.padding.value_or(0));
    const int inner_w = cw - 2 * pad;
    const int inner_h = ch - 2 * pad;
    if (inner_w <= 0 || inner_h <= 0) return;

    int draw_w = inner_w;
    int draw_h = inner_h;
    if (style_.pixel_size.has_value()) {
        // pixel_size is a *logical* dimension: scale to device pixels so the
        // icon keeps a constant physical size across monitors and DPI changes
        // (logical-vs-device separation, see docs/ARCHITECTURE.md).
        const int px = scale.logical_to_pixels(*style_.pixel_size);
        draw_w = draw_h = (px > 0) ? px : 1;
    } else if (style_.preserve_aspect.value_or(false)) {
        // Icons are square; fit the largest centred square inside the bounds.
        draw_w = draw_h = (inner_w < inner_h) ? inner_w : inner_h;
    }
    if (draw_w > inner_w) draw_w = inner_w;
    if (draw_h > inner_h) draw_h = inner_h;

    const int ox = base_x + pad + (inner_w - draw_w) / 2;
    const int oy = base_y + pad + (inner_h - draw_h) / 2;

    // SS_OWNERDRAW statics do not reliably report ODS_DISABLED, so consult the
    // authoritative window state as well as the reflected PaintState.
    const bool enabled = state.enabled && (IsWindowEnabled(hwnd()) != FALSE);
    if (enabled) {
        DrawIconEx(target, ox, oy, icon_, draw_w, draw_h, 0, nullptr, DI_NORMAL);
    } else {
        // Documented Win32 disabled/embossed rendering: greys the icon with the
        // system button-shadow colours, keeping IconView visually consistent
        // with disabled native controls.
        DrawState(target, nullptr, nullptr, reinterpret_cast<LPARAM>(icon_), 0,
                  ox, oy, draw_w, draw_h, DST_ICON | DSS_DISABLED);
    }
}

} // namespace nfui
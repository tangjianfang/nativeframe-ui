#include <nfui/Controls/IconView.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool IconView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style = WS_CHILD | WS_VISIBLE;
    return create_native(L"STATIC", p, SS_OWNERDRAW);
}

void IconView::set_icon(HICON icon) noexcept {
    icon_ = icon;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void IconView::on_paint(HDC dc, const PaintState& state) noexcept {
    if (icon_ == nullptr) return;
    const RECT& b = state.bounds;
    const int cw = b.right - b.left;
    const int ch = b.bottom - b.top;
    if (cw <= 0 || ch <= 0) return;

    // Determine the draw size. With no style set we preserve the historical
    // stretch-to-fill behaviour. The opt-in style fields switch to fixed-size
    // or aspect-preserving rendering; both are centred within the client rect
    // so an icon never sits flush against one edge.
    int draw_w = cw;
    int draw_h = ch;
    if (style_.pixel_size.has_value()) {
        // pixel_size is a *logical* dimension: scale to device pixels so the
        // icon keeps a constant physical size across monitors and DPI changes
        // (logical-vs-device separation, see docs/ARCHITECTURE.md).
        const DpiScale scale(dpi_of(hwnd()));
        const int px = scale.logical_to_pixels(*style_.pixel_size);
        draw_w = draw_h = (px > 0) ? px : 1;
    } else if (style_.preserve_aspect.value_or(false)) {
        // Icons are square; fit the largest centred square inside the bounds.
        draw_w = draw_h = (cw < ch) ? cw : ch;
    }
    if (draw_w > cw) draw_w = cw;
    if (draw_h > ch) draw_h = ch;

    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const int base_x = mem.valid() ? 0 : b.left;
    const int base_y = mem.valid() ? 0 : b.top;
    const int ox = base_x + (cw - draw_w) / 2;
    const int oy = base_y + (ch - draw_h) / 2;

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

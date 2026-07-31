#include <nfui/Controls/Scrollbar.hpp>
#include <nfui/Controls/Detail/effective_palette.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <commctrl.h>
#include <windowsx.h>

namespace nfui {

namespace {

// CP-A4: thumb width in logical px. Hover state expands the thumb to
// `thumb_width_hover` so a user aiming for the thumb has a generous
// target. Sizes scale with DPI via DpiScale.
constexpr int thumb_width_rest = 4;
constexpr int thumb_width_hover = 8;

// CP-A4: thumb alpha at 60% — large enough to read against the
// transparent track without dominating the chrome. The native
// SCROLLBAR thumb is closer to 100% on its own surface, but our
// track is transparent so a strongly opaque thumb reads as a band
// rather than a discrete elevator.
constexpr unsigned char thumb_alpha = 0x99;  // 0x99 = 153 ≈ 60%

RECT track_rect_for(HWND bar_hwnd, bool vertical) noexcept {
    RECT client{};
    GetClientRect(bar_hwnd, &client);
    if (vertical) {
        return RECT{client.left, client.top, client.right, client.bottom};
    }
    return RECT{client.top, client.left, client.bottom, client.right};
}

// CP-B19: thumb geometry shared between the standalone Scrollbar instance
// (which owns the band via GetClientRect) and the NM_CUSTOMDRAW forwarding
// path used by ListView / TreeView / Edit (where the band is the right /
// bottom edge of the host control's client area). Factoring the math
// here keeps both paths in lockstep when we tweak thumb length or width.
RECT thumb_rect_for(const RECT& track, bool vertical, int position,
                     int min, int max, int width, int dpi) noexcept {
    const DpiScale dpi_scale{dpi};
    const int span = std::max(1, max - min);
    const int track_len = vertical
        ? (track.bottom - track.top)
        : (track.right - track.left);
    int thumb_len = track_len / 4;
    if (thumb_len < dpi_scale.logical_to_pixels(24)) {
        thumb_len = dpi_scale.logical_to_pixels(24);
    }
    const int clamped_pos = (position < min) ? min : (position > max ? max : position);
    const int offset = (clamped_pos - min) * (track_len - thumb_len) / span;
    if (vertical) {
        const int track_w = static_cast<int>(track.right - track.left);
        const int thickness = std::min(width, track_w);
        const int left = track.left + (track_w - thickness) / 2;
        return RECT{left, track.top + offset, left + thickness, track.top + offset + thumb_len};
    }
    const int track_h = static_cast<int>(track.bottom - track.top);
    const int thickness = std::min(width, track_h);
    const int top = track.top + (track_h - thickness) / 2;
    return RECT{track.left + offset, top, track.left + offset + thumb_len, top + thickness};
}

} // namespace

bool Scrollbar::create(const ControlCreateParams& params, bool vertical) noexcept {
    ControlCreateParams sb_params = params;
    // CP-A4: suppress the native arrow buttons (SBS_SIZEBOXBOTTOMRIGHT
    // would draw a 4-button chrome). SBS_VERT (or no SBS_HORZ) lets the
    // native SCROLLBAR host instantiate without arrows; the chrome
    // subclass then paints the thumb on top.
    sb_params.style = WS_CHILD | WS_VISIBLE;
    DWORD extra = vertical ? SBS_VERT : SBS_HORZ;
    if (!create_native(L"SCROLLBAR", sb_params, extra)) {
        return false;
    }
    vertical_ = vertical;
    // CP-A4: disable comctl32's themed background so the chrome subclass's
    // thumb (alpha-blended palette.accent) is the only visible pixel on
    // the bar. Without this, the uxtheme background brush (light grey in
    // dark mode) draws behind the thumb and the track reads as a pale
    // stripe.
    theme_disable_window_theme(hwnd());
    // CP-A4: install the chrome subclass AFTER create_native. SetWindowSubclass
    // dispatches in reverse install order; ours runs FIRST on WM_PAINT
    // and returns 0 so the native SCROLLBAR pass never paints on top of
    // our thumb.
    if (SetWindowSubclass(hwnd(), &Scrollbar::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void Scrollbar::set_range(int min, int max) noexcept {
    if (hwnd() == nullptr) return;
    min_ = min;
    max_ = max;
    SendMessageW(hwnd(), SBM_SETRANGE, TRUE, MAKELPARAM(min, max));
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void Scrollbar::set_position(int pos) noexcept {
    if (hwnd() == nullptr) return;
    position_ = pos;
    SendMessageW(hwnd(), SBM_SETPOS, TRUE, pos);
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void Scrollbar::on_palette_changed() noexcept {
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Scrollbar::on_subclass_mouse_move(LPARAM lparam) noexcept {
    // CP-A4: hit-test the thumb rect against the cursor position. Both
    // coords from lparam are in client space already. The hovered flag
    // is read by paint_chrome to expand the thumb.
    const POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
    if (hit_test_thumb(pt)) {
        if (!hovered_thumb_) {
            hovered_thumb_ = true;
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    } else if (hovered_thumb_) {
        hovered_thumb_ = false;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Scrollbar::on_subclass_mouse_leave() noexcept {
    if (hovered_thumb_) {
        hovered_thumb_ = false;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

bool Scrollbar::hit_test_thumb(POINT pt) const noexcept {
    const RECT thumb = thumb_rect();
    return PtInRect(&thumb, pt) != FALSE;
}

RECT Scrollbar::thumb_rect() const noexcept {
    HWND bar_hwnd = hwnd();
    if (bar_hwnd == nullptr) return RECT{};
    const DpiScale dpi{dpi_of(bar_hwnd)};
    const int rest_w = dpi.logical_to_pixels(thumb_width_rest);
    const int hover_w = dpi.logical_to_pixels(thumb_width_hover);
    const int width = hovered_thumb_ ? hover_w : rest_w;
    const RECT track = track_rect_for(bar_hwnd, vertical_);
    return thumb_rect_for(track, vertical_, position_, min_, max_, width, dpi.dpi());
}

void Scrollbar::paint_thumb_into(HDC target, const RECT& track, bool vertical,
                                  int position, int min, int max,
                                  const ThemePalette& palette) noexcept {
    if (target == nullptr) return;
    if (track.right <= track.left || track.bottom <= track.top) return;

    // CP-B19: forwarding contract. The forwarding host supplies a band
    // along its right or bottom edge in its own client coordinates.
    // We mirror the standalone instance's geometry by reusing the same
    // helper, with width = rest (4 logical px) because the host control
    // already paints its own hover state. A future CP could expand the
    // thumb on hover inside the host control's custom draw if needed;
    // for now we paint a single, stable-width thumb and let the host
    // invalidate on hover via its own subclass proc.
    HWND any_hwnd = WindowFromDC(target);
    const int dpi = (any_hwnd != nullptr) ? dpi_of(any_hwnd) : 96;
    const DpiScale dpi_scale{dpi};
    const int width = dpi_scale.logical_to_pixels(thumb_width_rest);
    const RECT thumb = thumb_rect_for(track, vertical, position, min, max, width, dpi);
    if (thumb.right <= thumb.left || thumb.bottom <= thumb.top) return;

    // Track is the host's existing background — we don't repaint it.
    // Only the rounded thumb (alpha-blended accent + stroke border) is
    // ours, matching paint_chrome on the standalone instance.
    const int t_radius = static_cast<int>(std::min<long>(
        (thumb.right - thumb.left), (thumb.bottom - thumb.top))) / 2;
    fill_rect_alpha(target, thumb, palette.accent, thumb_alpha);
    const HPEN pen = CreatePen(PS_SOLID, 1, palette.accent.rgb);
    if (pen != nullptr) {
        const HGDIOBJ old_pen = SelectObject(target, pen);
        const HGDIOBJ old_brush = SelectObject(target, GetStockObject(NULL_BRUSH));
        RoundRect(target, thumb.left, thumb.top, thumb.right, thumb.bottom,
                  t_radius * 2, t_radius * 2);
        SelectObject(target, old_brush);
        SelectObject(target, old_pen);
        DeleteObject(pen);
    }
}

void Scrollbar::paint_chrome(HDC dc) noexcept {
    if (dc == nullptr) return;
    HWND bar_hwnd = hwnd();
    if (bar_hwnd == nullptr) return;

    const ThemePalette& p = detail::effective_palette(palette());
    RECT client{};
    GetClientRect(bar_hwnd, &client);
    if (client.right <= client.left || client.bottom <= client.top) return;

    MemoryDC mem(dc, client);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds{0, 0, client.right - client.left, client.bottom - client.top};

    // CP-A4: track is transparent — fill with the palette background so
    // the bar doesn't reveal any native chrome. This is invisible on
    // hover and useful in case a parent panel's background changes
    // between paints.
    fill_rect(target, paint_bounds, p.background);

    // CP-B19: the thumb paint is the same code path the NM_CUSTOMDRAW
    // forwarding hosts use. Keep paint_thumb_into as the single source
    // of truth so changing the chrome doesn't require a sync change in
    // ListView / TreeView / Edit custom-draw handlers.
    paint_thumb_into(target, paint_bounds, vertical_, position_, min_, max_, p);
}

void Scrollbar::on_paint(HDC dc, const PaintState& state) noexcept {
    (void)state;
    paint_chrome(dc);
}

LRESULT CALLBACK Scrollbar::visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept {
    auto* sb = reinterpret_cast<Scrollbar*>(ref_data);
    switch (message) {
    case WM_PAINT: {
        // CP-A4: chrome proc runs FIRST (last-installed-runs-first).
        // Paint the themed chrome ourselves and return 0 so DefSubclassProc
        // does not chain into the native SCROLLBAR pass.
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (dc != nullptr) {
            if (sb != nullptr) {
                sb->paint_chrome(dc);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN: {
        // CP-A4: hit-test the thumb; if hit, capture the mouse so the
        // drag tracks the cursor even when it leaves the bar. Clicks
        // on bare track are no-ops (a follow-up CP would page-scroll
        // on these — out of scope for the standalone wrapper).
        if (sb != nullptr) {
            const POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            if (sb->hit_test_thumb(pt)) {
                sb->set_position(sb->position());  // no-op, but lets us set `dragging_` if added later
                SetCapture(hwnd);
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (sb != nullptr) {
            sb->on_subclass_mouse_move(lparam);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (sb != nullptr) {
            sb->on_subclass_mouse_leave();
        }
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &Scrollbar::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui

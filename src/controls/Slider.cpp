#include <nfui/Controls/Slider.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <commctrl.h>

namespace nfui {

namespace {

// CP25: resolve the effective palette. Mirrors ProgressBar's
// `effective_palette` helper so the chrome subclass proc — which is a
// free function and cannot reach the protected palette() accessor — can
// be passed the resolved palette by value.
const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

// CP25: project a slider position to a thumb centre pixel along the rail
// axis. Uses TBM_GETRANGE / TBM_GETPOS so callers do not need to track the
// state machine separately. `channel` is the perpendicular dimension of
// the rail (height for horizontal, width for vertical); the rail sits
// centred in the channel and the thumb is drawn at the rail's centre
// perpendicular position.
struct SliderGeometry {
    RECT  rail{};        // the un-filled rail rect
    RECT  filled{};      // the filled portion (rail from low edge to thumb)
    POINT thumb_centre{};
    int   thumb_radius{};
};

[[nodiscard]] SliderGeometry compute_geometry(HWND slider_hwnd,
                                              const DpiScale& dpi,
                                              const SliderStyle& style,
                                              int thumb_radius) noexcept {
    SliderGeometry g{};

    RECT client{};
    GetClientRect(slider_hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return g;
    }

    // Track channel thickness in device px — uses theme_metrics spacing so
    // it scales with the rest of the chrome on DPI bumps.
    const int channel = std::max(2, dpi.logical_to_pixels(theme_metrics().spacing / 2));

    DWORD style_bits = GetWindowLongW(slider_hwnd, GWL_STYLE);
    const bool vertical = (style_bits & TBS_VERT) != 0;

    // Pull live range + position via the SDK-provided TBM_GETRANGEMIN/MAX
    // and TBM_GETPOS. The original CP25 draft used the macro TBM_GETRANGE
    // for the low/high pair, but that symbol is not exported by the public
    // Windows SDK — only the per-bound queries are. So we read the bounds
    // directly. TBM_GETRANGE does exist inside commctrl.h in the form of an
    // undocumented struct-returning variant, but the supported path is the
    // two single-bound queries we use here.
    int low  = static_cast<int>(SendMessageW(slider_hwnd, TBM_GETRANGEMIN, 0, 0));
    int high = static_cast<int>(SendMessageW(slider_hwnd, TBM_GETRANGEMAX, 0, 0));
    const int pos = static_cast<int>(SendMessageW(slider_hwnd, TBM_GETPOS, 0, 0));

    if (vertical) {
        const int rail_top    = thumb_radius;
        const int rail_bottom = height - thumb_radius;
        const int rail_left   = (width - channel) / 2;
        const int rail_right  = rail_left + channel;
        g.rail = RECT{rail_left, rail_top, rail_right, rail_bottom};

        const int span = std::max(1, high - low);
        const int thumb_y = rail_top + (rail_bottom - rail_top) * (high - pos) / span;
        g.thumb_centre = POINT{(rail_left + rail_right) / 2, thumb_y};
        g.filled = RECT{rail_left, thumb_y, rail_right, rail_bottom};
    } else {
        const int rail_left   = thumb_radius;
        const int rail_right  = width - thumb_radius;
        const int rail_top    = (height - channel) / 2;
        const int rail_bottom = rail_top + channel;
        g.rail = RECT{rail_left, rail_top, rail_right, rail_bottom};

        const int span = std::max(1, high - low);
        const int thumb_x = rail_left + (rail_right - rail_left) * (pos - low) / span;
        g.thumb_centre = POINT{thumb_x, (rail_top + rail_bottom) / 2};
        g.filled = RECT{rail_left, rail_top, thumb_x, rail_bottom};
    }

    // Honour caller-supplied thumb radius override when present.
    g.thumb_radius = style.thumb_radius.value_or(thumb_radius);
    return g;
}

// CP25: paint the themed chrome — track + filled portion + thumb. The
// filled portion overlays the track on the low-side of the thumb; the
// thumb disc sits centred on the rail axis. Caller supplies the resolved
// palette + style so this can be called from both the member paint_chrome
// (which has access to palette()) and the chrome subclass proc (which
// doesn't).
void paint_slider_chrome(HDC dc, HWND slider_hwnd,
                         const ThemePalette& p,
                         const SliderStyle& style,
                         bool enabled) noexcept {
    if (slider_hwnd == nullptr || dc == nullptr) return;

    const DpiScale dpi{dpi_of(slider_hwnd)};
    const int thumb_radius = dpi.logical_to_pixels(
        style.thumb_radius.value_or(theme_metrics().corner_radius_control));

    const SliderGeometry g = compute_geometry(slider_hwnd, dpi, style, thumb_radius);
    if (g.rail.right <= g.rail.left || g.rail.bottom <= g.rail.top) return;

    const Color track  = style.track_color.value_or(p.border);
    const Color filled = style.filled_color.value_or(p.accent);
    // CP25: disabled uses text_secondary (mirrors ProgressBar CP8) so the
    // thumb reads as faded secondary tone instead of "tinted grey that
    // blends in". Filled portion also drops to text_secondary.
    const Color thumb_col = enabled
        ? style.thumb_color.value_or(p.accent)
        : p.text_secondary;
    const Color filled_col = enabled ? filled : p.text_secondary;

    RECT client{};
    GetClientRect(slider_hwnd, &client);
    MemoryDC mem(dc, client);
    HDC target = mem.valid() ? mem.dc() : dc;

    const RECT paint_bounds{0, 0, client.right - client.left,
                            client.bottom - client.top};
    // CP8/CP25: clear the rounded boundary with the window background so a
    // theme swap cannot preserve a frame from the previous palette in the
    // rounded corners (mirrors Button / ProgressBar).
    fill_rect(target, paint_bounds, p.background);

    // Rail — drawn as a rounded rect with the un-filled colour. Cast
    // through int because std::max is a template that needs both args to
    // share a single deduced type; RECT members are LONG.
    const int rail_height = std::max(1, static_cast<int>(g.rail.bottom - g.rail.top));
    const int channel_radius = std::max(2, rail_height / 2);
    fill_rounded_rect(target, g.rail, channel_radius, track, track);

    // Filled portion — overlay on top of the rail from the low edge to
    // the thumb centre.
    if (g.filled.right > g.filled.left && g.filled.bottom > g.filled.top) {
        fill_rounded_rect(target, g.filled, channel_radius, filled_col, filled_col);
    }

    // Thumb disc.
    const int r = g.thumb_radius;
    RECT thumb_rect{g.thumb_centre.x - r, g.thumb_centre.y - r,
                    g.thumb_centre.x + r, g.thumb_centre.y + r};
    fill_ellipse(target, thumb_rect, thumb_col);
    // Subtle border so the thumb reads against the rail in high-contrast.
    draw_ellipse(target, thumb_rect, p.border, 1);

    // CP25-B: focus ring around the thumb when the slider has keyboard
    // focus. Uses stroke-only paint_focus_border so the disc stays visible
    // and the ring sits on top. Honour enabled state so disabled doesn't
    // get a misleading focus indicator.
    if (enabled && GetFocus() == slider_hwnd) {
        RECT focus_rect = thumb_rect;
        // Inflate by 2 device px so the ring is visible without overlapping
        // the thumb edge by more than half a stroke.
        InflateRect(&focus_rect, 2, 2);
        paint_focus_border(target, focus_rect, p.accent, 1);
    }
}

} // namespace

bool Slider::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams sb_params = params;
    sb_params.style = (params.style & ~WS_TABSTOP) | WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    // CP25: install TBS_TRANSPARENT + TBS_NOTIFYBUBBLE so the native chrome
    // (which we suppress in paint) doesn't draw ticks we can't erase, and so
    // we still receive WM_HSCROLL / WM_VSCROLL for position changes.
    DWORD extra = 0;
    if (!create_native(TRACKBAR_CLASSW, sb_params, extra)) {
        return false;
    }
    // CP25: install the chrome subclass AFTER create_native. SetWindowSubclass
    // dispatches in REVERSE install order; ours runs FIRST on WM_PAINT and
    // returns 0 so the native TRACKBAR pass never paints on top of our fill.
    if (SetWindowSubclass(hwnd(), &Slider::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void Slider::set_orientation(Slider::Orientation orientation) noexcept {
    if (orientation_ == orientation) return;
    orientation_ = orientation;
    if (hwnd() != nullptr) {
        LONG_PTR bits = GetWindowLongPtrW(hwnd(), GWL_STYLE);
        if (orientation == Orientation::vertical) {
            bits |= TBS_VERT;
        } else {
            bits &= ~static_cast<LONG_PTR>(TBS_VERT);
        }
        SetWindowLongPtrW(hwnd(), GWL_STYLE, bits);
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Slider::set_style(SliderStyle style) noexcept {
    style_ = style;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Slider::set_range(int low, int high) noexcept {
    if (hwnd() == nullptr) return;
    SendMessageW(hwnd(), TBM_SETRANGE, TRUE, MAKELPARAM(low, high));
}

void Slider::set_pos(int position) noexcept {
    if (hwnd() == nullptr) return;
    SendMessageW(hwnd(), TBM_SETPOS, TRUE, position);
}

int Slider::pos() const noexcept {
    if (hwnd() == nullptr) return 0;
    return static_cast<int>(SendMessageW(hwnd(), TBM_GETPOS, 0, 0));
}

int Slider::range_low() const noexcept {
    if (hwnd() == nullptr) return 0;
    return static_cast<int>(SendMessageW(hwnd(), TBM_GETRANGEMIN, 0, 0));
}

int Slider::range_high() const noexcept {
    if (hwnd() == nullptr) return 0;
    return static_cast<int>(SendMessageW(hwnd(), TBM_GETRANGEMAX, 0, 0));
}

void Slider::paint_chrome(HDC dc) noexcept {
    if (dc == nullptr) return;
    HWND slider_hwnd = hwnd();
    if (slider_hwnd == nullptr) return;
    const ThemePalette* pal_ptr = palette();
    const ThemePalette& p = pal_ptr ? *pal_ptr : theme_palette(ThemeMode::light);
    const bool enabled = IsWindowEnabled(slider_hwnd) != FALSE;
    paint_slider_chrome(dc, slider_hwnd, p, style_, enabled);
}

void Slider::on_paint(HDC dc, const PaintState& state) noexcept {
    // Fallback path: if any caller dispatches paint directly (e.g. via
    // framework internals), paint via paint_chrome so the visual contract
    // holds. The chrome subclass normally intercepts WM_PAINT first.
    (void)state;
    paint_chrome(dc);
}

LRESULT CALLBACK Slider::visual_subclass_proc(HWND hwnd, UINT message,
                                               WPARAM wparam, LPARAM lparam,
                                               UINT_PTR subclass_id,
                                               DWORD_PTR ref_data) noexcept {
    auto* sl = reinterpret_cast<Slider*>(ref_data);
    switch (message) {
    case WM_PAINT: {
        // CP25: chrome proc runs FIRST (last-installed-runs-first). Paint
        // the themed chrome ourselves and return 0 so DefSubclassProc does
        // not chain into the native TRACKBAR pass (which would draw TBS_
        // TRANSPARENT native chrome on top of our themed fill).
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (dc != nullptr) {
            if (sl != nullptr) {
                sl->paint_chrome(dc);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        // Suppress native erase — paint_chrome paints the background.
        return 1;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        // Focus change toggles the focus ring; invalidate so the chrome
        // repaints against the new focus state.
        InvalidateRect(hwnd, nullptr, FALSE);
        return r;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &Slider::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui

#include <nfui/Controls/StatusBar.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>

#include <algorithm>
#include <commctrl.h>

namespace nfui {

bool StatusBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams status_params = params;
    status_params.style = WS_CHILD | WS_VISIBLE;
    if (!create_native(STATUSCLASSNAMEW, status_params, SBARS_SIZEGRIP)) {
        return false;
    }
    // CP23: install the chrome subclass AFTER create_native. SetWindowSubclass
    // chains onto whatever is already installed and dispatches in REVERSE
    // install order — the LAST installed subclass runs FIRST. So our chrome
    // proc runs before the base Control::subclass proc and short-circuits
    // WM_PAINT (returns 0) so the native ComCtl32 status-bar chrome never
    // paints pale grey SBARS_SIZEGRIP dots and part backgrounds on top of
    // our themed fill. For non-WM_PAINT messages we fall through to
    // DefSubclassProc, which reaches the base's chain entry.
    if (SetWindowSubclass(hwnd(), &StatusBar::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void StatusBar::set_style(FrameStyle style) noexcept {
    style_ = style;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

// CP23: paint the full bar chrome into `dc` covering the control's client
// rect. Reads live text from the native status bar via SB_GETTEXT(W|LENGTH)
// so callers that set text via SB_SETTEXTW continue to see it in our paint.
// Paints: background, top hairline, part text (in palette.text), and a
// three-row diagonal grip (replaces native SBARS_SIZEGRIP dots that read as
// COLOR_BTNTEXT in dark mode).
void StatusBar::paint_chrome(HDC dc) noexcept {
    if (dc == nullptr) return;
    HWND bar_hwnd = hwnd();
    if (bar_hwnd == nullptr) return;

    const ThemePalette* pal_ptr = palette();
    const ThemePalette& p = pal_ptr ? *pal_ptr : theme_palette(ThemeMode::light);

    RECT bounds{};
    GetClientRect(bar_hwnd, &bounds);
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;

    // CP23: scale the layout offsets by the live DPI so the part text
    // padding, hairline height, and grip reservation all hold constant in
    // logical units across DPI. Without scaling, high-DPI layouts leave
    // the grip sitting inside the text rect and the hairline collapses to
    // a sub-pixel stripe.
    const DpiScale dpi{dpi_of(bar_hwnd)};
    const int pad_left = dpi.logical_to_pixels(6);
    const int pad_top = dpi.logical_to_pixels(1);
    const int grip = dpi.logical_to_pixels(16);
    const int cell = dpi.logical_to_pixels(3);
    const int gap = dpi.logical_to_pixels(2);
    const int hairline_h = std::max(dpi.logical_to_pixels(1), 1);

    const Color fill = style().surface_brush.value_or(p.surface);

    MemoryDC mem(dc, bounds);
    HDC target = mem.valid() ? mem.dc() : dc;
    // CP23: in the offscreen path, paint_bounds is (0, 0, w, h) because
    // MemoryDC origin is at the top-left of the memory bitmap. In the
    // direct-DC fallback path, GetClientRect already returned client-relative
    // coords (left == top == 0), so using bounds directly is also (0, 0, w, h).
    // Both branches now use the same (0, 0, w, h) rect, eliminating the
    // historical asymmetry flagged by the CP23 correctness review.
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const RECT paint_bounds{0, 0, width, height};
    fill_rect(target, paint_bounds, fill);

    // Top hairline so the bar reads as a distinct band against the window
    // chrome above it.
    RECT hairline = paint_bounds;
    hairline.bottom = hairline.top + hairline_h;
    fill_rect(target, hairline, p.border);

    // Read live text via SB_GETTEXTW. The two-step call (length then copy)
    // matches how ComCtl32 v6 status bars expect to be queried; callers that
    // set text via SB_SETTEXTW continue to see their content.
    //
    // CP23 scope: this implementation paints part 0 only. Multi-part status
    // bars set up via SB_SETPARTS + SB_SETTEXTW(part, ...) will only show
    // the first part's text in our paint; the native control would render
    // all parts. Document this in the header so callers know to either keep
    // the bar single-part or extend StatusBar with a multi-part paint path.
    LRESULT text_len = SendMessageW(bar_hwnd, SB_GETTEXTLENGTHW, 0, 0);
    std::wstring text;
    if (HIWORD(text_len) != 0) {
        const int chars = LOWORD(text_len);
        text.resize(static_cast<std::size_t>(chars) + 1);
        LRESULT copied = SendMessageW(bar_hwnd, SB_GETTEXTW, 0,
                                       reinterpret_cast<LPARAM>(text.data()));
        if (copied == 0) {
            text.clear();
        } else {
            while (!text.empty() && text.back() == L'\0') {
                text.pop_back();
            }
        }
    }

    // Part rect: full client area minus the size-grip column.
    RECT part_rect = paint_bounds;
    part_rect.left += pad_left;
    part_rect.right -= grip;
    part_rect.top += pad_top;

    if (!text.empty() && fonts() != nullptr) {
        const int dpi_value = dpi_of(bar_hwnd);
        HFONT font = fonts()->semibold(dpi_value, font_pt::ui);
        draw_text(target,
                  part_rect,
                  text,
                  font,
                  style().foreground.value_or(p.text),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // Size grip — three diagonal rows of dots in palette.border. Replaces
    // the native SBARS_SIZEGRIP chevrons (which paint in COLOR_BTNTEXT and
    // read as a pale patch in dark mode).
    const int base_x = paint_bounds.right - grip + gap;
    const int base_y = paint_bounds.bottom - grip + gap;
    for (int row = 0; row < 3; ++row) {
        const int y = base_y + row * (cell + gap);
        for (int col = 0; col <= row; ++col) {
            const int x = base_x + col * (cell + gap);
            RECT dot{ x, y, x + cell, y + cell };
            fill_rect(target, dot, p.border);
        }
    }
}

void StatusBar::on_paint(HDC dc, const PaintState& state) noexcept {
    // Defensive: the chrome subclass should already have painted by the time
    // the base Control::subclass_proc forwards here. If for any reason
    // WM_PAINT reaches on_paint directly (e.g. direct call from a custom
    // caller), fall back to paint_chrome so the visual contract holds.
    (void)state;
    paint_chrome(dc);
}

LRESULT CALLBACK StatusBar::visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept {
    auto* sb = reinterpret_cast<StatusBar*>(ref_data);
    switch (message) {
    case WM_PAINT: {
        // CP23: chrome proc runs FIRST (last-installed-runs-first), so we
        // intercept WM_PAINT and paint the full themed chrome. Returning 0
        // (instead of chaining DefSubclassProc) suppresses the native
        // ComCtl32 status-bar pass that would otherwise paint pale grey
        // SBARS_SIZEGRIP dots and part chrome on top of our themed fill.
        // For all other messages we fall through to DefSubclassProc, which
        // reaches the base Control::subclass proc and from there the rest
        // of the chain.
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
        // Suppress native erase — paint_chrome paints the background.
        return 1;
    case WM_NCDESTROY:
        // Detach the chrome subclass before chaining. DefSubclassProc would
        // chain to the base, and the base's WM_NCDESTROY path will release
        // hwnd_ and detach its own subclass; we mirror that for symmetry and
        // so any future subclass removal utility doesn't leak our entry.
        RemoveWindowSubclass(hwnd, &StatusBar::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
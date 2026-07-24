#include <nfui/Controls/ProgressBar.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <commctrl.h>
#include <string>

namespace nfui {

namespace {

// CP24-B / CP28: paint the rounded track + filled portion into `dc` covering
// the bar's client rect. This is a static helper that takes the resolved
// palette, style, and CP28 state explicitly so the member paint_chrome can
// forward them — the chrome subclass proc is a free function and cannot
// reach the protected palette()/fonts() accessors directly.
void paint_progress_chrome(HDC dc, HWND bar_hwnd,
                            const ThemePalette& p,
                            const FrameStyle& style,
                            FontCache* fonts,
                            ProgressBarKind kind,
                            bool show_percentage,
                            bool enabled) noexcept {
    if (bar_hwnd == nullptr || dc == nullptr) return;

    RECT bounds{};
    GetClientRect(bar_hwnd, &bounds);
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;

    const DpiScale dpi{dpi_of(bar_hwnd)};
    const int radius = dpi.logical_to_pixels(theme_metrics().corner_radius_control);

    const Color track = style.surface_brush.value_or(p.surface);
    // CP8: disabled uses text_secondary for a fill that clears WCAG AA against
    // the track surface. Enabled fill resolves from semantic kind or the
    // style.bar_color override, falling back to palette.accent.
    Color fill = p.accent;
    if (!enabled) {
        fill = p.text_secondary;
    } else {
        switch (kind) {
        case ProgressBarKind::success: fill = p.success; break;
        case ProgressBarKind::warning: fill = p.warning; break;
        case ProgressBarKind::error:    fill = p.danger; break;
        case ProgressBarKind::accent:
        default:                        fill = style.bar_color.value_or(p.accent); break;
        }
    }

    // PBM_GETRANGE fills the low word of wParam via return; lParam is the
    // PPBRANGE. Capture range + position so the bar reflects the live control
    // state set via PBM_SETPOS / PBM_SETRANGE.
    PBRANGE range{};
    SendMessageW(bar_hwnd, PBM_GETRANGE, FALSE,
                 reinterpret_cast<LPARAM>(&range));
    const int pos = static_cast<int>(SendMessageW(bar_hwnd, PBM_GETPOS, 0, 0));
    const int span = range.iHigh - range.iLow;

    MemoryDC mem(dc, bounds);
    HDC target = mem.valid() ? mem.dc() : dc;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const RECT paint_bounds{0, 0, width, height};
    // CP8: RoundRect leaves outside-corner pixels untouched; clear them with
    // the window background so a theme swap cannot preserve a frame from the
    // previous palette in the rounded boundary (mirrors Button).
    fill_rect(target, paint_bounds, p.background);
    fill_rounded_rect(target, paint_bounds, radius, track, p.border);

    int fill_w = 0;
    if (span > 0 && pos > range.iLow && paint_bounds.right > paint_bounds.left) {
        fill_w = paint_bounds.right * (pos - range.iLow) / span;
        RECT bar{paint_bounds.left,
                 paint_bounds.top,
                 paint_bounds.left + fill_w,
                 paint_bounds.bottom};
        fill_rect(target, bar, fill);
    }

    // CP28: optional centered percentage label. The same text is drawn twice
    // under different clip regions so the portion sitting on the empty track
    // uses p.text and the portion sitting on the filled bar uses p.background,
    // giving a clear contrast against both surfaces.
    if (show_percentage && span > 0 && fonts != nullptr) {
        const int percent = std::clamp((100 * (pos - range.iLow)) / span, 0, 100);
        const std::wstring label = std::to_wstring(percent) + L'%';
        HFONT font = fonts->semibold(dpi.dpi(), font_pt::ui);
        if (font != nullptr) {
            const RECT text_bounds = paint_bounds;
            constexpr UINT text_format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;

            if (fill_w < paint_bounds.right) {
                const int saved = SaveDC(target);
                IntersectClipRect(target, fill_w, paint_bounds.top,
                                  paint_bounds.right, paint_bounds.bottom);
                draw_text(target, text_bounds, label, font, p.text, text_format);
                RestoreDC(target, saved);
            }
            if (fill_w > paint_bounds.left) {
                const int saved = SaveDC(target);
                IntersectClipRect(target, paint_bounds.left, paint_bounds.top,
                                  fill_w, paint_bounds.bottom);
                draw_text(target, text_bounds, label, font, p.background, text_format);
                RestoreDC(target, saved);
            }
        }
    }
}

} // namespace

bool ProgressBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams pb_params = params;
    pb_params.style = WS_CHILD | WS_VISIBLE;
    if (!create_native(PROGRESS_CLASSW, pb_params, WS_CLIPCHILDREN)) {
        return false;
    }
    // CP24-B: install the chrome subclass AFTER create_native. SetWindowSubclass
    // dispatches in REVERSE install order (last-installed-runs-first); our
    // chrome proc intercepts WM_PAINT FIRST and short-circuits the native
    // PBS_SMOOTH pass so the themed fill is the only paint ever visible.
    if (SetWindowSubclass(hwnd(), &ProgressBar::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void ProgressBar::set_style(FrameStyle style) noexcept {
    style_ = style;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void ProgressBar::set_kind(ProgressBarKind kind) noexcept {
    kind_ = kind;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void ProgressBar::set_show_percentage(bool show) noexcept {
    show_percentage_ = show;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void ProgressBar::paint_chrome(HDC dc) noexcept {
    if (dc == nullptr) return;
    HWND bar_hwnd = hwnd();
    if (bar_hwnd == nullptr) return;
    const ThemePalette* pal_ptr = palette();
    const ThemePalette& p = pal_ptr ? *pal_ptr : theme_palette(ThemeMode::light);
    const bool enabled = IsWindowEnabled(bar_hwnd) != FALSE;
    paint_progress_chrome(dc, bar_hwnd, p, style_, fonts(), kind_,
                          show_percentage_, enabled);
}

void ProgressBar::on_paint(HDC dc, const PaintState& state) noexcept {
    // Fallback path: if any caller dispatches paint directly (e.g. via
    // framework internals), paint via paint_chrome so the visual contract
    // holds. The chrome subclass normally intercepts WM_PAINT first.
    (void)state;
    paint_chrome(dc);
}

LRESULT CALLBACK ProgressBar::visual_subclass_proc(HWND hwnd, UINT message,
                                                   WPARAM wparam, LPARAM lparam,
                                                   UINT_PTR subclass_id,
                                                   DWORD_PTR ref_data) noexcept {
    auto* pb = reinterpret_cast<ProgressBar*>(ref_data);
    switch (message) {
    case WM_PAINT: {
        // CP24-B: chrome proc runs FIRST (last-installed-runs-first). Paint
        // the themed chrome ourselves and return 0 so DefSubclassProc does
        // not chain into the native ComCtl32 progress-bar pass (which would
        // draw PBS_SMOOTH / native chrome on top of our themed fill).
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (dc != nullptr) {
            if (pb != nullptr) {
                pb->paint_chrome(dc);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        // Suppress native erase — paint_chrome paints the background.
        return 1;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &ProgressBar::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui

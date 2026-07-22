#include <nfui/Controls/TabControl.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <commctrl.h>

namespace nfui {

namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

// CP20: theme tab fills via FrameStyle + palette. Active / hot / inactive get
// distinct colours so the tab strip reads as part of the themed chrome instead
// of an OS-default grey band. Returns the colour for the given item state.
Color tab_fill_color(const FrameStyle& style, const ThemePalette& p, bool selected, bool hot) noexcept {
    if (selected) {
        return style.background.value_or(p.surface);
    }
    if (hot) {
        return style.chrome_bg.value_or(p.surface_hover);
    }
    return style.chrome_bg.value_or(p.surface);
}

Color tab_text_color(const FrameStyle& style, const ThemePalette& p, bool selected) noexcept {
    if (selected) {
        return style.foreground.value_or(style.chrome_text.value_or(p.text));
    }
    return style.foreground.value_or(style.chrome_text.value_or(p.text_secondary));
}

} // namespace

bool TabControl::create(const ControlCreateParams& params) noexcept {
    if (!create_native(WC_TABCONTROLW, params, WS_CLIPSIBLINGS)) {
        return false;
    }
    // CP20: install the chrome subclass for NMCTCUSTOMDRAW. The base
    // Control::subclass_proc does not know about TabControl's NM_CUSTOMDRAW
    // structure (NMCUSTOMDRAW vs NMLVCUSTOMDRAW layout), so the chrome proc
    // owns the entire custom-draw path.
    if (SetWindowSubclass(hwnd(), &TabControl::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    on_palette_changed();
    return true;
}

void TabControl::on_palette_changed() noexcept {
    // The chrome subclass already pulls palette+FrameStyle from
    // handle_custom_draw on every paint cycle, so the only work here is to
    // invalidate so the system asks us to repaint tabs with the new theme.
    if (valid()) {
        InvalidateRect(hwnd(), nullptr, TRUE);
    }
}

bool TabControl::set_padding(int cx, int cy) noexcept {
    if (!valid()) {
        return false;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    SendMessageW(hwnd(), TCM_SETPADDING, 0, MAKELPARAM(static_cast<WORD>(cx), static_cast<WORD>(cy)));
    return true;
}

void TabControl::paint_tab(NMCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr || cd->hdc == nullptr) return;
    const ThemePalette& p = effective_palette(palette());
    const bool selected = (cd->uItemState & CDIS_SELECTED) != 0;
    const bool hot = (cd->uItemState & CDIS_HOT) != 0;
    const Color fill = tab_fill_color(style_, p, selected, hot);
    const Color border = p.border;
    // Rounded rect: the chrome group reads as soft, matching CP16 Panel /
    // Splitter / StatusBar language. Radius scales with DPI so the curve is
    // consistent across screen configurations.
    const int radius = (selected || hot) ? theme_metrics().corner_radius_control : 0;
    fill_rounded_rect(cd->hdc, cd->rc, radius, fill, border);

    // Caption: read via TCM_GETITEM so the per-tab label honours whatever the
    // application inserted at insert time. Pad inside the tab so the text does
    // not touch the border. dwItemSpec is the tab index — Win32 has no
    // reserved/sentinel value, so the first tab at index 0 is drawn with its
    // caption exactly like the rest. (Found and fixed by the CP20 adversarial
    // review — the prior `if (dwItemSpec == 0) return;` skipped the first tab.)
    wchar_t text[128]{};
    TCITEM tci{};
    tci.mask = TCIF_TEXT;
    tci.pszText = text;
    tci.cchTextMax = static_cast<int>(sizeof(text) / sizeof(text[0]));
    if (SendMessageW(hwnd(), TCM_GETITEM, cd->dwItemSpec,
                     reinterpret_cast<LPARAM>(&tci)) == FALSE) {
        return;
    }
    RECT text_rc = cd->rc;
    // CP21: padding is in logical pixels, scaled by DPI so 125/150/200%
    // monitors produce the same visual margin. At 96 dpi this is still 6 px.
    const DpiScale dpi{dpi_of(hwnd())};
    const int pad = dpi.logical_to_pixels(6);
    text_rc.left += pad;
    text_rc.right -= pad;
    HFONT font = fonts() ? fonts()->regular(dpi.dpi(), font_pt::ui) : nullptr;
    draw_text(cd->hdc, text_rc, text, font, tab_text_color(style_, p, selected),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // CP21: selected-tab accent edge along the page-facing (bottom) side,
    // matching the CP19 chrome focus-ring language. Kept to one edge so the
    // accent reads as a tab indicator rather than a full outline. The accent
    // is inset horizontally by `radius` so it stays inside the rounded
    // bottom corners of the selected tab — without this inset the accent
    // would paint 2 px accent-coloured "tabs" sticking out of the round,
    // found by the CP21 adversarial review.
    if (selected) {
        const int accent_h = dpi.logical_to_pixels(2);
        RECT accent = cd->rc;
        accent.top = accent.bottom - accent_h;
        accent.bottom = cd->rc.bottom;
        accent.left += radius;
        accent.right -= radius;
        if (accent.right > accent.left) {
            fill_rect(cd->hdc, accent, p.accent);
        }
    }
}

LRESULT TabControl::handle_custom_draw(NMCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        // Suppress the default tab paint and ask for a post-paint so we can
        // self-paint the fill + text with palette colours. The system keeps
        // drawing the page area / scroll arrows itself.
        return CDRF_SKIPDEFAULT | CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPOSTPAINT:
        paint_tab(cd);
        return CDRF_DODEFAULT;
    default:
        return CDRF_DODEFAULT;
    }
}

LRESULT CALLBACK TabControl::visual_subclass_proc(HWND hwnd, UINT message,
                                                   WPARAM wparam, LPARAM lparam,
                                                   UINT_PTR subclass_id,
                                                   DWORD_PTR ref_data) noexcept {
    auto* tc = reinterpret_cast<TabControl*>(ref_data);
    if (tc == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    if (message == ocm_base + WM_NOTIFY) {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr && nmh->code == NM_CUSTOMDRAW) {
            return tc->handle_custom_draw(reinterpret_cast<NMCUSTOMDRAW*>(lparam));
        }
    }
    switch (message) {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &TabControl::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui

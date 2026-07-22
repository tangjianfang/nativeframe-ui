#include <nfui/Controls/ListView.hpp>
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

} // namespace

bool ListView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style &= ~WS_BORDER;
    if (!create_native(WC_LISTVIEWW, p, LVS_REPORT | LVS_SINGLESEL)) {
        return false;
    }
    if (fonts() != nullptr) {
        if (HFONT f = fonts()->regular(dpi_of(hwnd()), font_pt::ui)) {
            SendMessageW(hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
        }
    }
    // CP2: chain on_palette_changed so the native empty-area background and
    // base text colour agree with the palette at create time, not just on
    // subsequent palette swaps. CP3: enable LVS_EX_TRACKSELECT so the OS
    // delivers CDIS_HOT for hover rows; our custom-draw item handler reads
    // that bit to paint the hover background.
    ListView_SetExtendedListViewStyle(hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT);

    // CP20: install the chrome subclass on the body and on the header window
    // (the header is a separate child HWND, addressed via ListView_GetHeader).
    if (SetWindowSubclass(hwnd(), &ListView::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    if (HWND header = ListView_GetHeader(hwnd())) {
        theme_header(header);
    }
    on_palette_changed();
    return true;
}

void ListView::on_palette_changed() noexcept {
    if (hwnd() == nullptr) {
        return;
    }
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal != nullptr ? *pal : theme_palette(ThemeMode::light);
    // Keep the empty-area native background and base text in sync with the
    // custom-draw item colors. The subclass hook redraws rows after this call.
    const Color bg = style_.row_background.value_or(p.surface);
    const Color fg = style_.row_foreground.value_or(p.text);
    ListView_SetBkColor(hwnd(), bg.rgb);
    ListView_SetTextColor(hwnd(), fg.rgb);
    // CP20: re-theme the header in case the palette swapped.
    if (HWND header = ListView_GetHeader(hwnd())) {
        theme_header(header);
        InvalidateRect(header, nullptr, TRUE);
    }
}

void ListView::theme_header(HWND header) noexcept {
    if (header == nullptr) return;
    // CP20: header background theming via WM_ERASEBKGND in the chrome subclass.
    // The Windows SDK does not define HDM_SETBKCOLOR for the Header control,
    // so the only reliable palette-driven background is to paint the band's
    // client area before the system draws the column rectangles. Header text
    // colour inherits the system default (no public HDM_SETTEXTCOLOR either).
    if (HFONT f = fonts() ? fonts()->semibold(dpi_of(hwnd()), font_pt::ui) : nullptr) {
        SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
    }
    // Install (or replace) the header chrome subclass. SetWindowSubclass on a
    // (proc, id) pair that is already installed updates the ref_data in place,
    // so this is safe to call on every palette swap without an explicit
    // "already installed?" probe.
    SetWindowSubclass(header, &ListView::header_subclass_proc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(this));
}

LRESULT ListView::on_custom_draw_item(NMLVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
    const bool hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
    // CP20: row chrome is fully driven by clrText / clrTextBk at PREPAINT —
    // the system honours them in normal LVS_REPORT + LVS_EX_FULLROWSELECT
    // usage. We deliberately do NOT add CDRF_NOTIFYPOSTPAINT here: a
    // post-paint fill_rect over the row rectangle would erase the icons and
    // subitem text the system just rendered with those colours, and any
    // divergence between the two computations (alpha blends, hover ramps)
    // leaves visible bands. (Found and fixed by the CP20 adversarial
    // review — see docs/KNOWLEDGE/polish/2026-07-23-cp20-listview-treeview-tab-chrome.md.)
    cd->clrText = selected
        ? style_.selected_foreground.value_or(p.selection_text).rgb
        : style_.row_foreground.value_or(p.text).rgb;
    cd->clrTextBk = selected
        ? style_.selected_background.value_or(p.selection).rgb
        : (hot ? p.surface_hover : style_.row_background.value_or(p.surface)).rgb;
    return CDRF_DODEFAULT;
}

LRESULT ListView::handle_custom_draw(NMLVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        const LRESULT custom = on_custom_draw_item(cd);
        if (custom != CDRF_DODEFAULT) {
            return custom;
        }
        return CDRF_DODEFAULT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

LRESULT CALLBACK ListView::visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept {
    auto* lv = reinterpret_cast<ListView*>(ref_data);
    if (lv == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    // CP20: intercept reflected NM_CUSTOMDRAW (ocm_base + WM_NOTIFY with code
    // NM_CUSTOMDRAW) before the base Control::subclass_proc sees it. The base
    // handles PREPAINT/ITEMPREPAINT; we keep the same boundary and let the
    // system honour clrText/clrTextBk without any post-paint overrides.
    if (message == ocm_base + WM_NOTIFY) {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr && nmh->code == NM_CUSTOMDRAW) {
            return lv->handle_custom_draw(reinterpret_cast<NMLVCUSTOMDRAW*>(lparam));
        }
    }
    switch (message) {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &ListView::visual_subclass_proc, subclass_id);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK ListView::header_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept {
    auto* lv = reinterpret_cast<ListView*>(ref_data);
    // CP20: theme the header band via WM_ERASEBKGND. The header is a
    // WC_HEADERW (a separate window from the ListView body) so this subclass
    // lives on its own HWND. The Windows SDK does not expose HDM_SETBKCOLOR,
    // and the header's NM_CUSTOMDRAW payload is the universal NMCUSTOMDRAW
    // (no clrText/clrTextBk), so painting the band background here before
    // the system draws the column rectangles is the only palette-driven path.
    // Header text colour inherits the system default — a future CP can
    // self-draw the caption pass via item-post-paint if the default reads
    // wrong against the palette surface.
    if (lv != nullptr && message == WM_ERASEBKGND) {
        const ThemePalette* pal = lv->palette();
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        HDC dc = reinterpret_cast<HDC>(wparam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        const Color bg = lv->style().row_background.value_or(p.surface);
        fill_rect(dc, rc, bg);
        return TRUE;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, &ListView::header_subclass_proc, subclass_id);
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
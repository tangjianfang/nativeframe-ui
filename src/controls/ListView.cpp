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
    // colour is self-painted in CP21 via NM_CUSTOMDRAW (no public HDM API).
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

void ListView::paint_header_item(HWND header, NMCUSTOMDRAW* cd) noexcept {
    if (header == nullptr || cd == nullptr || cd->hdc == nullptr) return;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool hot = (cd->uItemState & CDIS_HOT) != 0;
    const bool disabled = (cd->uItemState & CDIS_DISABLED) != 0;

    // CP21: fill the column rect (CDRF_SKIPDEFAULT suppressed native paint),
    // draw a 1px right-edge divider, then the caption. Background uses the
    // palette surface so dark mode is readable; hot items use surface_hover.
    const Color fill = hot
        ? p.surface_hover
        : style_.row_background.value_or(p.surface);
    fill_rect(cd->hdc, cd->rc, fill);
    RECT divider = cd->rc;
    divider.left = divider.right - 1;
    fill_rect(cd->hdc, divider, p.border);

    // Item metadata: caption text + format flags + sort indicator.
    wchar_t text[256]{};
    HDITEMW item{};
    item.mask = HDI_TEXT | HDI_FORMAT;
    item.pszText = text;
    item.cchTextMax = static_cast<int>(std::size(text));
    const int index = static_cast<int>(cd->dwItemSpec);
    if (SendMessageW(header, HDM_GETITEMW, index,
                     reinterpret_cast<LPARAM>(&item)) == FALSE) {
        return;
    }

    // CP21: reserve a DPI-scaled right-edge box for the sort glyph so the
    // caption never overlaps it.
    const DpiScale dpi{dpi_of(header)};
    const int sort_gap = dpi.logical_to_pixels(6);
    const int sort_size = dpi.logical_to_pixels(8);
    const bool has_sort = (item.fmt & (HDF_SORTUP | HDF_SORTDOWN)) != 0;
    const bool sort_down = (item.fmt & HDF_SORTDOWN) != 0;

    RECT text_rc = cd->rc;
    const int pad = dpi.logical_to_pixels(6);
    text_rc.left += pad;
    text_rc.right -= pad;
    if (has_sort) {
        text_rc.right -= (sort_gap + sort_size);
    }

    UINT dt = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    switch (item.fmt & HDF_JUSTIFYMASK) {
    case HDF_CENTER: dt |= DT_CENTER; break;
    case HDF_RIGHT:  dt |= DT_RIGHT;  break;
    default:         dt |= DT_LEFT;   break;
    }
    if (item.fmt & HDF_RTLREADING) {
        dt |= DT_RTLREADING;
    }

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(header, WM_GETFONT, 0, 0));
    const Color fg = disabled
        ? p.text_secondary
        : style_.row_foreground.value_or(p.text);
    draw_text(cd->hdc, text_rc, text, font, fg, dt);

    // Sort triangle on the trailing edge — replaces the native HDF_SORT* glyph
    // that CDRF_SKIPDEFAULT suppresses. Triangle points down for SORTDOWN and
    // up for SORTUP, filled with the same colour as the caption.
    if (has_sort) {
        POINT tri[3]{};
        // CP21: anchor the sort glyph inside the trailing box
        // [rc.right - sort_gap - sort_size, rc.right - sort_gap]. The previous
        // version placed cx at rc.right - sort_gap + sort_size/2, which pushed
        // the triangle's right vertex past the column's right edge (and onto
        // the divider / next column) — found by the CP21 adversarial review.
        const int right = cd->rc.right - sort_gap;
        const int cx = right - sort_size / 2;
        const int cy = cd->rc.top + (cd->rc.bottom - cd->rc.top) / 2;
        if (sort_down) {
            tri[0] = {cx - sort_size / 2, cy - sort_size / 4};
            tri[1] = {cx + sort_size / 2, cy - sort_size / 4};
            tri[2] = {cx,                  cy + sort_size / 4};
        } else {
            tri[0] = {cx - sort_size / 2, cy + sort_size / 4};
            tri[1] = {cx + sort_size / 2, cy + sort_size / 4};
            tri[2] = {cx,                  cy - sort_size / 4};
        }
        fill_polygon(cd->hdc, tri, 3, fg, fg);
    }
}

LRESULT ListView::handle_header_custom_draw(HWND header, NMCUSTOMDRAW* cd) noexcept {
    if (header == nullptr || cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->dwDrawStage) {
    case CDDS_PREPAINT:
        // Request per-item notifications so we can paint each column with
        // its own palette state and format (alignment, sort glyph).
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        // Suppress the default header paint so we can paint the whole column
        // (background + divider + caption + sort glyph) in CDDS_ITEMPOSTPAINT.
        // The header's NM_CUSTOMDRAW payload is the universal NMCUSTOMDRAW
        // (no clrText/clrTextBk), so a pure PREPAINT colour override is not
        // available — full self-paint is the only palette-driven path.
        return CDRF_SKIPDEFAULT | CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPOSTPAINT:
        paint_header_item(header, cd);
        return CDRF_DODEFAULT;
    default:
        return CDRF_DODEFAULT;
    }
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
    // CP20: theme the header band via WM_ERASEBKGND. CP21: also intercept
    // reflected NM_CUSTOMDRAW (ocm_base + WM_NOTIFY) so column captions,
    // hot state, and sort glyphs are palette-driven. The header is a
    // WC_HEADERW (a separate window from the ListView body) so this subclass
    // lives on its own HWND. The Windows SDK does not expose HDM_SETBKCOLOR
    // or HDM_SETTEXTCOLOR, and the header's NM_CUSTOMDRAW payload is the
    // universal NMCUSTOMDRAW (no clrText/clrTextBk), so WM_ERASEBKGND +
    // full item self-paint in CDDS_ITEMPOSTPAINT are the only palette paths.
    if (lv != nullptr && message == ocm_base + WM_NOTIFY) {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr &&
            nmh->hwndFrom == hwnd &&
            nmh->code == NM_CUSTOMDRAW) {
            return lv->handle_header_custom_draw(
                hwnd, reinterpret_cast<NMCUSTOMDRAW*>(lparam));
        }
    }
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
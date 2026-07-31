#include <nfui/Controls/ListView.hpp>
#include <nfui/Controls/Detail/effective_palette.hpp>
#include <nfui/Controls/Scrollbar.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/ThemeBroker.hpp>
#include <commctrl.h>

namespace nfui {

namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

// CP-A3: ListView rows resolve their fill from the per-state palette so
// every state (rest/hover/pressed/focused/disabled/error) is driven from
// one helper instead of six hand-rolled paths in the custom-draw handler.
// Selection overrides fill + text via the explicit selected_* style slots.
Color row_background_for(const ThemePalette& base, ControlState state,
                         const ListViewStyle& style) noexcept {
    const StatePalette sp = state_palette(base, state);
    return style.row_background.value_or(sp.background);
}

Color row_foreground_for(const ThemePalette& base, ControlState state,
                         const ListViewStyle& style) noexcept {
    const StatePalette sp = state_palette(base, state);
    return style.row_foreground.value_or(sp.foreground);
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
    // CP42: disable system theming so our palette-driven background colours
    // are not overwritten by the native comctl32 theme (dark/HC white islands).
    theme_disable_window_theme(hwnd());
    if (SetWindowSubclass(hwnd(), &ListView::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    if (HWND header = ListView_GetHeader(hwnd())) {
        theme_disable_window_theme(header);
        theme_header(header);
    }
    // CP-A3: register this HWND + the header with the ThemeBroker so a
    // cross-window theme switch invalidates the chrome on the same
    // message-loop turn. Unregistered in the WM_NCDESTROY arm of the
    // subclass proc below. Capture the HWND by value so the lambda is
    // self-contained and does not need member access during broadcast.
    const HWND self_hwnd = hwnd();
    ThemeBroker::instance().register_hwnd(self_hwnd,
        [self_hwnd](ThemeMode) {
            if (IsWindow(self_hwnd) == FALSE) return;
            InvalidateRect(self_hwnd, nullptr, TRUE);
            if (HWND hdr = ListView_GetHeader(self_hwnd)) {
                InvalidateRect(hdr, nullptr, TRUE);
            }
        });
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
    // CP20: re-theme the header in case the palette swapped. CP42: also
    // re-disable system theming in case a later external call re-applied it.
    theme_disable_window_theme(hwnd());
    if (HWND header = ListView_GetHeader(hwnd())) {
        theme_disable_window_theme(header);
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
    const bool selected = (cd->uItemState & CDIS_SELECTED) != 0;

    // CP-A3: header band background lifts halfway between palette.surface and
    // palette.background so it reads as a distinct structural layer against
    // the row strip below. The brief specifies `alpha_blend(surface,
    // background, 0.5)` — paint::alpha_blend() composites src over dst, so
    // `alpha_blend(surface, background, 0.5)` resolves to a 50/50 mix of
    // surface and background. header_background style override wins when a
    // consumer has explicitly tinted the band.
    const Color default_header = alpha_blend(p.surface, p.background, 0.5f);
    const Color base_bg = style_.header_background.value_or(default_header);
    const Color tint = hot ? p.surface_hover : base_bg;
    fill_rect(cd->hdc, cd->rc, tint);
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
    const int pad = dpi.logical_to_pixels(8);
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

    // CP26: caption colour + face. The previous version read the font via
    // WM_GETFONT — which returned whatever Tahoma-ish face the native header
    // was holding at create time, *not* the Segoe UI Semibold that
    // theme_header() installed via WM_SETFONT. The Semibold face is what
    // gives the header the visual weight that distinguishes it from body
    // rows; without it the caption reads as "another list row", which was
    // the user-reported dark-mode "header looks dim" symptom. Pull the
    // cached Semibold HFONT directly so the chrome paint is the source of
    // truth, not whatever the native header happens to be holding. If the
    // FontCache is missing (uncommon — Control::inject_theme wires it
    // up), fall back to WM_GETFONT.
    HFONT font = (fonts() != nullptr)
        ? fonts()->semibold(dpi_of(header), font_pt::ui)
        : nullptr;
    if (font == nullptr) {
        font = reinterpret_cast<HFONT>(SendMessageW(header, WM_GETFONT, 0, 0));
    }
    // CP-A3: caption colour is text_secondary by default (per the brief) so
    // the header reads as a quieter layer than the body rows (which use
    // text). header_caption style override wins when a consumer wants the
    // caption emphasised. Sort glyph inherits the same colour.
    const Color fg = disabled
        ? p.text_secondary
        : style_.header_caption.value_or(p.text_secondary);
    draw_text(cd->hdc, text_rc, text, font, fg, dt);

    // CP-A3: sort triangle on the trailing edge — replaces the native
    // HDF_SORT* glyph that CDRF_SKIPDEFAULT suppresses. Triangle points down
    // for SORTDOWN and up for SORTUP, filled with the same colour as the
    // caption. When the column is the "selected" sort column (passed via
    // CDIS_SELECTED on the header item), the glyph inherits accent for
    // emphasis.
    if (has_sort) {
        POINT tri[3]{};
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
        const Color sort_color = selected ? p.accent : fg;
        fill_polygon(cd->hdc, tri, 3, sort_color, sort_color);
    }
}

LRESULT ListView::handle_header_custom_draw(HWND header, NMCUSTOMDRAW* cd) noexcept {
    if (header == nullptr || cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->dwDrawStage) {
    case CDDS_PREPAINT: {
        // Paint the full header band background first so the empty area
        // beyond the rightmost column is not left as the native/theme colour
        // (which reads as a white island in dark/HC captures).
        const ThemePalette* pal = palette();
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        const Color bg = style().row_background.value_or(p.surface);
        fill_rect(cd->hdc, cd->rc, bg);
        // Request per-item notifications so we can paint each column with
        // its own palette state and format (alignment, sort glyph). Skip the
        // default header band paint so the native/theme background does not
        // overwrite our fill in the empty area beyond the rightmost column.
        return CDRF_SKIPDEFAULT | CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT:
        // Suppress the default header paint and self-paint the whole column
        // (background + divider + caption + sort glyph) while the item HDC is
        // current. The header's NM_CUSTOMDRAW payload is the universal
        // NMCUSTOMDRAW (no clrText/clrTextBk), so a pure PREPAINT colour
        // override is not available — full self-paint is the only palette
        // path. POSTPAINT is not reliably delivered by the Header control,
        // so all drawing happens here.
        paint_header_item(header, cd);
        return CDRF_SKIPDEFAULT;
    case CDDS_ITEMPOSTPAINT:
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
    const bool focused = (cd->nmcd.uItemState & CDIS_FOCUS) != 0;
    // CP-B17: even-indexed rows in the rest state read surface_variant
    // (the "elevated / nested" surface) so adjacent rows register as
    // distinct chrome bands without a hard rule line.
    const bool alt_row = (cd->nmcd.dwItemSpec % 2) != 0;
    // CP-A3: row chrome is fully driven by clrText / clrTextBk at PREPAINT —
    // the system honours them in normal LVS_REPORT + LVS_EX_FULLROWSELECT
    // usage. We deliberately do NOT add CDRF_NOTIFYPOSTPAINT here: a
    // post-paint fill_rect over the row rectangle would erase the icons and
    // subitem text the system just rendered with those colours, and any
    // divergence between the two computations (alpha blends, hover ramps)
    // leaves visible bands. (Found and fixed by the CP20 adversarial
    // review — see docs/KNOWLEDGE/polish/2026-07-23-cp20-listview-treeview-tab-chrome.md.)
    //
    // CP-A3: selection wins over every other state — a selected row's text
    // and background always come from selection_text / selection (or the
    // matching style overrides). When the control is also focused, the
    // CDIS_FOCUS bit lets us paint a 2px focus ring around the row via
    // paint_focus_border at ITEMPOSTPAINT (see handle_custom_draw).
    if (selected) {
        cd->clrText     = style_.selected_foreground.value_or(p.selection_text).rgb;
        cd->clrTextBk   = style_.selected_background.value_or(p.selection).rgb;
    } else {
        const ControlState state = hot ? ControlState::hover : ControlState::rest;
        cd->clrText   = row_foreground_for(p, state, style_).rgb;
        // CP-B17: alt rows in the rest state read palette.surface_variant
        // (the elevated / nested surface) so adjacent rows register as
        // distinct chrome bands. Hover / pressed / disabled keep their
        // per-state fills regardless of row parity.
        if (alt_row && state == ControlState::rest && !style_.row_background.has_value()) {
            cd->clrTextBk = p.surface_variant.rgb;
        } else {
            cd->clrTextBk = row_background_for(p, state, style_).rgb;
        }
    }
    // CP-A3: ask for ITEMPOSTPAINT only when a focus ring is actually
    // needed (selected + focused). Other items return CDRF_DODEFAULT so
    // the system does not deliver a useless post-paint cycle.
    return (selected && focused) ? (CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT)
                                 : CDRF_DODEFAULT;
}

LRESULT ListView::handle_custom_draw(NMLVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
        // CP20: fill the full client background so the empty area (right of
        // the rightmost column / below the last row) matches the palette.
        // Without this, comctl32's native/theme background can read as a white
        // island in dark/HC captures.
        const ThemePalette* pal = palette();
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        const Color bg = style().row_background.value_or(p.surface);
        fill_rect(cd->nmcd.hdc, cd->nmcd.rc, bg);
        // Ask for per-item notifications so on_custom_draw_item can drive the
        // row text/item colours. The empty-area background is now handled by
        // ListView_SetBkColor after theme_disable_window_theme removed the
        // native theme override; this fill is a defensive guard.
        //
        // CP-B19: also opt in to POSTPAINT so we can paint the themed
        // scrollbar thumb on top of the native SCROLLBAR chrome. The native
        // thumb is left visible underneath because its colour is the OS
        // theme; our thumb is alpha-blended over it at 60%, producing a
        // readable elevator in every theme.
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPREPAINT: {
        const LRESULT custom = on_custom_draw_item(cd);
        if (custom != CDRF_DODEFAULT) {
            return custom;
        }
        return CDRF_DODEFAULT;
    }
    case CDDS_ITEMPOSTPAINT: {
        // CP-A3: stroke-only focus ring around a focused + selected row.
        // Mirrors the TreeView pattern (see TreeView::handle_custom_draw)
        // so a keyboard user can see which row arrow-keys will activate
        // even when the ListView itself is not the foreground window.
        const ThemePalette* pal = palette();
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        const StatePalette sp = state_palette(p, ControlState::focused);
        paint_focus_border(cd->nmcd.hdc, cd->nmcd.rc, sp.accent, 2);
        return CDRF_DODEFAULT;
    }
    case CDDS_POSTPAINT: {
        // CP-B19: forward the scrollbar thumb paint. The native SCROLLBAR
        // chrome is still visible underneath; paint_thumb_into composites
        // the rounded accent over it. We only repaint when there is at
        // least one scrollable pixel — GetScrollInfo returns range 0,0
        // when nothing is scrollable, and painting into that band would
        // waste a draw cycle.
        const ThemePalette* pal = palette();
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        const DpiScale dpi{dpi_of(hwnd())};
        const int sb_w = dpi.logical_to_pixels(16);  // SM_CXVSCROLL = 16 logical px
        const int sb_h = dpi.logical_to_pixels(16);  // SM_CYHSCROLL = 16 logical px
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
        if (GetScrollInfo(hwnd(), SB_VERT, &si) != 0
            && si.nMax > si.nMin
            && (si.nMax - si.nMin) > static_cast<int>(si.nPage)) {
            RECT track = cd->nmcd.rc;
            track.left = track.right - sb_w;
            // CP-B19: paint_thumb_into takes the absolute (min, max, pos)
            // triple from GetScrollInfo and resolves thumb geometry itself.
            Scrollbar::paint_thumb_into(cd->nmcd.hdc, track, true,
                                        si.nPos, si.nMin, si.nMax, p);
        }
        if (GetScrollInfo(hwnd(), SB_HORZ, &si) != 0
            && si.nMax > si.nMin
            && (si.nMax - si.nMin) > static_cast<int>(si.nPage)) {
            RECT track = cd->nmcd.rc;
            track.top = track.bottom - sb_h;
            Scrollbar::paint_thumb_into(cd->nmcd.hdc, track, false,
                                        si.nPos, si.nMin, si.nMax, p);
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
    // CP20: the header is a separate child HWND; its WM_NOTIFY is delivered to
    // the ListView (its immediate parent), not to the top-level Window. Reflect
    // it to the header chrome subclass so NM_CUSTOMDRAW based header painting
    // fires in LVS_REPORT.
    if (message == WM_NOTIFY) {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr) {
            if (HWND header = ListView_GetHeader(hwnd);
                header != nullptr && nmh->hwndFrom == header) {
                return SendMessageW(header, ocm_base + WM_NOTIFY, wparam, lparam);
            }
        }
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
        ThemeBroker::instance().unregister_hwnd(hwnd);
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
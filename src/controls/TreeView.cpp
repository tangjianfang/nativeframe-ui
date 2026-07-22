#include <nfui/Controls/TreeView.hpp>
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

bool TreeView::create(const ControlCreateParams& params) noexcept {
    // CP20: TVS_TRACKSELECT enables CDIS_HOT delivery so the custom-draw hook
    // can paint a hover background on the row under the cursor. WS_BORDER is
    // kept (consistent with ListView's borderless choice) so the focus ring
    // reads against the parent surface instead of a system border frame.
    const DWORD extra = WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_TRACKSELECT;
    if (!create_native(WC_TREEVIEWW, params, extra)) {
        return false;
    }
    if (fonts() != nullptr) {
        if (HFONT f = fonts()->regular(dpi_of(hwnd()), font_pt::ui)) {
            SendMessageW(hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
        }
    }
    // CP20: install the chrome subclass. The base Control::subclass_proc does
    // not know about NMTVCUSTOMDRAW (it would reinterpret the payload as
    // NMLVCUSTOMDRAW), so the chrome proc owns the entire custom-draw path.
    if (SetWindowSubclass(hwnd(), &TreeView::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    on_palette_changed();
    return true;
}

void TreeView::on_palette_changed() noexcept {
    if (hwnd() == nullptr) {
        return;
    }
    const ThemePalette& p = effective_palette(palette());
    // TVM_SET*COLOR sets persist across palette swaps until explicitly changed.
    // Re-applying on every swap keeps the empty-area background, base text
    // colour, and indent-guide lines in sync with the palette. Custom-draw
    // row fills take priority for selection/hover; these are the non-row chrome.
    const Color bg = style_.row_background.value_or(p.surface);
    const Color fg = style_.row_foreground.value_or(p.text);
    const Color line = style_.line_color.value_or(p.border);
    TreeView_SetBkColor(hwnd(), bg.rgb);
    TreeView_SetTextColor(hwnd(), fg.rgb);
    TreeView_SetLineColor(hwnd(), line.rgb);
}

LRESULT TreeView::handle_custom_draw(NMTVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        // CP24-C: also ask for POSTPAINT so the focused row gets a focus
        // ring drawn AFTER the system paints text (so the ring sits on top
        // of the selection background, not under it). The ring is a 1-px
        // stroke-only rectangle inset from the row rect; we use
        // paint_focus_border to match the focus ring used by other chrome
        // (Button / Showcase / etc.) so the visual contract is uniform.
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPREPAINT: {
        const ThemePalette& p = effective_palette(palette());
        const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
        const bool hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
        // CP20: row chrome is fully driven by clrText / clrTextBk at PREPAINT.
        // We do NOT fill_rect at ITEMPOSTPAINT — that would erase the indent
        // guides, +/- glyph, and item text the system just rendered. The
        // CP24-C focus ring is stroke-only via paint_focus_border, so it
        // draws on top of the system-rendered chrome without overwriting
        // it. (Found and fixed by the CP20 adversarial review — see
        // docs/KNOWLEDGE/polish/2026-07-23-cp20-listview-treeview-tab-chrome.md.)
        cd->clrText = selected
            ? style_.selected_foreground.value_or(p.selection_text).rgb
            : style_.row_foreground.value_or(p.text).rgb;
        cd->clrTextBk = selected
            ? style_.selected_background.value_or(p.selection).rgb
            : (hot ? p.surface_hover.rgb : CLR_NONE);
        return CDRF_DODEFAULT;
    }
    case CDDS_ITEMPOSTPAINT: {
        // CP24-C: draw a stroke-only focus ring around the focused row. We
        // honour CDIS_FOCUS (the TreeView's "focused item" state, distinct
        // from CDIS_SELECTED) so a keyboard user can see which row arrow-
        // keys will activate, even when the control itself is not the
        // foreground window. Stroke-only via paint_focus_border means the
        // selection fill + text stay visible underneath.
        const ThemePalette& p = effective_palette(palette());
        const bool focused = (cd->nmcd.uItemState & CDIS_FOCUS) != 0;
        if (focused) {
            paint_focus_border(cd->nmcd.hdc, cd->nmcd.rc, p.accent, 1);
        }
        return CDRF_DODEFAULT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

LRESULT CALLBACK TreeView::visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept {
    auto* tv = reinterpret_cast<TreeView*>(ref_data);
    if (tv == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    if (message == ocm_base + WM_NOTIFY) {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr && nmh->code == NM_CUSTOMDRAW) {
            return tv->handle_custom_draw(reinterpret_cast<NMTVCUSTOMDRAW*>(lparam));
        }
    }
    switch (message) {
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &TreeView::visual_subclass_proc, subclass_id);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui

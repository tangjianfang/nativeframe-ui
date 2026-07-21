#include <nfui/Controls/ListView.hpp>
#include <nfui/Dpi.hpp>
#include <commctrl.h>

namespace nfui {

bool ListView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style &= ~WS_BORDER;
    if (!create_native(WC_LISTVIEWW, p, LVS_REPORT | LVS_SINGLESEL)) {
        return false;
    }
    if (fonts() != nullptr) {
        if (HFONT f = fonts()->regular(dpi_of(hwnd()), 9)) {
            SendMessageW(hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
        }
    }
    ListView_SetExtendedListViewStyle(hwnd(), LVS_EX_FULLROWSELECT);
    // P1.4: when a palette is injected and the user has supplied chrome_bg /
    // chrome_text, push those into the ListView control's native base colours
    // via the documented ComCtl32 ListView_SetBkColor / ListView_SetTextColor
    // helpers. The custom-draw handler below still wins per-item, so this
    // just guarantees the empty-area background and the base text colour
    // agree with the palette even when no items are present.
    const ThemePalette* pal = palette();
    if (pal != nullptr) {
        const Color bg = style_.row_background.value_or(pal->surface);
        const Color fg = style_.row_foreground.value_or(pal->text);
        ListView_SetBkColor(hwnd(), bg.rgb);
        ListView_SetTextColor(hwnd(), fg.rgb);
    }
    return true;
}

LRESULT ListView::on_custom_draw_item(NMLVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
    cd->clrText = selected
        ? style_.selected_foreground.value_or(p.selection_text).rgb
        : style_.row_foreground.value_or(p.text).rgb;
    cd->clrTextBk = selected
        ? style_.selected_background.value_or(p.selection).rgb
        : style_.row_background.value_or(p.surface).rgb;
    return CDRF_DODEFAULT;
}

} // namespace nfui

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

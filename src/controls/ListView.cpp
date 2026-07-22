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
// CP2: chain on_palette_changed so the native empty-area background and
    // base text colour agree with the palette at create time, not just on
    // subsequent palette swaps. CP3: enable LVS_EX_TRACKSELECT so the OS
    // delivers CDIS_HOT for hover rows; our custom-draw item handler reads
    // that bit to paint the hover background.
    ListView_SetExtendedListViewStyle(hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT);
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
}

LRESULT ListView::on_custom_draw_item(NMLVCUSTOMDRAW* cd) noexcept {
    if (cd == nullptr) return CDRF_DODEFAULT;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
    const bool hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
    cd->clrText = selected
        ? style_.selected_foreground.value_or(p.selection_text).rgb
        : style_.row_foreground.value_or(p.text).rgb;
    cd->clrTextBk = selected
        ? style_.selected_background.value_or(p.selection).rgb
        : (hot ? p.surface_hover : style_.row_background.value_or(p.surface)).rgb;
    return CDRF_DODEFAULT;
}

} // namespace nfui

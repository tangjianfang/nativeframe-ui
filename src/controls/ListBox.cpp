#include <nfui/Controls/ListBox.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <windowsx.h>

namespace nfui {

bool ListBox::create(const ControlCreateParams& params) noexcept {
    if (!create_native(L"LISTBOX", params, WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL)) return false;
    const int row = font_pixel_height(9, dpi_of(hwnd())) + 8;
    SendMessageW(hwnd(), LB_SETITEMHEIGHT, 0, static_cast<LPARAM>(row));
    return true;
}

void ListBox::draw_item(DRAWITEMSTRUCT* di) noexcept {
    if (di == nullptr) return;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool selected = (di->itemState & ODS_SELECTED) != 0;
    const bool disabled = (di->itemState & ODS_DISABLED) != 0;
    RECT rc = di->rcItem;
    const Color bg = selected ? style_.selected_background.value_or(p.selection) : p.surface;
    const Color fg = disabled ? p.text_secondary : (selected ? style_.selected_foreground.value_or(p.selection_text) : p.text);
    const int radius = theme_metrics().corner_radius_control;
    fill_rounded_rect(di->hDC, rc, radius, bg, bg);
    if (di->itemID != LB_ERR) {
        LRESULT len = SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        if (len != LB_ERR && len < 255) {
            wchar_t buf[256]{};
            if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, reinterpret_cast<LPARAM>(buf)) != LB_ERR) {
                HFONT font = fonts() ? fonts()->regular(dpi_of(di->hwndItem), 9) : nullptr;
                RECT text_rc = rc;
                text_rc.left += style_.horizontal_padding.value_or(8);
                draw_text(di->hDC, text_rc, buf, font, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }
}

} // namespace nfui

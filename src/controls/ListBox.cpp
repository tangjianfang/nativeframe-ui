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

void ListBox::set_hovered_row(int row) noexcept {
    if (hovered_row_ == row) {
        return;
    }
    hovered_row_ = row;
    if (valid()) {
        // Whole-control invalidation. Cheap for typical list sizes; per-row rect
        // invalidation is deferred to V1.5 (see docs/KNOWLEDGE/polish/...).
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void ListBox::draw_item(DRAWITEMSTRUCT* di) noexcept {
    if (di == nullptr) return;
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const bool selected = (di->itemState & ODS_SELECTED) != 0;
    const bool disabled = (di->itemState & ODS_DISABLED) != 0;
    RECT rc = di->rcItem;
    Color bg = selected ? style_.selected_background.value_or(p.selection) : p.surface;
    if (!selected && static_cast<int>(di->itemID) == hovered_row_) {
        // Hover overlay: tint surface toward text at 6% so the hover state reads
        // on both light (text is darker → subtle darken) and dark (text is lighter
        // → subtle lighten) palettes. Selection styling still takes priority.
        bg = alpha_blend(p.surface, p.text, 0.06f);
    }
    const Color fg = disabled ? p.text_secondary : (selected ? style_.selected_foreground.value_or(p.selection_text) : p.text);
    const int radius = theme_metrics().corner_radius_control;
    // Establish the current list surface underneath the rounded selection/hover
    // shape. Otherwise the outside corner pixels come from an old native
    // listbox frame during a palette transition.
    fill_rect(di->hDC, rc, p.surface);
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

void ListBox::on_reflected_draw_item(DRAWITEMSTRUCT* di) noexcept {
    draw_item(di);
}

void ListBox::on_subclass_mouse_move(LPARAM lparam) noexcept {
    if (!valid()) return;
    // Use the standard listbox control message to find which row is under the
    // cursor. LB_ITEMFROMPOINT is harmless on non-list HWNDs, but we still
    // gate on the class name to keep the behavior explicit and to avoid
    // surprise cost on busy controls.
    wchar_t cls[32]{};
    if (GetClassNameW(hwnd(), cls, 32) <= 0 || lstrcmpW(cls, WC_LISTBOXW) != 0) {
        return;
    }
    POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    LRESULT hit = SendMessageW(hwnd(), LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
    int row = (hit != LB_ERR && HIWORD(hit) == 0) ? static_cast<int>(LOWORD(hit)) : -1;
    if (hovered_row() != row) {
        set_hovered_row(row);
    }
}

void ListBox::on_subclass_mouse_leave() noexcept {
    set_hovered_row(-1);
}

} // namespace nfui

#include <nfui/Controls.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>
#include <string>

namespace nfui {

namespace {
constexpr UINT ocm_base = WM_USER + 0x1c00;

bool control_is_owner_draw(HWND hwnd) noexcept {
    // LBS_OWNERDRAW* excluded: per-row hover highlight not yet implemented; avoids needless repaint.
    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    return (style & BS_OWNERDRAW) != 0
        || (style & SS_OWNERDRAW) != 0;
}

void draw_list_item(DRAWITEMSTRUCT* di, const ThemePalette& p, FontCache* fonts) noexcept {
    if (di == nullptr) {
        return;
    }
    const bool selected = (di->itemState & ODS_SELECTED) != 0;
    const bool disabled = (di->itemState & ODS_DISABLED) != 0;
    RECT rc = di->rcItem;
    const Color bg = selected ? p.selection : p.surface;
    const Color fg = disabled ? p.text_secondary : (selected ? p.selection_text : p.text);
    // ListBox owner-draw uses per-item DRAWITEMSTRUCT DCs (one per row); buffering each row
    // separately is wasteful. Paint directly into di->hDC with metrics-driven corner radius.
    const int radius = theme_metrics().corner_radius_control;
    fill_rounded_rect(di->hDC, rc, radius, bg, bg);
    if (di->itemID != LB_ERR) {
        LRESULT len = SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        if (len != LB_ERR && len < 255) { // leave room for null terminator
            wchar_t buf[256]{};
            if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, reinterpret_cast<LPARAM>(buf)) != LB_ERR) {
                HFONT font = fonts ? fonts->regular(dpi_of(di->hwndItem), 9) : nullptr;
                RECT text_rc = rc;
                text_rc.left += 8;
                draw_text(di->hDC, text_rc, buf, font, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }
}
}

HWND Control::hwnd() const noexcept {
    return hwnd_.hwnd();
}

bool Control::valid() const noexcept {
    return hwnd_.valid();
}

bool Control::create_native(std::wstring_view class_name, const ControlCreateParams& params, DWORD extra_style) noexcept {
    if (params.instance == nullptr || params.parent == nullptr || class_name.empty()) {
        return false;
    }

    std::wstring owned_class_name(class_name);
    caption_ = std::wstring(params.text);
    HWND created = CreateWindowExW(params.ex_style,
                                   owned_class_name.c_str(),
                                   caption_.c_str(),
                                   params.style | extra_style,
                                   params.x,
                                   params.y,
                                   params.width,
                                   params.height,
                                   params.parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(params.control_id)),
                                   params.instance,
                                   nullptr);
    if (created == nullptr) {
        return false;
    }

    hwnd_ = OwnedHwnd(created);
    if (SetWindowSubclass(created, &Control::subclass_proc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        hwnd_.reset();
        return false;
    }
    return true;
}

void Control::detach_destroyed_hwnd(HWND hwnd) noexcept {
    if (hwnd_.hwnd() == hwnd) {
        static_cast<void>(hwnd_.release());
    }
}

LRESULT CALLBACK Control::subclass_proc(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam,
                                        UINT_PTR subclass_id,
                                        DWORD_PTR ref_data) noexcept {
    auto* control = reinterpret_cast<Control*>(ref_data);

    switch (message) {
    case ocm_base + WM_DRAWITEM: {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        if (di == nullptr || control == nullptr) {
            break;
        }
        if (di->CtlType == ODT_BUTTON || di->CtlType == ODT_STATIC) {
            PaintState state{};
            state.bounds = di->rcItem;
            state.hover = control->hover_state_.hover();
            state.pressed = (di->itemState & ODS_SELECTED) != 0;
            state.focused = (di->itemState & ODS_FOCUS) != 0;
            state.enabled = (di->itemState & ODS_DISABLED) == 0;
            control->on_paint(di->hDC, state);
        } else if (di->CtlType == ODT_LISTBOX) {
            const ThemePalette* pal = control->palette();
            const ThemePalette& palref = pal ? *pal : theme_palette(ThemeMode::light);
            draw_list_item(di, palref, control->fonts());
        }
        return TRUE;
    }
    case ocm_base + WM_MEASUREITEM: {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (mi != nullptr && mi->CtlType == ODT_LISTBOX) {
            mi->itemHeight = static_cast<UINT>(font_pixel_height(9, dpi_of(hwnd)) + 8);
            return TRUE;
        }
        break; // let DefSubclassProc handle unknown CtlTypes
    }
    case WM_MOUSEMOVE: {
        if (control != nullptr && control_is_owner_draw(hwnd)) {
            control->hover_state_.on_mouse_move(hwnd);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (control != nullptr) {
            control->hover_state_.on_mouse_leave();
            if (!control->hover_state_.hover()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        if (control != nullptr && control_is_owner_draw(hwnd)) {
            control->hover_state_.on_lbutton_down();
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (control != nullptr) {
            control->hover_state_.on_lbutton_up();
        }
        break;
    }
    case WM_SETTEXT: {
        if (control != nullptr && lparam != 0) {
            try {
                control->caption_ = reinterpret_cast<const wchar_t*>(lparam);
            } catch (...) {
                // Allocation failure: leave caption_ as-is. Painting falls back
                // to whatever caption was cached; never let an exception escape
                // the noexcept subclass callback boundary.
            }
        }
        break;
    }
    case ocm_base + WM_NOTIFY: {
        auto* nmh = reinterpret_cast<NMHDR*>(lparam);
        if (nmh != nullptr && nmh->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                return CDRF_NOTIFYITEMDRAW;
            }
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const ThemePalette* pal = control ? control->palette() : nullptr;
                const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
                const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                cd->clrText = selected ? p.selection_text.rgb : p.text.rgb;
                cd->clrTextBk = selected ? p.selection.rgb : p.surface.rgb;
                return CDRF_DODEFAULT;
            }
        }
        break; // other notify codes → DefSubclassProc
    }
    case WM_PAINT: {
        if (control != nullptr && control->wants_self_paint()) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc != nullptr) {
                PaintState state{};
                state.hover = control->hover_state_.hover();
                state.pressed = false;
                state.focused = false;
                state.enabled = IsWindowEnabled(hwnd) != FALSE;
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                state.bounds = bounds;
                control->on_paint(dc, state);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        break;
    }
    default:
        break;
    }

    LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        if (control != nullptr) {
            control->detach_destroyed_hwnd(hwnd);
        }
        RemoveWindowSubclass(hwnd, &Control::subclass_proc, subclass_id);
    }
    return result;
}

} // namespace nfui
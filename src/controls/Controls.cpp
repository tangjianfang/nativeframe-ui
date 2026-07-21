#include <nfui/Controls.hpp>
#include <nfui/Controls/ListBox.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>
#include <windowsx.h>
#include <string>

namespace nfui {

namespace {
constexpr UINT ocm_base = WM_USER + 0x1c00;

bool control_is_owner_draw(HWND hwnd) noexcept {
    // ListBox owner-draw now supports per-row hover via ListBox::set_hovered_row.
    // Invalidation is whole-control (cheap for typical list sizes); per-row rect
    // optimization deferred to V1.5.
    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    return (style & BS_OWNERDRAW) != 0
        || (style & SS_OWNERDRAW) != 0;
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
            // Dispatch to ListBox::draw_item so per-row hover (and any other
            // ListBox-specific styling) is honored. draw_list_item remains
            // available for any non-ListBox ODT_LISTBOX owner.
            auto* lb = static_cast<ListBox*>(control);
            lb->draw_item(di);
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
        // ListBox row hover: track which row is under the cursor. The subclass
        // proc is shared across all controls, so we gate on the window class
        // name (LISTBOX) to avoid running LB_ITEMFROMPOINT on non-list HWNDs.
        if (control != nullptr) {
            wchar_t cls[32]{};
            if (GetClassNameW(hwnd, cls, 32) > 0 && lstrcmpW(cls, WC_LISTBOXW) == 0) {
                POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                LRESULT hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
                int row = (hit != LB_ERR && HIWORD(hit) == 0) ? static_cast<int>(LOWORD(hit)) : -1;
                auto* lb = static_cast<ListBox*>(control);
                if (lb->hovered_row() != row) {
                    lb->set_hovered_row(row);
                }
            }
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (control != nullptr) {
            control->hover_state_.on_mouse_leave();
            if (!control->hover_state_.hover()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            // Clear any ListBox row hover tracking when the cursor leaves the
            // control entirely (set_hovered_row will InvalidateRect for us).
            wchar_t cls[32]{};
            if (GetClassNameW(hwnd, cls, 32) > 0 && lstrcmpW(cls, WC_LISTBOXW) == 0) {
                auto* lb = static_cast<ListBox*>(control);
                lb->set_hovered_row(-1);
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
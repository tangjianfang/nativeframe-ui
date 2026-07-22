#include <nfui/Controls.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>
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
            // Dispatch to the leaf class via virtual on_reflected_draw_item.
            // ListBox overrides this; other ODT_LISTBOX owners (none today)
            // get the default no-op. This keeps nfui_control_base free of
            // any direct dependency on nfui_listbox.
            control->on_reflected_draw_item(di);
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
        // Leaf-specific subclass hooks (ListBox row-hover tracking, etc.).
        // Default impls are no-ops; ListBox overrides on_subclass_mouse_move
        // to do LB_ITEMFROMPOINT work. This keeps the control_base TU free
        // of any listbox-specific symbols.
        if (control != nullptr) {
            control->on_subclass_mouse_move(lparam);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (control != nullptr) {
            control->hover_state_.on_mouse_leave();
            if (!control->hover_state_.hover()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            control->on_subclass_mouse_leave();
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
    case WM_SETFONT: {
        // P6.1: forward WM_SETFONT to defaults and invalidate the control so
        // the new font is applied immediately (covers DPI-change rebroadcast
        // where the OS hands each control a freshly-scaled HFONT).
        // See docs/KNOWLEDGE/polish/2026-07-22-dpi-wm-setfont.md.
        {
            LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
            InvalidateRect(hwnd, nullptr, FALSE);
#ifdef _DEBUG
            OutputDebugStringW(L"NFUI: WM_SETFONT received\n");
#endif
            return r;
        }
    }
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE: {
        // System theme notifications do not carry the application's injected
        // palette. Chain first so native controls refresh their own theme data,
        // then invalidate so custom paint and native chrome converge on the
        // same message-loop turn.
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return r;
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
            // Chain DefSubclassProc so the native control can render its
            // content (notably StatusBar parts and text) over the custom
            // palette background.
            return DefSubclassProc(hwnd, message, wparam, lparam);
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
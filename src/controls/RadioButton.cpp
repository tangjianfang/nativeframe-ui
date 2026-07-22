#include <nfui/Controls/RadioButton.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

namespace {

const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

// Suppress the native dotted focus rectangle on a button-class control by
// setting its UISF_HIDEFOCUS UI-state flag directly (per-control; does not
// propagate to siblings). See CheckBox.cpp for the full rationale.
void suppress_native_focus_rect(HWND hwnd) noexcept {
    SendMessageW(hwnd, WM_UPDATEUISTATE,
                 MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
}

} // namespace

bool RadioButton::create(const ControlCreateParams& params) noexcept {
    if (!create_native(L"BUTTON", params, BS_AUTORADIOBUTTON)) {
        return false;
    }
    if (SetWindowSubclass(hwnd(), &RadioButton::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    suppress_native_focus_rect(hwnd());
    return true;
}

LRESULT CALLBACK RadioButton::visual_subclass_proc(HWND hwnd, UINT message,
                                                   WPARAM wparam, LPARAM lparam,
                                                   UINT_PTR subclass_id,
                                                   DWORD_PTR ref_data) noexcept {
    auto* rb = reinterpret_cast<RadioButton*>(ref_data);
    switch (message) {
    case WM_SETFOCUS: {
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        suppress_native_focus_rect(hwnd);
        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_UPDATENOW);
        return r;
    }
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_UPDATENOW);
        return r;
    }
    case WM_PAINT: {
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        if (rb != nullptr
            && GetFocus() == hwnd
            && IsWindowEnabled(hwnd) != FALSE) {
            HDC dc = GetDC(hwnd);
            if (dc != nullptr) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const ThemePalette& p = effective_palette(rb->palette());
                paint_focus_border(dc, rc, p.accent, 2);
                ReleaseDC(hwnd, dc);
            }
        }
        return r;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &RadioButton::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
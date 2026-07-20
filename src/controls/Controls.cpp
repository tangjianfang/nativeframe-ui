#include <nfui/Controls.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>
#include <string>

namespace nfui {

namespace {
constexpr UINT ocm_base = WM_USER + 0x1c00;

bool control_is_owner_draw(HWND hwnd) noexcept {
    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    return (style & BS_OWNERDRAW) != 0
        || (style & LBS_OWNERDRAWFIXED) != 0
        || (style & LBS_OWNERDRAWVARIABLE) != 0
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
        if (di != nullptr && control != nullptr) {
            PaintState state{};
            state.bounds = di->rcItem;
            state.hover = control->hover_;
            state.pressed = (di->itemState & ODS_SELECTED) != 0;
            state.focused = (di->itemState & ODS_FOCUS) != 0;
            state.enabled = (di->itemState & ODS_DISABLED) == 0;
            control->on_paint(di->hDC, state);
        }
        return TRUE;
    }
    case WM_MOUSEMOVE: {
        if (control != nullptr && !control->hover_ && control_is_owner_draw(hwnd)) {
            control->hover_ = true;
            TRACKMOUSEEVENT tme{};
            tme.cbSize = static_cast<DWORD>(sizeof(tme));
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (control != nullptr && control->hover_) {
            control->hover_ = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }
    case WM_SETTEXT: {
        if (control != nullptr && lparam != 0) {
            control->caption_ = reinterpret_cast<const wchar_t*>(lparam);
        }
        break;
    }
    case WM_PAINT: {
        if (control != nullptr && control->wants_self_paint()) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc != nullptr) {
                PaintState state{};
                state.hover = control->hover_;
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

bool Button::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams owner_params = params;
    owner_params.style &= ~WS_BORDER;
    return create_native(L"BUTTON", owner_params, BS_OWNERDRAW | BS_FLAT);
}

void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* palette_ptr = palette();
    const ThemePalette& p = palette_ptr ? *palette_ptr : theme_palette(ThemeMode::light);
    Color face = p.accent;
    Color text = p.accent_text;
    if (!state.enabled) {
        face = p.border;
        text = p.text_secondary;
    } else if (state.pressed || state.hover) {
        face = p.accent_hover;
    }
    fill_rounded_rect(dc, state.bounds, 6, face, p.border);
    FontCache* cache = fonts();
    HFONT font = cache ? cache->regular(96, 9) : nullptr;
    draw_text(dc, state.bounds, caption(), font, text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

bool CheckBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"BUTTON", params, BS_AUTOCHECKBOX);
}

bool RadioButton::create(const ControlCreateParams& params) noexcept {
    return create_native(L"BUTTON", params, BS_AUTORADIOBUTTON);
}

bool Edit::create(const ControlCreateParams& params) noexcept {
    return create_native(L"EDIT", params, WS_BORDER | ES_LEFT | ES_AUTOHSCROLL);
}

bool StaticText::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams static_params = params;
    static_params.style &= ~WS_TABSTOP;
    return create_native(L"STATIC", static_params, SS_LEFT);
}

void StaticText::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    RECT rc = state.bounds;
    fill_rounded_rect(dc, rc, 0, p.background, p.background);
    HFONT font = fonts() ? fonts()->regular(96, 9) : nullptr;
    draw_text(dc, rc, caption(), font, p.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

bool ComboBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"COMBOBOX", params, CBS_DROPDOWNLIST | WS_VSCROLL);
}

bool ListBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"LISTBOX", params, WS_BORDER | LBS_NOTIFY | WS_VSCROLL);
}

bool TreeView::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_TREEVIEWW, params, WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS);
}

bool ListView::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_LISTVIEWW, params, WS_BORDER | LVS_REPORT);
}

bool StatusBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams status_params = params;
    status_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(STATUSCLASSNAMEW, status_params, SBARS_SIZEGRIP);
}

bool TabControl::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_TABCONTROLW, params, WS_CLIPSIBLINGS);
}

bool ProgressBar::create(const ControlCreateParams& params) noexcept {
    return create_native(PROGRESS_CLASSW, params, PBS_SMOOTH);
}

bool Panel::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams panel_params = params;
    panel_params.style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
    return create_native(L"STATIC", panel_params, SS_WHITERECT);
}

bool Splitter::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams splitter_params = params;
    splitter_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(L"STATIC", splitter_params, SS_GRAYRECT);
}

void Splitter::set_ratio(double ratio) noexcept {
    if (ratio < 0.0) {
        ratio_ = 0.0;
    } else if (ratio > 1.0) {
        ratio_ = 1.0;
    } else {
        ratio_ = ratio;
    }
}

double Splitter::ratio() const noexcept {
    return ratio_;
}

} // namespace nfui

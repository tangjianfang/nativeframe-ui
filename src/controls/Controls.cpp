#include <nfui/Controls.hpp>

#include <commctrl.h>
#include <string>

namespace nfui {

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
    std::wstring text(params.text);
    HWND created = CreateWindowExW(params.ex_style,
                                   owned_class_name.c_str(),
                                   text.c_str(),
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
    return create_native(L"BUTTON", params, BS_PUSHBUTTON);
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

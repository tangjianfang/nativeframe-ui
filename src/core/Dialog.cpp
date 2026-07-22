#include <nfui/Dialog.hpp>

namespace nfui {

int Dialog::show_modal(HINSTANCE instance, LPCWSTR template_name,
                       HWND parent, DLGPROC proc, LPARAM init_param) noexcept {
    if (instance == nullptr || template_name == nullptr || proc == nullptr) {
        return -1;
    }
    modal_ = true;
    // DialogBoxParamW owns the dialog HWND; EndDialog inside the DLGPROC
    // tears it down. modal_result_ holds the INT_PTR returned for the
    // caller (IDOK, IDCANCEL, -1 on failure).
    modal_result_ = static_cast<int>(DialogBoxParamW(instance, template_name, parent, proc, init_param));
    return modal_result_;
}

HWND Dialog::show_modeless(HINSTANCE instance, LPCWSTR template_name,
                           HWND parent, DLGPROC proc, LPARAM init_param) noexcept {
    if (instance == nullptr || template_name == nullptr || proc == nullptr) {
        return nullptr;
    }
    HWND created = CreateDialogParamW(instance, template_name, parent, proc, init_param);
    if (created == nullptr) {
        return nullptr;
    }
    hwnd_ = OwnedHwnd(created);
    modal_ = false;
    return created;
}

void Dialog::end_modeless(int result_code) noexcept {
    if (valid() && !modal_) {
        EndDialog(hwnd(), result_code);
        hwnd_.reset();
    }
}

} // namespace nfui
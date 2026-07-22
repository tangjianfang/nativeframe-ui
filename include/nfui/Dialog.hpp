#pragma once

#include <nfui/Handle.hpp>

#include <windows.h>

namespace nfui {

// Lightweight wrapper around the Win32 dialog APIs (DialogBoxParamW /
// CreateDialogParamW). Mirrors the Window class surface: hwnd()/valid()
// return the underlying HWND, and an OwnedHwnd enforces RAII for the
// modeless lifetime. Modal dialogs are auto-destroyed by DialogBoxParamW
// once EndDialog is called inside the DLGPROC, so the OwnedHwnd stays
// empty for that path; modal_result() exposes the INT_PTR returned by
// DialogBoxParamW after the user dismisses the dialog.
class Dialog {
public:
    Dialog() = default;
    ~Dialog() = default;

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;

    // Modal dialog: blocks until dismissed. Returns IDOK / IDCANCEL / etc.
    // DialogBoxParamW creates and destroys the underlying HWND internally;
    // callers must call EndDialog from their DLGPROC to terminate.
    [[nodiscard]] int show_modal(HINSTANCE instance, LPCWSTR template_name,
                                 HWND parent, DLGPROC proc, LPARAM init_param = 0) noexcept;

    // Modeless: returns immediately. The Dialog owns the resulting HWND
    // via OwnedHwnd RAII; callers close it with end_modeless() or let the
    // destructor DestroyWindow the HWND.
    [[nodiscard]] HWND show_modeless(HINSTANCE instance, LPCWSTR template_name,
                                     HWND parent, DLGPROC proc, LPARAM init_param = 0) noexcept;

    // Close a modeless dialog. No-op for modal dialogs or when no HWND
    // is currently owned (e.g. already closed).
    void end_modeless(int result_code) noexcept;

    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_.hwnd(); }
    [[nodiscard]] bool valid() const noexcept { return hwnd_.valid(); }
    [[nodiscard]] int modal_result() const noexcept { return modal_result_; }

private:
    OwnedHwnd hwnd_{};
    bool modal_{false};
    int modal_result_{-1};
};

} // namespace nfui
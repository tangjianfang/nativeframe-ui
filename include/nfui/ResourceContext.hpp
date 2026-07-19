#pragma once

#include <string>
#include <windows.h>

namespace nfui {

class ResourceContext {
public:
    explicit ResourceContext(HINSTANCE module) noexcept;

    [[nodiscard]] HINSTANCE module() const noexcept;
    [[nodiscard]] bool has_dialog(int resource_id) const noexcept;
    [[nodiscard]] bool has_menu(int resource_id) const noexcept;
    [[nodiscard]] bool has_string(int resource_id) const noexcept;
    [[nodiscard]] bool has_icon(int resource_id) const noexcept;
    [[nodiscard]] bool has_bitmap(int resource_id) const noexcept;
    [[nodiscard]] bool has_toolbar(int resource_id) const noexcept;
    [[nodiscard]] std::wstring load_string(int resource_id) const;
    [[nodiscard]] HWND create_dialog(int resource_id, HWND parent, DLGPROC dialog_proc, LPARAM init_param) const noexcept;
    [[nodiscard]] INT_PTR show_modal_dialog(int resource_id, HWND parent, DLGPROC dialog_proc, LPARAM init_param) const noexcept;

private:
    HINSTANCE module_{};
};

} // namespace nfui

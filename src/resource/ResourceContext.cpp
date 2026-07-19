#include <nfui/ResourceContext.hpp>

#include <string>

namespace nfui {
namespace {

const LPCWSTR toolbar_resource_type = MAKEINTRESOURCEW(241);

[[nodiscard]] bool has_resource(HINSTANCE module, int resource_id, LPCWSTR resource_type) noexcept {
    return module != nullptr && FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_type) != nullptr;
}

} // namespace

ResourceContext::ResourceContext(HINSTANCE module) noexcept
    : module_(module) {
}

HINSTANCE ResourceContext::module() const noexcept {
    return module_;
}

bool ResourceContext::has_dialog(int resource_id) const noexcept {
    return has_resource(module_, resource_id, RT_DIALOG);
}

bool ResourceContext::has_menu(int resource_id) const noexcept {
    if (module_ == nullptr) {
        return false;
    }

    HMENU menu = LoadMenuW(module_, MAKEINTRESOURCEW(resource_id));
    if (menu == nullptr) {
        return false;
    }
    DestroyMenu(menu);
    return true;
}

bool ResourceContext::has_string(int resource_id) const noexcept {
    if (module_ == nullptr) {
        return false;
    }

    wchar_t buffer[2]{};
    return LoadStringW(module_, resource_id, buffer, 2) > 0;
}

bool ResourceContext::has_icon(int resource_id) const noexcept {
    if (module_ == nullptr) {
        return false;
    }

    HICON icon = static_cast<HICON>(
        LoadImageW(module_, MAKEINTRESOURCEW(resource_id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (icon == nullptr) {
        return false;
    }
    DestroyIcon(icon);
    return true;
}

bool ResourceContext::has_bitmap(int resource_id) const noexcept {
    if (module_ == nullptr) {
        return false;
    }

    HBITMAP bitmap = static_cast<HBITMAP>(
        LoadImageW(module_, MAKEINTRESOURCEW(resource_id), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
    if (bitmap == nullptr) {
        return false;
    }
    DeleteObject(bitmap);
    return true;
}

bool ResourceContext::has_toolbar(int resource_id) const noexcept {
    return has_resource(module_, resource_id, toolbar_resource_type);
}

std::wstring ResourceContext::load_string(int resource_id) const {
    if (module_ == nullptr) {
        return {};
    }

    wchar_t stack_buffer[256]{};
    int length = LoadStringW(module_, resource_id, stack_buffer, static_cast<int>(std::size(stack_buffer)));
    if (length <= 0) {
        return {};
    }
    return std::wstring(stack_buffer, static_cast<std::size_t>(length));
}

HWND ResourceContext::create_dialog(int resource_id, HWND parent, DLGPROC dialog_proc, LPARAM init_param) const noexcept {
    if (module_ == nullptr || dialog_proc == nullptr) {
        return nullptr;
    }

    return CreateDialogParamW(module_, MAKEINTRESOURCEW(resource_id), parent, dialog_proc, init_param);
}

INT_PTR ResourceContext::show_modal_dialog(int resource_id, HWND parent, DLGPROC dialog_proc, LPARAM init_param) const noexcept {
    if (module_ == nullptr || dialog_proc == nullptr) {
        return -1;
    }

    return DialogBoxParamW(module_, MAKEINTRESOURCEW(resource_id), parent, dialog_proc, init_param);
}

} // namespace nfui

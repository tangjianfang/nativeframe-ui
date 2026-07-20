#include <nfui/Icon.hpp>

#include <commctrl.h>
#include <commoncontrols.h>

namespace nfui {

int icon_pixel_size(int logical_size, int dpi) noexcept {
    if (logical_size <= 0 || dpi <= 0) return 0;
    return MulDiv(logical_size, dpi, 96);
}

HICON load_scaled_icon(HINSTANCE instance, LPCWSTR resource_id, int logical_size, int dpi) noexcept {
    const int px = icon_pixel_size(logical_size, dpi);
    if (px <= 0 || resource_id == nullptr) return nullptr;

    using LoadIconWithScaleDownProc = HRESULT (WINAPI*)(HINSTANCE, PCWSTR, int, int, HICON*);
    static auto* const proc = reinterpret_cast<LoadIconWithScaleDownProc>(
        GetProcAddress(GetModuleHandleW(L"comctl32.dll"), "LoadIconWithScaleDown"));

    HICON icon = nullptr;
    if (proc != nullptr && SUCCEEDED(proc(instance, resource_id, px, px, &icon))) {
        return icon;
    }

    icon = static_cast<HICON>(LoadImageW(instance, resource_id, IMAGE_ICON, px, px, LR_DEFAULTCOLOR));
    return icon;
}

} // namespace nfui

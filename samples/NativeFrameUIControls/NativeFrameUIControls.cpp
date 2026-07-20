#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <string>

namespace {

constexpr int id_header = 101;
constexpr int id_ok = 102;
constexpr int id_cancel = 103;
constexpr int id_list = 104;
constexpr int id_view = 105;
constexpr int id_icon = 106;

class ControlsWindow final : public nfui::Window {
public:
    explicit ControlsWindow(HINSTANCE instance) noexcept
        : instance_(instance) {
    }

    ~ControlsWindow() noexcept {
        if (app_icon_ != nullptr) {
            DestroyIcon(app_icon_);
            app_icon_ = nullptr;
        }
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIControlsWindow",
            L"NativeFrame UI Controls",
            WS_OVERLAPPEDWINDOW,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            480,
            360,
        };

        if (!create(params)) {
            return false;
        }

        palette_ = nfui::theme_palette(nfui::ThemeMode::light);

        if (!create_children()) {
            return false;
        }

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_DPICHANGED: {
            auto* new_rect = reinterpret_cast<RECT*>(lparam);
            if (new_rect != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             new_rect->left,
                             new_rect->top,
                             new_rect->right - new_rect->left,
                             new_rect->bottom - new_rect->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            InvalidateRect(header_.hwnd(), nullptr, TRUE);
            InvalidateRect(ok_.hwnd(), nullptr, TRUE);
            InvalidateRect(cancel_.hwnd(), nullptr, TRUE);
            InvalidateRect(list_.hwnd(), nullptr, TRUE);
            InvalidateRect(view_.hwnd(), nullptr, TRUE);
            InvalidateRect(icon_.hwnd(), nullptr, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int command_id = LOWORD(wparam);
            if (command_id == id_ok || command_id == id_cancel) {
                destroy();
                return 0;
            }
            break;
        }
        case WM_DESTROY: {
            if (app_icon_ != nullptr) {
                DestroyIcon(app_icon_);
                app_icon_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
        }

        return nfui::Window::handle_message(message, wparam, lparam);
    }

private:
    [[nodiscard]] int dpi_of_main() const noexcept {
        const UINT dpi = GetDpiForWindow(hwnd());
        return dpi != 0 ? static_cast<int>(dpi) : 96;
    }

    template<typename ControlType>
    [[nodiscard]] bool create_child(ControlType& control,
                                    int id,
                                    const wchar_t* text,
                                    int x,
                                    int y,
                                    int w,
                                    int h) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            x,
            y,
            w,
            h,
        };
        if (!control.create(params)) {
            return false;
        }
        control.set_palette(&palette_);
        control.set_font_cache(&fonts_);
        return true;
    }

    [[nodiscard]] bool create_children() noexcept {
        if (!create_child(header_, id_header, L"NativeFrame UI Controls", 16, 16, 448, 24)) {
            return false;
        }
        if (!create_child(ok_, id_ok, L"OK", 16, 56, 96, 32)) {
            return false;
        }
        if (!create_child(cancel_, id_cancel, L"Cancel", 120, 56, 96, 32)) {
            return false;
        }
        if (!create_child(icon_, id_icon, L"", 384, 56, 32, 32)) {
            return false;
        }
        app_icon_ = nfui::load_scaled_icon(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), 32, dpi_of_main());
        if (app_icon_ != nullptr) {
            icon_.set_icon(app_icon_);
        }
        if (!create_child(list_, id_list, L"", 16, 104, 200, 240)) {
            return false;
        }
        ListBox_AddString(list_.hwnd(), L"Apple");
        ListBox_AddString(list_.hwnd(), L"Banana");
        ListBox_AddString(list_.hwnd(), L"Cherry");
        ListBox_AddString(list_.hwnd(), L"Dates");

        if (!create_child(view_, id_view, L"", 232, 104, 232, 240)) {
            return false;
        }
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.cx = 200;
        column.pszText = const_cast<LPWSTR>(L"Item");
        ListView_InsertColumn(view_.hwnd(), 0, &column);

        for (int i = 0; i < 3; ++i) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            std::wstring text = L"Row " + std::to_wstring(i);
            item.pszText = text.data();
            ListView_InsertItem(view_.hwnd(), &item);
        }

        return true;
    }

    HINSTANCE instance_{};
    HICON app_icon_{};
    nfui::ThemePalette palette_{};
    nfui::FontCache fonts_{};
    nfui::StaticText header_;
    nfui::Button ok_;
    nfui::Button cancel_;
    nfui::ListBox list_;
    nfui::ListView view_;
    nfui::IconView icon_;
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    ControlsWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}

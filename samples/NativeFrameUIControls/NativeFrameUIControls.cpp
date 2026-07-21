#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

namespace {

constexpr int id_ok = 102;
constexpr int id_cancel = 103;
constexpr int id_list = 104;
constexpr int id_view = 105;
constexpr int id_icon = 106;
constexpr int id_theme_toggle = 107;

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

        mode_ = nfui::ThemeMode::light;
        palette_ = nfui::theme_palette(mode_);

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
            const int dpi = nfui::dpi_of(hwnd());
            if (view_.hwnd() != nullptr) {
                if (HFONT f = fonts_.regular(dpi, 9)) {
                    SendMessageW(view_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
                }
            }
            theme_toggle_.set_font_cache(&fonts_);
            if (HFONT toggle_font = fonts_.regular(dpi, 9)) {
                SendMessageW(theme_toggle_.hwnd(),
                             WM_SETFONT,
                             reinterpret_cast<WPARAM>(toggle_font),
                             FALSE);
            }
            if (list_.hwnd() != nullptr) {
                SendMessageW(list_.hwnd(),
                             LB_SETITEMHEIGHT,
                             0,
                             static_cast<LPARAM>(nfui::font_pixel_height(9, dpi) + 8));
                InvalidateRect(list_.hwnd(), nullptr, TRUE);
            }
            InvalidateRect(ok_.hwnd(), nullptr, FALSE);
            InvalidateRect(cancel_.hwnd(), nullptr, FALSE);
            InvalidateRect(theme_toggle_.hwnd(), nullptr, FALSE);
            InvalidateRect(list_.hwnd(), nullptr, FALSE);
            InvalidateRect(view_.hwnd(), nullptr, FALSE);
            InvalidateRect(icon_.hwnd(), nullptr, FALSE);
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND: {
            const int command_id = LOWORD(wparam);
            if (command_id == id_theme_toggle) {
                apply_toggle_theme();
                return 0;
            }
            if (command_id == id_ok || command_id == id_cancel) {
                destroy();
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            RECT client{};
            GetClientRect(hwnd(), &client);
            // Flicker-free offscreen buffer over the full client area.
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                nfui::fill_rect(target, client, palette_.background);
                const int dpi = nfui::dpi_of(hwnd());
                const int header_height = nfui::font_pixel_height(16, dpi);
                RECT header_rect{16, 16, client.right - 16, 16 + header_height};
                HFONT header_font = fonts_.serif(dpi, 16);
                nfui::draw_text(target,
                                header_rect,
                                L"NativeFrame UI Controls",
                                header_font,
                                palette_.text,
                                DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
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
        control.set_palette(&palette_);
        control.set_font_cache(&fonts_);
        if (!control.create(params)) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool create_children() noexcept {
        if (!create_child(theme_toggle_, id_theme_toggle, L"Switch to dark", 360, 12, 104, 28)) {
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
        app_icon_ = nfui::load_scaled_icon(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), 32, nfui::dpi_of(hwnd()));
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

        static constexpr const wchar_t* rows[] = {L"Row 0", L"Row 1", L"Row 2"};
        int row_index = 0;
        for (const wchar_t* text : rows) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row_index++;
            item.pszText = const_cast<LPWSTR>(text);
            ListView_InsertItem(view_.hwnd(), &item);
        }

        return true;
    }

    HINSTANCE instance_{};
    HICON app_icon_{};
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::ThemePalette palette_{};
    nfui::FontCache fonts_{};
    nfui::Button theme_toggle_;
    nfui::Button ok_;
    nfui::Button cancel_;
    nfui::ListBox list_;
    nfui::ListView view_;
    nfui::IconView icon_;

    void apply_toggle_theme() noexcept {
        mode_ = (mode_ == nfui::ThemeMode::dark) ? nfui::ThemeMode::light : nfui::ThemeMode::dark;
        palette_ = nfui::theme_palette(mode_);
        theme_toggle_.set_palette(&palette_);
        ok_.set_palette(&palette_);
        cancel_.set_palette(&palette_);
        list_.set_palette(&palette_);
        view_.set_palette(&palette_);
        icon_.set_palette(&palette_);
        SetWindowTextW(theme_toggle_.hwnd(),
                       mode_ == nfui::ThemeMode::dark ? L"Switch to light" : L"Switch to dark");
        InvalidateRect(theme_toggle_.hwnd(), nullptr, FALSE);
        InvalidateRect(ok_.hwnd(), nullptr, FALSE);
        InvalidateRect(cancel_.hwnd(), nullptr, FALSE);
        InvalidateRect(list_.hwnd(), nullptr, FALSE);
        InvalidateRect(view_.hwnd(), nullptr, FALSE);
        InvalidateRect(icon_.hwnd(), nullptr, FALSE);
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
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

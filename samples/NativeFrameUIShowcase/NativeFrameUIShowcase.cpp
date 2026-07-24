#include <nfui/Application.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/ResourceContext.hpp>
#include <nfui/Window.hpp>

#include "NativeFrameUIResource.h"
#include "ShowcaseView.hpp"

#include <windowsx.h>

namespace {

class ShowcaseWindow final : public nfui::Window {
public:
    explicit ShowcaseWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance) {
    }

    ~ShowcaseWindow() noexcept override {
        destroy_icons();
    }

    // CP32: lets wWinMain seed the view's mode before create_main wires the
    // first paint. Without this, --theme dark would still capture light.
    void set_view_theme(nfui::ThemeMode mode) noexcept {
        view_.set_theme_mode(mode);
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIShowcaseWindow",
            L"NativeFrame UI Showcase",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1440,
            900,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        refresh_view_bounds();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            refresh_view_bounds();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            track_mouse_leave();
            if (view_.on_mouse_move({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (view_.clear_hover()) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN:
            // CP22: a mouse click on a paint-only affordance (theme toggle
            // or nav row) also moves keyboard focus onto it so Tab/Enter
            // behave consistently with the click path.
            if (view_.on_left_button_down({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN: {
            // CP22: keyboard navigation across the painted affordances.
            bool redraw = false;
            switch (wparam) {
            case VK_TAB:
                view_.cycle_focus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
                redraw = true;
                break;
            case VK_RETURN:
            case VK_SPACE:
                if (view_.activate_focused()) {
                    redraw = true;
                }
                break;
            default:
                break;
            }
            if (redraw) {
                InvalidateRect(hwnd(), nullptr, FALSE);
                return 0;
            }
            return 0;
        }
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            view_.set_dpi(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            refresh_view_bounds();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            view_.paint(hdc, fonts_);
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            destroy_icons();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

private:
    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }

        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON),
                                                    LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
        }
        if (small_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
        }
    }

    void destroy_icons() noexcept {
        if (large_icon_ != nullptr) {
            DestroyIcon(large_icon_);
            large_icon_ = nullptr;
        }
        if (small_icon_ != nullptr) {
            DestroyIcon(small_icon_);
            small_icon_ = nullptr;
        }
    }

    void track_mouse_leave() noexcept {
        if (tracking_mouse_) {
            return;
        }

        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = hwnd();
        tracking_mouse_ = TrackMouseEvent(&event) != FALSE;
    }

    void refresh_view_bounds() noexcept {
        if (hwnd() == nullptr) {
            return;
        }

        RECT client{};
        GetClientRect(hwnd(), &client);
        view_.set_client_rect(client);
        view_.set_dpi(nfui::dpi_of(hwnd()));
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::FontCache fonts_{};
    ShowcaseView view_;
    bool tracking_mouse_{};
    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme seeds the mode before create_main so the first paint
    // already shows the requested palette. Audit quotes the value
    // (--theme "dark"); strip the leading quote so the comparison sees 'dark'.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::light;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::light;
        tag += 7;
        while (*tag == L' ' || *tag == L'\t') ++tag;
        if (*tag == L'"') ++tag;
        if (wcsncmp(tag, L"dark", 4) == 0 && (tag[4] == L' ' || tag[4] == 0 || tag[4] == L'"')) return nfui::ThemeMode::dark;
        if (wcsncmp(tag, L"high_contrast", 13) == 0) return nfui::ThemeMode::high_contrast;
        return nfui::ThemeMode::light;
    };
    const nfui::ThemeMode initial_mode = parse_theme(cmd_line);

    ShowcaseWindow window(instance);
    window.set_view_theme(initial_mode);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}

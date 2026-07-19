#pragma once

#include <string_view>
#include <windows.h>
#include <commctrl.h>

namespace nfui {

struct WindowCreateParams {
    HINSTANCE instance{};
    std::wstring_view class_name{};
    std::wstring_view title{};
    DWORD style{WS_OVERLAPPEDWINDOW};
    DWORD ex_style{};
    int x{CW_USEDEFAULT};
    int y{CW_USEDEFAULT};
    int width{CW_USEDEFAULT};
    int height{CW_USEDEFAULT};
    HWND parent{};
    HMENU menu{};
};

class Window {
public:
    Window() = default;
    virtual ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool create(const WindowCreateParams& params) noexcept;
    void destroy() noexcept;

protected:
    virtual LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    virtual bool on_command(int command_id, HWND source, UINT notification_code);
    virtual LRESULT on_notify(int control_id, NMHDR* header);

private:
    [[nodiscard]] static bool register_window_class(HINSTANCE instance, std::wstring_view class_name) noexcept;
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

    HWND hwnd_{};
};

} // namespace nfui

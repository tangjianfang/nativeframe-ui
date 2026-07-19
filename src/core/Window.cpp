#include <nfui/Window.hpp>

#include <string>
#include <windowsx.h>

namespace nfui {
namespace {

void report_callback_exception() noexcept {
    OutputDebugStringW(L"NativeFrameUI: exception was stopped at WindowProc boundary.\n");
}

} // namespace

Window::~Window() noexcept {
    destroy();
}

HWND Window::hwnd() const noexcept {
    return hwnd_;
}

bool Window::create(const WindowCreateParams& params) noexcept {
    if (hwnd_ != nullptr || params.instance == nullptr || params.class_name.empty()) {
        return false;
    }

    if (!register_window_class(params.instance, params.class_name)) {
        return false;
    }

    std::wstring class_name(params.class_name);
    std::wstring title(params.title);
    HWND created = CreateWindowExW(params.ex_style,
                                   class_name.c_str(),
                                   title.c_str(),
                                   params.style,
                                   params.x,
                                   params.y,
                                   params.width,
                                   params.height,
                                   params.parent,
                                   params.menu,
                                   params.instance,
                                   this);
    return created != nullptr;
}

void Window::destroy() noexcept {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

LRESULT Window::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_COMMAND) {
        int command_id = LOWORD(wparam);
        UINT notification_code = HIWORD(wparam);
        HWND source = reinterpret_cast<HWND>(lparam);
        if (on_command(command_id, source, notification_code)) {
            return 0;
        }
    }

    if (message == WM_NOTIFY) {
        auto* header = reinterpret_cast<NMHDR*>(lparam);
        return on_notify(static_cast<int>(wparam), header);
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool Window::on_command(int, HWND, UINT) {
    return false;
}

LRESULT Window::on_notify(int, NMHDR*) {
    return 0;
}

bool Window::register_window_class(HINSTANCE instance, std::wstring_view class_name) noexcept {
    std::wstring owned_class_name(class_name);

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &Window::window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = owned_class_name.c_str();

    if (RegisterClassExW(&window_class) != 0) {
        return true;
    }

    if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW existing_class{};
    existing_class.cbSize = sizeof(existing_class);
    if (GetClassInfoExW(instance, owned_class_name.c_str(), &existing_class) == FALSE) {
        return false;
    }

    return existing_class.lpfnWndProc == &Window::window_proc;
}

LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    Window* window = nullptr;

    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<Window*>(create_struct->lpCreateParams);
        window->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window == nullptr) {
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    try {
        LRESULT result = window->handle_message(message, wparam, lparam);
        if (message == WM_NCDESTROY) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            window->hwnd_ = nullptr;
        }
        return result;
    } catch (...) {
        report_callback_exception();
        if (message == WM_NCDESTROY) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            window->hwnd_ = nullptr;
        }
        return 0;
    }
}

} // namespace nfui

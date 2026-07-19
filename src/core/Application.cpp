#include <nfui/Application.hpp>

#include <commctrl.h>

namespace nfui {

Application::Application(ApplicationConfig config) noexcept
    : config_(config) {
}

HINSTANCE Application::instance() const noexcept {
    return config_.instance;
}

int Application::show_command() const noexcept {
    return config_.show_command;
}

bool Application::initialize_process_dpi() noexcept {
    return SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE ||
           GetLastError() == ERROR_ACCESS_DENIED;
}

bool Application::initialize_common_controls() noexcept {
    INITCOMMONCONTROLSEX init{};
    init.dwSize = sizeof(init);
    init.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                 ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    return InitCommonControlsEx(&init) != FALSE;
}

int Application::run() noexcept {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

std::wstring_view version() noexcept {
    return L"0.1.0-dev";
}

} // namespace nfui

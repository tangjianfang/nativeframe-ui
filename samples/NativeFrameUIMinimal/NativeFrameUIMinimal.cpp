// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_controls + nfui_button
// (NOT the NativeFrameUI umbrella). Verifies that consumers can ship a
// minimal product with a small dependency footprint.
//
// Build deps (see CMakeLists.txt): NativeFrameUI::nfui_core,
// NativeFrameUI::nfui_theme, NativeFrameUI::nfui_controls,
// NativeFrameUI::nfui_button. Total 4 libs vs. 13 via the umbrella.
//
// TODO(P2.1): switch nfui_controls to nfui_control_base once the base-class
// extraction lands.

#include <nfui/Application.hpp>
#include <nfui/Controls/Button.hpp>
#include <nfui/Controls/Control.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>

namespace {

constexpr int ID_BUTTON_OK = 1001;

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_COMMAND && LOWORD(wparam) == ID_BUTTON_OK) {
        MessageBoxW(hwnd, L"Button clicked!", L"Minimal", MB_OK);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"NFUI_MINIMAL";
    if (RegisterClassExW(&wc) == 0) return 2;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NativeFrameUI Minimal",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 320, 120,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) return 3;

    // Create one Button via the per-component library (nfui_button).
    // No umbrella needed.
    nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::light);
    nfui::FontCache fonts;
    nfui::Button ok;
    nfui::ControlCreateParams bp{};
    bp.instance = instance;
    bp.parent = hwnd;
    bp.control_id = ID_BUTTON_OK;
    bp.text = L"Click me";
    bp.x = 100; bp.y = 40;
    bp.width = 120; bp.height = 32;
    ok.inject_theme(&palette, &fonts);
    if (!ok.create(bp)) return 4;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
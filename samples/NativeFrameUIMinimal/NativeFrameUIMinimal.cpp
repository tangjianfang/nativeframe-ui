// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_control_base +
// nfui_button (NOT the NativeFrameUI umbrella). Verifies that consumers
// can ship a minimal product with a small dependency footprint.
//
// Build deps (see CMakeLists.txt): NativeFrameUI::nfui_core,
// NativeFrameUI::nfui_theme, NativeFrameUI::nfui_control_base,
// NativeFrameUI::nfui_button. 4 libs vs. 13 via the umbrella.
//
// CP31 polish: replaced the generic DEFAULT_GUI_FONT title/button with
// FontCache Segoe UI faces, added a subtitle and a secondary ghost-style
// "Learn more" button, bumped vertical spacing to a comfortable 32 px
// rhythm, and made the sample respect `--theme light|dark|high_contrast`.

#include <nfui/Application.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>
#include <windowsx.h>

#include <shellapi.h>
#include <string>

namespace {

// Logical layout at 96 dpi; the window is fixed at this baseline so the
// sample stays tiny while still demonstrating a real vertical rhythm.
constexpr RECT kPrimaryButton{180, 144, 300, 180};     // "Open Sample"
constexpr RECT kSecondaryButton{180, 212, 300, 240};   // "Learn more" ghost

std::wstring g_status_text =
    L"Press a button to confirm the minimal link set is alive.";

nfui::ThemeMode g_theme_mode = nfui::ThemeMode::light;
nfui::FontCache g_fonts;

nfui::ThemeMode parse_theme_mode() noexcept {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) return nfui::ThemeMode::light;

    nfui::ThemeMode mode = nfui::ThemeMode::light;
    for (int i = 1; i < argc - 1; ++i) {
        if (wcscmp(argv[i], L"--theme") != 0) continue;
        const wchar_t* value = argv[i + 1];
        if (wcscmp(value, L"dark") == 0) {
            mode = nfui::ThemeMode::dark;
        } else if (wcscmp(value, L"high_contrast") == 0) {
            mode = nfui::ThemeMode::high_contrast;
        } else if (wcscmp(value, L"light") == 0) {
            mode = nfui::ThemeMode::light;
        }
        break;
    }
    LocalFree(argv);
    return mode;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_LBUTTONUP) {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (PtInRect(&kPrimaryButton, pt) != FALSE) {
            g_status_text = L"Primary action confirmed — minimal link works.";
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&kSecondaryButton, pt) != FALSE) {
            g_status_text =
                L"Learn more: see docs/INTEGRATION.md to link NativeFrameUI.";
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hwnd, &paint);

        RECT client{};
        GetClientRect(hwnd, &client);

        const int dpi = nfui::dpi_of(hwnd);
        const nfui::ThemePalette palette = nfui::theme_palette(g_theme_mode);
        const nfui::ThemeMetrics metrics = nfui::theme_metrics();

        nfui::fill_rect(hdc, client, palette.background);

        // Heading -- semibold, centred, product name.
        RECT heading{24, 24, client.right - 24, 56};
        nfui::draw_text(hdc, heading, L"NativeFrame UI",
                        g_fonts.semibold(dpi, 16),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Subtitle -- regular, centred, one line of context.
        RECT subtitle{24, 88, client.right - 24, 112};
        nfui::draw_text(hdc, subtitle,
                        L"A minimal native Win32 window using NativeFrameUI",
                        g_fonts.regular(dpi, nfui::font_pt::ui),
                        palette.text_secondary,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Primary action -- accent fill, semibold caption.
        nfui::fill_rounded_rect(hdc, kPrimaryButton, metrics.corner_radius_control,
                                palette.accent, palette.accent);
        nfui::draw_text(hdc, kPrimaryButton, L"Open Sample",
                        g_fonts.semibold(dpi, nfui::font_pt::ui),
                        palette.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Secondary action -- ghost button: transparent fill on background
        // with an accent border and accent text.
        nfui::fill_rounded_rect(hdc, kSecondaryButton, metrics.corner_radius_control,
                                palette.background, palette.accent);
        nfui::draw_text(hdc, kSecondaryButton, L"Learn more",
                        g_fonts.semibold(dpi, nfui::font_pt::ui),
                        palette.accent,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Status row -- secondary text, centred.
        RECT status_rect{24, 272, client.right - 24, 296};
        nfui::draw_text(hdc, status_rect, g_status_text,
                        g_fonts.regular(dpi, nfui::font_pt::ui),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        EndPaint(hwnd, &paint);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
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

    g_theme_mode = parse_theme_mode();
    const nfui::ThemePalette palette = nfui::theme_palette(g_theme_mode);

    HBRUSH background_brush = CreateSolidBrush(palette.background.rgb);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = background_brush;
    wc.lpszClassName = L"NFUI_MINIMAL";
    if (RegisterClassExW(&wc) == 0) return 2;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NativeFrameUI Minimal",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 480, 340,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) return 3;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

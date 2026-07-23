// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_control_base +
// nfui_button (NOT the NativeFrameUI umbrella). Verifies that consumers
// can ship a minimal product with a small dependency footprint.
//
// CP32: modern brand window with two owner-drawn buttons, the 4/8/12/16 grid,
// and the new font_pt scale. The HWND wrappers live as globals so their
// addresses stay stable until the parent window reaches WM_NCDESTROY.

#include <nfui/Application.hpp>
#include <nfui/Controls/Button.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>
#include <windowsx.h>

#include <string>

namespace {

constexpr int id_confirm = 101;
constexpr int id_reset   = 102;

// Same constant used by Control::subclass_proc to reflect owner-draw messages.
constexpr UINT ocm_base = WM_USER + 0x1c00;

// Global wrappers guarantee a stable object address for the lifetime of the
// HWND. Control::detach_destroyed_hwnd releases the HWND at WM_NCDESTROY,
// so the wrapper destructor does not call DestroyWindow on a dead handle.
nfui::Button g_confirm_button;
nfui::Button g_reset_button;
nfui::ThemePalette g_palette;
nfui::FontCache g_fonts;
nfui::DpiScale g_dpi{96};
std::wstring g_status = L"Press a button to confirm the minimal link set works.";
HWND g_hwnd{};
RECT g_status_rect{};

int px(int logical) noexcept {
    return g_dpi.logical_to_pixels(logical);
}

void layout() noexcept {
    if (g_hwnd == nullptr || g_confirm_button.hwnd() == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(g_hwnd, &client);
    g_dpi = nfui::DpiScale(nfui::dpi_of(g_hwnd));

    const int outer   = px(24);
    const int gap     = px(16);
    const int btn_w   = px(140);
    const int btn_h   = px(40);
    const int status_h = px(24);

    // Brand block: top-left, aligned to the grid.
    // (Painted in WM_PAINT; no child control needed.)

    // Status line rectangle; defined early so WM_PAINT can read it.
    g_status_rect = RECT{outer, client.bottom - outer - status_h,
                         client.right - outer, client.bottom - outer};

    // Two buttons centred in the remaining client area.
    const int total_w = btn_w * 2 + gap;
    const int x = (client.right - total_w) / 2;
    const int y = client.bottom / 2 - btn_h / 2;
    MoveWindow(g_confirm_button.hwnd(), x, y, btn_w, btn_h, TRUE);
    MoveWindow(g_reset_button.hwnd(), x + btn_w + gap, y, btn_w, btn_h, TRUE);

    // Refresh fonts on DPI changes.
    const int d = g_dpi.dpi();
    SendMessageW(g_confirm_button.hwnd(), WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_fonts.semibold(d, nfui::font_pt::md)), TRUE);
    SendMessageW(g_reset_button.hwnd(), WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_fonts.semibold(d, nfui::font_pt::md)), TRUE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        g_palette = nfui::theme_palette(nfui::ThemeMode::light);
        g_dpi = nfui::DpiScale(nfui::dpi_of(hwnd));

        g_confirm_button.inject_theme(&g_palette, &g_fonts);
        g_reset_button.inject_theme(&g_palette, &g_fonts);

        {
            nfui::ButtonStyle style{};
            style.use_semibold = true;
            g_confirm_button.set_style(style);
            g_reset_button.set_style(style);
        }

        {
            HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance;
            nfui::ControlCreateParams params{
                instance, hwnd, id_confirm, L"Confirm",
                0, 0, px(140), px(40), WS_CHILD | WS_VISIBLE | WS_TABSTOP
            };
            if (!g_confirm_button.create(params)) {
                return -1;
            }
            params.control_id = id_reset;
            params.text = L"Reset";
            if (!g_reset_button.create(params)) {
                return -1;
            }
        }
        layout();
        return 0;

    case WM_SIZE:
        layout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED: {
        auto* suggested = reinterpret_cast<RECT*>(lparam);
        if (suggested != nullptr) {
            SetWindowPos(hwnd, nullptr,
                         suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        layout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_COMMAND: {
        const int cmd = LOWORD(wparam);
        if (cmd == id_confirm) {
            g_status = L"NativeFrame UI is running with the minimal link set.";
            InvalidateRect(hwnd, &g_status_rect, FALSE);
            return 0;
        }
        if (cmd == id_reset) {
            g_status = L"Press a button to confirm the minimal link set works.";
            InvalidateRect(hwnd, &g_status_rect, FALSE);
            return 0;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        nfui::MemoryDC mem(hdc, client);
        HDC target = mem.valid() ? mem.dc() : hdc;

        nfui::fill_rect(target, client, g_palette.background);

        const int d = g_dpi.dpi();
        const int outer = px(24);

        // Brand title.
        RECT title_rc{outer, outer, client.right - outer, outer + px(40)};
        nfui::draw_text(target, title_rc, L"NativeFrame UI",
                        g_fonts.bold(d, nfui::font_pt::xl), g_palette.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Brand subtitle.
        RECT sub_rc{outer, outer + px(36), client.right - outer, outer + px(62)};
        nfui::draw_text(target, sub_rc, L"Minimal sample",
                        g_fonts.regular(d, nfui::font_pt::sm), g_palette.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Accent divider under the brand block.
        RECT divider{outer, outer + px(68), outer + px(120), outer + px(70)};
        nfui::fill_rect(target, divider, g_palette.accent);

        // Status line at the bottom of the window.
        if (g_status_rect.bottom > g_status_rect.top) {
            nfui::draw_text(target, g_status_rect, g_status,
                            g_fonts.regular(d, nfui::font_pt::base), g_palette.text,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    g_palette = nfui::theme_palette(nfui::ThemeMode::light);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(g_palette.background.rgb);
    wc.lpszClassName = L"NFUI_MINIMAL";
    if (RegisterClassExW(&wc) == 0) {
        return 2;
    }

    // Logical size 560 x 280; the create-time WM_CREATE path will scale and
    // centre the controls once the window DPI is known.
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NativeFrameUI Minimal",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 560, 280,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        return 3;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

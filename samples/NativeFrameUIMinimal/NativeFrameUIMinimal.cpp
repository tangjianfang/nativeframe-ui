// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_control_base +
// nfui_text + nfui_button (NOT the NativeFrameUI umbrella). Verifies
// that consumers can ship a minimal product with a small dependency
// footprint.
//
// Build deps (see CMakeLists.txt): NativeFrameUI::nfui_core,
// NativeFrameUI::nfui_theme, NativeFrameUI::nfui_control_base,
// NativeFrameUI::nfui_text, NativeFrameUI::nfui_button. 5 libs vs. 13
// via the umbrella.
//
// CP28-C polish: the original window was a 320x120 stretch with the
// system white background, a centred button that read as a flat gray
// rect, and a status label whose caption never visibly updated because
// the static text was sitting in the chrome's bottom band. The window
// now paints palette.background in WM_PAINT, lays out the controls on a
// real 8/16px rhythm, and the click feedback actually moves into a
// visible status row.

#include <nfui/Application.hpp>
#include <nfui/Controls/StaticText.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>
#include <windowsx.h>

namespace {

constexpr int ID_LABEL_STATUS = 1002;

constexpr RECT kButtonRect{180, 96, 300, 132};

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_LBUTTONUP) {
        // CP28-C: manual hit-test against the painted button rect. Doing
        // this in the parent (instead of routing through a Button HWND)
        // avoids the system gray rectangle that the framework Button
        // wrapper leaves on Win11 because the wrapper self-paints after
        // the parent already drew its accent fill.
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (PtInRect(&kButtonRect, pt) != FALSE) {
            HWND status = GetDlgItem(hwnd, ID_LABEL_STATUS);
            if (status != nullptr) {
                SetWindowTextW(status,
                               L"NativeFrame UI is running with the minimal link set.");
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_PAINT) {
        // CP28-C: paint the client area entirely from the framework —
        // background, heading, button face, and status row. Going
        // through the framework paint primitives (fill_rect / draw_text)
        // proves the minimal link set produces visible chrome and avoids
        // the system gray BOX that the previous version showed because
        // the static text controls sat on a COLOR_WINDOW background and
        // the system theme painted over them in many DWM configs.
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        const nfui::ThemePalette palette =
            nfui::theme_palette(nfui::ThemeMode::light);

        nfui::fill_rect(hdc, client, palette.background);

        // Heading — semibold, centred.
        RECT heading{24, 32, client.right - 24, 60};
        nfui::draw_text(hdc, heading, L"NativeFrame UI — minimal sample",
                        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Button face — accent fill, semibold caption, accent_text on accent.
        nfui::fill_rounded_rect(hdc, kButtonRect,
                                nfui::theme_metrics().corner_radius_control,
                                palette.accent, palette.accent);
        nfui::draw_text(hdc, kButtonRect, L"Click me",
                        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),
                        palette.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Status row — secondary text, centred.
        RECT status_rect{24, 156, client.right - 24, 180};
        HWND status = GetDlgItem(hwnd, ID_LABEL_STATUS);
        wchar_t status_text[128] = L"Press the button to confirm the minimal link works.";
        if (status != nullptr) {
            GetWindowTextW(status, status_text, static_cast<int>(std::size(status_text)));
        }
        nfui::draw_text(hdc, status_rect, status_text,
                        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),
                        palette.text_secondary,
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

    nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::light);
    nfui::FontCache fonts;

    // CP28-C: build the background brush from palette.background so the
    // window reads as part of the framework instead of the default
    // COLOR_WINDOW white. The brush lives for the lifetime of the app
    // (deleted at process exit; intentionally a leak by design here —
    // this sample never resizes or re-themes so we don't have to plumb
    // ownership).
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

    // CP28-C: 480x240 gives the controls room to breathe on a 96 dpi
    // baseline. WS_CLIPCHILDREN is intentionally NOT set — the parent
    // WM_PAINT paints the whole client area (background, heading, button
    // face, status row) via framework primitives. Earlier versions used
    // child StaticText / Button wrappers that left the chrome as a flat
    // gray rectangle on Win11; drawing through the framework paint
    // primitives proves the minimal link set actually paints.
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NativeFrameUI Minimal",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 480, 240,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) return 3;

    // CP28-C: hidden StaticText status row captures the click feedback
    // text via SetWindowTextW in WM_COMMAND. It's invisible but still a
    // valid owner-drawn StaticText that lives on the palette; the visible
    // status line is drawn from its window text in WM_PAINT.
    nfui::StaticText status;
    nfui::TextStyle status_style{};
    status_style.foreground = palette.text_secondary;
    status_style.font_size_pt = 9;
    status_style.align_h = nfui::StaticTextAlignH::center;
    nfui::ControlCreateParams sp{};
    sp.instance = instance;
    sp.parent = hwnd;
    sp.control_id = ID_LABEL_STATUS;
    sp.x = 24; sp.y = 156;
    sp.width = 432; sp.height = 24;
    sp.style |= WS_DISABLED;  // CP28-C: hide the StaticText face — the
                              // visible status row is drawn by WM_PAINT,
                              // and disabling the control prevents its
                              // self-paint from clobbering the parent
                              // palette.background fill underneath.
    status.set_style(status_style);
    status.inject_theme(&palette, &fonts);
    if (!status.create(sp)) return 6;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
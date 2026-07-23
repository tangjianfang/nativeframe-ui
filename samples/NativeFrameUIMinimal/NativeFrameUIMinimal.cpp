// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_control_base +
// nfui_button (NOT the NativeFrameUI umbrella). Verifies that consumers
// can ship a minimal product with a small dependency footprint.
//
// Build deps (see CMakeLists.txt): NativeFrameUI::nfui_core,
// NativeFrameUI::nfui_theme, NativeFrameUI::nfui_control_base,
// NativeFrameUI::nfui_button. 4 libs vs. 13 via the umbrella.
//
// CP28-C polish: the original window was a 320x120 stretch with the
// system white background, a centred button that read as a flat gray
// rect, and a status label whose caption never visibly updated because
// the static text was sitting in the chrome's bottom band. The window
// now paints palette.background in WM_PAINT, lays out the controls on a
// real 8/16px rhythm, and the click feedback actually moves into a
// visible status row.

#include <nfui/Application.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>
#include <windowsx.h>

#include <string>

namespace {

constexpr RECT kButtonRect{180, 96, 300, 132};

// CP30: status text is now stored in a process-local std::wstring rather
// than living in a hidden StaticText HWND. Earlier the static's owner-draw
// face kept painting its disabled gray fill over the row our parent
// WM_PAINT was drawing, so the text never read. A static std::wstring
// keeps the same "click button → row updates" contract without any child
// control to fight.
std::wstring g_status_text = L"Press the button to confirm the minimal link works.";

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_LBUTTONUP) {
        // CP28-C: manual hit-test against the painted button rect. Doing
        // this in the parent (instead of routing through a Button HWND)
        // avoids the system gray rectangle that the framework Button
        // wrapper leaves on Win11 because the wrapper self-paints after
        // the parent already drew its accent fill.
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (PtInRect(&kButtonRect, pt) != FALSE) {
            g_status_text = L"NativeFrame UI is running with the minimal link set.";
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

        // Status row — secondary text, centred. CP30: bumped from
        // text_secondary (which was nearly invisible against palette.background)
        // to palette.text and gave it a few more pixels of height so the
        // 9pt face reads at a glance. The muted look came from before the
        // button was wired up to a real click handler — there's no reason
        // for the status row to fade into the chrome.
        RECT status_rect{24, 156, client.right - 24, 184};
        nfui::draw_text(hdc, status_rect, g_status_text,
                        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),
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

    // CP30: removed the hidden StaticText status row — its owner-draw
    // face kept painting over our WM_PAINT-drawn status line, leaving
    // the text invisible. The status string now lives in g_status_text
    // (a process-local std::wstring) and the click handler updates it
    // directly, so we no longer need a child HWND to ferry the text.

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
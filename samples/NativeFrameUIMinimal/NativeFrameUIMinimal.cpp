// NativeFrameUIMinimal -- proves the per-component library split delivers
// value by linking ONLY nfui_core + nfui_theme + nfui_control_base +
// nfui_button (NOT the NativeFrameUI umbrella). Verifies that consumers
// can ship a minimal product with a small dependency footprint.
//
// CP32 redesign: replaced the flat left-aligned layout with a centered
// hero card. Vertical surface->background gradient, top-left coral N
// brand mark, centered xl title + sm subtitle, full-width coral primary
// button and outlined secondary button (both with visible hover/pressed
// state tracking). Layout is recomputed every paint from a logical grid
// (4/8/12/16/24 spacing) via DpiScale so proportions survive DPI bumps.
//
// Build deps (see CMakeLists.txt): NativeFrameUI::nfui_core,
// NativeFrameUI::nfui_theme, NativeFrameUI::nfui_control_base,
// NativeFrameUI::nfui_button. 4 libs vs. 13 via the umbrella.

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

// Hit-test rects are recomputed every WM_PAINT (they depend on window DPI),
// so the global hover/pressed flags and the captured mouse state are
// module-local and reconciled against the latest layout on each paint.
nfui::ThemeMode g_theme_mode = nfui::ThemeMode::light;
nfui::FontCache  g_fonts;
RECT g_primary_rect{};
RECT g_secondary_rect{};
bool g_primary_hot = false;
bool g_primary_pressed = false;
bool g_secondary_hot = false;
bool g_secondary_pressed = false;
bool g_capture_active = false;

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

// Recompute the hit-test rects from the design grid. Cached in module-level
// RECTs so the mouse handler can hit-test cheaply without rerunning the math.
void layout_column(const RECT& client, int dpi) noexcept {
    nfui::DpiScale dpi_(dpi);
    constexpr int kColWidthLogical = 480;
    int col_w = dpi_.logical_to_pixels(kColWidthLogical);
    if (col_w > client.right) col_w = client.right;
    const int col_x = (client.right - col_w) / 2;

    const int title_h      = dpi_.logical_to_pixels(40);   // line-box for xl 28 pt
    const int subtitle_h   = dpi_.logical_to_pixels(20);   // line-box for sm 13 pt
    const int btn_h        = dpi_.logical_to_pixels(40);
    const int gap_title_sub = dpi_.logical_to_pixels(8);
    const int gap_sub_btn1  = dpi_.logical_to_pixels(24);
    const int gap_btn1_btn2 = dpi_.logical_to_pixels(12);

    const int content_h = title_h + gap_title_sub + subtitle_h
                        + gap_sub_btn1 + btn_h
                        + gap_btn1_btn2 + btn_h;
    int y = (client.bottom - content_h) / 2;
    if (y < 0) y = 0;

    y += title_h + gap_title_sub + subtitle_h + gap_sub_btn1;
    g_primary_rect   = {col_x, y, col_x + col_w, y + btn_h};
    y += btn_h + gap_btn1_btn2;
    g_secondary_rect = {col_x, y, col_x + col_w, y + btn_h};
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_MOUSEMOVE) {
        const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const bool ph = PtInRect(&g_primary_rect, pt) != FALSE;
        const bool sh = PtInRect(&g_secondary_rect, pt) != FALSE;
        if (ph != g_primary_hot || sh != g_secondary_hot) {
            g_primary_hot = ph;
            g_secondary_hot = sh;
            if (!g_capture_active) {
                g_primary_pressed = g_primary_hot;
                g_secondary_pressed = g_secondary_hot;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_MOUSELEAVE) {
        if (g_primary_hot || g_secondary_hot || g_capture_active) {
            g_primary_hot = false;
            g_secondary_hot = false;
            if (!g_capture_active) {
                g_primary_pressed = false;
                g_secondary_pressed = false;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (PtInRect(&g_primary_rect, pt) != FALSE) {
            g_primary_pressed = true;
            g_capture_active = true;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_secondary_rect, pt) != FALSE) {
            g_secondary_pressed = true;
            g_capture_active = true;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP) {
        const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (g_capture_active) {
            g_capture_active = false;
            ReleaseCapture();
        }
        const bool was_primary_pressed = g_primary_pressed;
        const bool was_secondary_pressed = g_secondary_pressed;
        // Reconcile pressed state to the current pointer location -- a release
        // outside the button must clear the pressed highlight even without a
        // preceding WM_MOUSEMOVE.
        g_primary_hot = PtInRect(&g_primary_rect, pt) != FALSE;
        g_secondary_hot = PtInRect(&g_secondary_rect, pt) != FALSE;
        g_primary_pressed = g_primary_pressed && g_primary_hot;
        g_secondary_pressed = g_secondary_pressed && g_secondary_hot;
        if (was_primary_pressed || was_secondary_pressed ||
            g_primary_pressed != was_primary_pressed ||
            g_secondary_pressed != was_secondary_pressed) {
            InvalidateRect(hwnd, nullptr, FALSE);
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
        const nfui::ThemeMetrics metrics  = nfui::theme_metrics();
        nfui::DpiScale dpi_(dpi);

        layout_column(client, dpi);

        // Background -- vertical surface->background gradient so the chrome
        // gets a quiet, depth-suggesting lift at the top without competing
        // with the column content.
        nfui::paint_linear_gradient(hdc, client, 0, palette.surface, palette.background);

        // Brand mark -- 32 logical px coral rounded square with a soft
        // elevation-2 shadow, anchored at top-left with a 24 px gutter.
        const int pad = dpi_.logical_to_pixels(24);
        const int logo_size = dpi_.logical_to_pixels(32);
        const int logo_radius = dpi_.logical_to_pixels(8);
        const RECT logo{ pad, pad, pad + logo_size, pad + logo_size };
        nfui::paint_drop_shadow(hdc, logo, logo_radius, 2, palette.shadow);
        nfui::fill_rounded_rect(hdc, logo, logo_radius, palette.accent, palette.accent);
        // White "N" glyph rendered in the logo's own bold face, sized so it
        // visually fills the rounded square (about 20 logical pt).
        const int logo_glyph_pt = dpi_.logical_to_pixels(20);
        nfui::draw_text(hdc, logo, L"N",
                        g_fonts.bold(dpi, logo_glyph_pt),
                        palette.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Centered hero column -- title, subtitle, primary, secondary.
        constexpr int kColWidthLogical = 480;
        int col_w = dpi_.logical_to_pixels(kColWidthLogical);
        if (col_w > client.right) col_w = client.right;
        const int col_x = (client.right - col_w) / 2;

        const int title_h      = dpi_.logical_to_pixels(40);
        const int subtitle_h   = dpi_.logical_to_pixels(20);
        const int btn_h        = dpi_.logical_to_pixels(40);
        const int gap_title_sub = dpi_.logical_to_pixels(8);
        const int gap_sub_btn1  = dpi_.logical_to_pixels(24);
        const int gap_btn1_btn2 = dpi_.logical_to_pixels(12);
        const int content_h = title_h + gap_title_sub + subtitle_h
                            + gap_sub_btn1 + btn_h
                            + gap_btn1_btn2 + btn_h;
        int y = (client.bottom - content_h) / 2;
        if (y < 0) y = 0;

        RECT title_rect{ col_x, y, col_x + col_w, y + title_h };
        nfui::draw_text(hdc, title_rect, L"NativeFrame UI",
                        g_fonts.bold(dpi, nfui::font_pt::xl),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += title_h + gap_title_sub;

        RECT subtitle_rect{ col_x, y, col_x + col_w, y + subtitle_h };
        nfui::draw_text(hdc, subtitle_rect, L"A minimal sample for the native shell",
                        g_fonts.regular(dpi, nfui::font_pt::sm),
                        palette.text_secondary,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += subtitle_h + gap_sub_btn1;

        // Primary action -- accent fill, white text. Pressed (or hover) lifts
        // to accent_hover so the state is visually distinct from rest.
        RECT btn1{ col_x, y, col_x + col_w, y + btn_h };
        const nfui::Color primary_fill =
            g_primary_pressed
                ? palette.accent_hover
                : palette.accent;
        nfui::fill_rounded_rect(hdc, btn1, metrics.corner_radius_control,
                                primary_fill, primary_fill);
        nfui::draw_text(hdc, btn1, L"Open Sample",
                        g_fonts.semibold(dpi, nfui::font_pt::base),
                        palette.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += btn_h + gap_btn1_btn2;

        // Secondary action -- surface fill with a 1 px palette.border outline
        // so the "View on GitHub" button reads as a secondary tier against
        // the gradient background. Pressed swaps in surface_hover.
        RECT btn2{ col_x, y, col_x + col_w, y + btn_h };
        const nfui::Color second_fill =
            g_secondary_pressed
                ? palette.surface_hover
                : palette.surface;
        nfui::fill_rounded_rect(hdc, btn2, metrics.corner_radius_control,
                                second_fill, palette.border);
        nfui::draw_text(hdc, btn2, L"View on GitHub",
                        g_fonts.semibold(dpi, nfui::font_pt::base),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

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

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // paint_gradient handles the chrome
    wc.lpszClassName = L"NFUI_MINIMAL";
    if (RegisterClassExW(&wc) == 0) return 2;

    // Centre the demo on the desktop work area so the chrome always lands
    // in a predictable spot regardless of monitor layout.
    constexpr int kWinWidthLogical  = 640;
    constexpr int kWinHeightLogical = 460;
    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    const int wx = work_area.left + ((work_area.right  - work_area.left) - kWinWidthLogical) / 2;
    const int wy = work_area.top  + ((work_area.bottom - work_area.top)  - kWinHeightLogical) / 2;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NativeFrameUI Minimal",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                wx, wy, kWinWidthLogical, kWinHeightLogical,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) return 3;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

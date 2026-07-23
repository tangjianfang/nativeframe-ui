// CP32 sample: NativeFrameUIDialogTour
//
// A polished, product-quality wizard shell that demonstrates nfui::Dialog for
// both modal and modeless dialogs. The main window is a modern tour card:
// a hero header with the application mark, a title + subtitle, three clearly
// tiered actions, and a clean status strip.
//
// Visual contracts:
//   - 4/8/12/16 logical-pixel grid, scaled per-window DPI.
//   - Typography uses the CP32 scale: xs/sm/base/md/xl.
//   - Buttons are framework owner-drawn (rounded, gradient, hover cross-fade).
//   - The window client area and header are painted from ThemePalette.
//   - Every HWND wrapper is stable until WM_NCDESTROY.

#include <nfui/Application.hpp>
#include <nfui/Controls.hpp>
#include <nfui/Dialog.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Handle.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <algorithm>
#include <string>
#include <windowsx.h>

#include "NativeFrameUIResource.h"

namespace {

constexpr int IDC_LAUNCH_ABOUT = 1001;
constexpr int IDC_LAUNCH_PREFS = 1002;
constexpr int IDC_CLOSE_PREFS  = 1003;
constexpr int IDC_STATUS_LABEL = 1004;

// CP32 grid: all logical values; converted to device pixels at layout time.
constexpr int kWindowW     = 560;
constexpr int kWindowH     = 480;
constexpr int kMarginOuter = 24;
constexpr int kHeaderH     = 144;
constexpr int kIconSize    = 56;
constexpr int kIconGap     = 16;
constexpr int kButtonH     = 48;
constexpr int kButtonGap   = 16;
constexpr int kSectionGap  = 32;
constexpr int kStatusH     = 40;
constexpr int kStatusGap   = 8;

// Custom WM_USER message used by the prefs DLGPROC to deliver a submitted
// payload back to the main window. Routed via SendMessageW so the DLGPROC
// stays free of any static pointer to the TourWindow.
constexpr UINT WM_NFUI_PREFS_SUBMITTED = WM_USER + 1;

// The modeless dialog HWND lives in a process-wide slot because the message
// loop needs to feed it to IsDialogMessageW. Only one modeless dialog is
// alive at a time in this sample, so a single pointer is sufficient.
HWND g_modeless_dlg = nullptr;

struct PrefsPayload {
    std::wstring name;
    bool remember{};
    int theme_index{};
};

std::wstring format_payload(const PrefsPayload& p) {
    std::wstring s = L"name=\"";
    s += p.name.empty() ? L"<empty>" : p.name;
    s += L"\" remember=";
    s += p.remember ? L"true" : L"false";
    s += L" theme=";
    switch (p.theme_index) {
        case 0: s += L"light"; break;
        case 1: s += L"dark"; break;
        case 2: s += L"high_contrast"; break;
        default: s += L"unknown"; break;
    }
    return s;
}

class TourWindow : public nfui::Window {
public:
    explicit TourWindow(HINSTANCE instance)
        : instance_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {}

    ~TourWindow() noexcept override {
        destroy_icon();
    }

    bool create_main(int cmd_show) {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIDialogTourWindow",
            L"NativeFrame UI Dialog Tour",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            kWindowW,
            kWindowH,
        };
        if (!create(params)) {
            return false;
        }

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        apply_window_icon();

        if (!create_children()) {
            return false;
        }

        ShowWindow(hwnd(), cmd_show);
        UpdateWindow(hwnd());
        return true;
    }

    void launch_about() {
        // Modal path: blocks until the user dismisses the dialog.
        last_about_result_ = about_.show_modal(
            instance_, MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
            hwnd(), &TourWindow::about_dlg_proc);
        refresh_status();
    }

    void launch_prefs() {
        // OwnedHwnd::valid() only checks the cached pointer, not that the
        // HWND is still alive. When the prefs DLGPROC destroys the HWND
        // directly without going through prefs_.end_modeless(), the wrapper
        // keeps the dead pointer and a second click would silently no-op.
        // Use IsWindow() on the cached HWND so we either foreground the live
        // dialog or treat the slot as empty.
        if (prefs_.valid() && IsWindow(prefs_.hwnd()) != FALSE) {
            SetForegroundWindow(prefs_.hwnd());
            return;
        }
        HWND created = prefs_.show_modeless(
            instance_, MAKEINTRESOURCEW(IDD_NFUI_PREFS),
            hwnd(), &TourWindow::prefs_dlg_proc);
        if (created != nullptr) {
            ShowWindow(created, SW_SHOW);
            g_modeless_dlg = created;
        }
        refresh_status();
    }

    void close_prefs() {
        if (prefs_.valid() && IsWindow(prefs_.hwnd()) != FALSE) {
            prefs_.end_modeless(IDCANCEL);
            g_modeless_dlg = nullptr;
        } else {
            g_modeless_dlg = nullptr;
        }
        refresh_status();
    }

    // Called by the prefs DLGPROC via WM_NFUI_PREFS_SUBMITTED.
    void on_prefs_submitted(const std::wstring& payload) {
        last_payload_ = payload;
        refresh_status();
    }

protected:
    LRESULT handle_message(UINT msg, WPARAM wp, LPARAM lp) noexcept override {
        switch (msg) {
            case WM_SIZE:
                layout_children();
                return 0;
            case WM_DPICHANGED: {
                dpi_ = nfui::DpiScale(HIWORD(wp));
                auto* suggested = reinterpret_cast<RECT*>(lp);
                if (suggested != nullptr) {
                    SetWindowPos(hwnd(),
                                 nullptr,
                                 suggested->left,
                                 suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOACTIVATE | SWP_NOZORDER);
                }
                layout_children();
                InvalidateRect(hwnd(), nullptr, TRUE);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT paint{};
                HDC hdc = BeginPaint(hwnd(), &paint);
                RECT client{};
                GetClientRect(hwnd(), &client);
                {
                    nfui::MemoryDC mem(hdc, client);
                    HDC target = mem.valid() ? mem.dc() : hdc;
                    paint_background(target);
                }
                EndPaint(hwnd(), &paint);
                return 0;
            }
            case WM_COMMAND: {
                const WORD code = HIWORD(wp);
                const WORD id   = LOWORD(wp);
                if (code == BN_CLICKED) {
                    if (id == IDC_LAUNCH_ABOUT)      launch_about();
                    else if (id == IDC_LAUNCH_PREFS) launch_prefs();
                    else if (id == IDC_CLOSE_PREFS)  close_prefs();
                }
                return 0;
            }
            case WM_GETMINMAXINFO: {
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
                mmi->ptMinTrackSize.x = dpi_.logical_to_pixels(420);
                mmi->ptMinTrackSize.y = dpi_.logical_to_pixels(360);
                return 0;
            }
            case WM_NFUI_PREFS_SUBMITTED: {
                std::wstring payload;
                if (lp != 0) {
                    payload = reinterpret_cast<const wchar_t*>(lp);
                }
                on_prefs_submitted(payload);
                return 0;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return nfui::Window::handle_message(msg, wp, lp);
        }
    }

private:
    [[nodiscard]] bool create_children() noexcept {
        about_btn_.inject_theme(&palette_, &fonts_);
        prefs_btn_.inject_theme(&palette_, &fonts_);
        close_btn_.inject_theme(&palette_, &fonts_);
        status_label_.inject_theme(&palette_, &fonts_);

        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            IDC_LAUNCH_ABOUT,
            L"Show About (modal)",
            0, 0, 100, kButtonH,
        };

        nfui::ButtonStyle accent_style{};
        accent_style.use_semibold = true;
        about_btn_.set_style(accent_style);
        if (!about_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_LAUNCH_PREFS;
        params.text       = L"Show Preferences (modeless)";
        prefs_btn_.set_style(accent_style);
        if (!prefs_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_CLOSE_PREFS;
        params.text       = L"Close modeless";
        close_btn_.set_style(accent_style);
        if (!close_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_STATUS_LABEL;
        params.text       = L"";
        params.style      = WS_CHILD | WS_VISIBLE;
        if (!status_label_.create(params)) {
            return false;
        }

        nfui::TextStyle status_style{};
        status_style.foreground         = palette_.text_secondary;
        status_style.background         = palette_.background;
        status_style.font_size_pt       = nfui::font_pt::sm;
        status_style.align_v            = nfui::StaticTextAlignV::middle;
        status_style.horizontal_padding = 0;
        status_style.vertical_padding   = 0;
        status_label_.set_style(status_style);

        refresh_status();
        layout_children();
        return true;
    }

    void apply_window_icon() noexcept {
        if (app_icon_ != nullptr) {
            return;
        }
        app_icon_ = static_cast<HICON>(LoadImageW(
            instance_,
            MAKEINTRESOURCEW(IDI_NFUI_APP),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR));
        if (app_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL,
                         reinterpret_cast<WPARAM>(app_icon_));
        }
    }

    void destroy_icon() noexcept {
        if (app_icon_ != nullptr) {
            DestroyIcon(app_icon_);
            app_icon_ = nullptr;
        }
    }

    void refresh_status() {
        std::wstring text = L"about=";
        switch (last_about_result_) {
            case IDOK:     text += L"IDOK";     break;
            case IDCANCEL: text += L"IDCANCEL"; break;
            default:       text += L"unset";    break;
        }
        text += L"   prefs_open=";
        text += (prefs_.valid() ? L"yes" : L"no");
        text += L"   last=";
        text += last_payload_;
        status_label_.set_caption(text);
    }

    void layout_children() {
        if (hwnd() == nullptr) {
            return;
        }
        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        RECT rc{};
        GetClientRect(hwnd(), &rc);

        const int margin   = dpi_.logical_to_pixels(kMarginOuter);
        const int header_h = dpi_.logical_to_pixels(kHeaderH);
        const int button_h = dpi_.logical_to_pixels(kButtonH);
        const int button_w = (rc.right - rc.left) - 2 * margin;
        const int gap      = dpi_.logical_to_pixels(kButtonGap);
        const int section  = dpi_.logical_to_pixels(kSectionGap);
        const int status_h = dpi_.logical_to_pixels(kStatusH);
        const int status_y = rc.bottom - dpi_.logical_to_pixels(kMarginOuter) - status_h;

        int y = header_h + section;
        MoveWindow(about_btn_.hwnd(),  margin, y, button_w, button_h, TRUE);
        y += button_h + gap;
        MoveWindow(prefs_btn_.hwnd(), margin, y, button_w, button_h, TRUE);
        y += button_h + gap;
        MoveWindow(close_btn_.hwnd(), margin, y, button_w, button_h, TRUE);

        MoveWindow(status_label_.hwnd(),
                   margin,
                   status_y + dpi_.logical_to_pixels(kStatusGap),
                   button_w,
                   status_h - dpi_.logical_to_pixels(kStatusGap), TRUE);
    }

    void paint_background(HDC target) noexcept {
        const int dpi_value = dpi_.dpi();
        const nfui::ThemePalette& p = palette_;

        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        // Hero header: surface background with a 1px bottom divider.
        const int header_h = dpi_.logical_to_pixels(kHeaderH);
        RECT header{client.left, client.top, client.right, client.top + header_h};
        nfui::fill_rect(target, header, p.surface);
        const RECT divider{
            header.left,
            header.bottom - 1,
            header.right,
            header.bottom,
        };
        nfui::fill_rect(target, divider, p.border);

        // App mark on the left of the header: a rounded accent square with a
        // white "N". This is sample-local chrome; the framework icon resource
        // does not carry the same brand mark, so we paint it ourselves.
        const int icon_size = dpi_.logical_to_pixels(kIconSize);
        const int margin    = dpi_.logical_to_pixels(kMarginOuter);
        const int icon_x    = client.left + margin;
        const int icon_y    = client.top + (header_h - icon_size) / 2;
        const RECT mark_rc{icon_x, icon_y, icon_x + icon_size, icon_y + icon_size};
        const int mark_radius = nfui::theme_metrics().corner_radius_card;
        nfui::fill_rounded_rect(target, mark_rc, mark_radius, p.accent, p.accent);
        RECT mark_text_rc = mark_rc;
        HFONT mark_font = fonts_.bold(dpi_value, nfui::font_pt::xl);
        nfui::draw_text(target, mark_text_rc, L"N", mark_font, p.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Title and subtitle to the right of the mark.
        const int icon_gap = dpi_.logical_to_pixels(kIconGap);
        const int text_left = icon_x + icon_size + icon_gap;
        const int text_right = client.right - margin;

        RECT title_rc{
            text_left,
            client.top + dpi_.logical_to_pixels(24),
            text_right,
            client.top + dpi_.logical_to_pixels(60),
        };
        HFONT title_font = fonts_.bold(dpi_value, nfui::font_pt::xl);
        nfui::draw_text(target, title_rc, L"Dialog Tour", title_font, p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT subtitle_rc{
            text_left,
            title_rc.bottom + dpi_.logical_to_pixels(4),
            text_right,
            header.bottom - dpi_.logical_to_pixels(24),
        };
        HFONT subtitle_font = fonts_.regular(dpi_value, nfui::font_pt::base);
        nfui::draw_text(target, subtitle_rc,
                        L"Demonstrates modal, modeless, and lifecycle routing in a native shell.",
                        subtitle_font, p.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        // 1px divider above the status strip so it reads as anchored chrome.
        const int status_h = dpi_.logical_to_pixels(kStatusH);
        const int status_y = client.bottom - dpi_.logical_to_pixels(kMarginOuter) - status_h;
        nfui::draw_line(target,
                        {client.left + margin, status_y},
                        {client.right - margin, status_y},
                        p.border, 1);
    }

    static INT_PTR CALLBACK about_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
        if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        if (msg == WM_CLOSE) {
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }

    static INT_PTR CALLBACK prefs_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
        switch (msg) {
            case WM_INITDIALOG: {
                SetDlgItemTextW(dlg, IDC_NFUI_PREFS_NAME, L"Guest");
                HWND theme = GetDlgItem(dlg, IDC_NFUI_PREFS_THEME);
                SendMessageW(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
                SendMessageW(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
                SendMessageW(theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"High Contrast"));
                SendMessageW(theme, CB_SETCURSEL, 0, 0);
                return TRUE;
            }
            case WM_COMMAND: {
                const WORD code = HIWORD(wp);
                const WORD id   = LOWORD(wp);
                if (code == BN_CLICKED && id == IDOK) {
                    wchar_t name[128]{};
                    GetDlgItemTextW(dlg, IDC_NFUI_PREFS_NAME, name, 128);
                    PrefsPayload p{};
                    p.name        = name;
                    p.remember    = (SendDlgItemMessageW(dlg, IDC_NFUI_PREFS_REMEMBER,
                                       BM_GETCHECK, 0, 0) == BST_CHECKED);
                    p.theme_index = static_cast<int>(SendDlgItemMessageW(
                                       dlg, IDC_NFUI_PREFS_THEME, CB_GETCURSEL, 0, 0));
                    std::wstring encoded = format_payload(p);
                    HWND main_hwnd = GetParent(dlg);
                    if (main_hwnd != nullptr) {
                        SendMessageW(main_hwnd, WM_NFUI_PREFS_SUBMITTED, 0,
                                     reinterpret_cast<LPARAM>(encoded.c_str()));
                    }
                    DestroyWindow(dlg);
                    g_modeless_dlg = nullptr;
                    return TRUE;
                }
                if (code == BN_CLICKED && id == IDCANCEL) {
                    DestroyWindow(dlg);
                    g_modeless_dlg = nullptr;
                    return TRUE;
                }
                return FALSE;
            }
            case WM_CLOSE:
                DestroyWindow(dlg);
                g_modeless_dlg = nullptr;
                return TRUE;
            default:
                return FALSE;
        }
    }

    HINSTANCE instance_{};
    nfui::Dialog about_{};
    nfui::Dialog prefs_{};
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::Button about_btn_;
    nfui::Button prefs_btn_;
    nfui::Button close_btn_;
    nfui::StaticText status_label_;
    nfui::DpiScale dpi_{96};
    HICON app_icon_{};
    int last_about_result_{-1};
    std::wstring last_payload_{L"none"};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int cmd_show) {
    nfui::Application app({instance, cmd_show});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    TourWindow window(instance);
    if (!window.create_main(cmd_show)) {
        return 2;
    }

    // Standard message loop with modeless IsDialogMessage routing. We
    // intentionally do not use app.run() here because the dialog wrapper
    // is the entire point of the sample, and showing its plumbing keeps
    // the example copy-pasteable.
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (g_modeless_dlg != nullptr
            && IsDialogMessageW(g_modeless_dlg, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

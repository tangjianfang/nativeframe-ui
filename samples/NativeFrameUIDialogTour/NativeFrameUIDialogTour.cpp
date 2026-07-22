// CP12 sample: NativeFrameUIDialogTour
//
// Exercises the nfui::Dialog wrapper (src/core/Dialog.cpp + include/nfui/Dialog.hpp)
// in both modal and modeless modes. None of the existing ten samples uses
// nfui::Dialog directly; this fills that gap and gives consumers a copy-
// pasteable pattern for DLGPROC wiring, owned-hwnd modeless lifecycle, and
// the modal_result / end_modeless contracts.
//
// Three launch points on the main window:
//   - "Show About (modal)"           : Dialog::show_modal with IDD_NFUI_ABOUT
//   - "Show Preferences (modeless)"  : Dialog::show_modeless with IDD_NFUI_PREFS
//   - "Close modeless"               : end_modeless() for the prefs dialog
//
// A status strip at the bottom reports:
//   - last modal result (IDOK / IDCANCEL / -1)
//   - whether the modeless dialog is open
//   - last submitted payload (name + remember + theme)
//
// CP22: the three launch buttons and the status strip used to be raw
// CreateWindowExW BUTTON / STATIC with BS_PUSHBUTTON / SS_SUNKEN — they
// rendered in un-themed system chrome (COLOR_BTNFACE, Tahoma 8) which
// stood out against the polished dialog chrome. They now go through
// nfui::Button + nfui::StaticText wrappers with the same palette/font
// injection as SettingsDemo, and the window paints palette.background
// instead of letting the system show COLOR_BTNFACE.

#include <nfui/Application.hpp>
#include <nfui/Controls.hpp>
#include <nfui/Dialog.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Handle.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <string>
#include <windowsx.h>

#include "NativeFrameUIResource.h"

namespace {

constexpr int IDC_LAUNCH_ABOUT = 1001;
constexpr int IDC_LAUNCH_PREFS = 1002;
constexpr int IDC_CLOSE_PREFS  = 1003;
constexpr int IDC_STATUS_LABEL = 1004;

constexpr int kButtonW = 220;
constexpr int kButtonH = 30;
constexpr int kPadX    = 24;
constexpr int kPadY    = 24;

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

    bool create_main(int cmd_show) {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIDialogTourWindow",
            L"NativeFrame UI Dialog Tour",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            360,
            240,
        };
        if (!create(params)) {
            return false;
        }
        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        if (!create_children()) {
            return false;
        }
        ShowWindow(hwnd(), cmd_show);
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
        // CP22: OwnedHwnd::valid() only checks the cached pointer, not that
        // the HWND is still alive. When the prefs DLGPROC destroys the HWND
        // directly (IDOK / IDCANCEL / WM_CLOSE paths in prefs_dlg_proc
        // below) without going through prefs_.end_modeless(), the wrapper
        // keeps the dead pointer and a second click on "Show Preferences"
        // silently no-ops. Use IsWindow() on the cached HWND so we either
        // foreground the live dialog or treat the slot as empty.
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
        // CP22: same IsWindow() guard so close_prefs() doesn't try to
        // end_modeless() on a slot the DLGPROC already destroyed.
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
                apply_native_fonts();
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
                // CP22: paint palette.background over the entire client area
                // so the un-themed sections between the buttons read as part
                // of the surface, not COLOR_BTNFACE.
                {
                    nfui::MemoryDC mem(hdc, client);
                    HDC target = mem.valid() ? mem.dc() : hdc;
                    nfui::fill_rect(target, client, palette_.background);
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
                mmi->ptMinTrackSize.x = 320;
                mmi->ptMinTrackSize.y = 220;
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
        // CP22: bind every wrapper to the shared palette + FontCache before
        // create(), so the buttons adopt themed chrome and the status strip
        // gets the cached UI font via its WM_SETFONT path.
        about_btn_.inject_theme(&palette_, &fonts_);
        prefs_btn_.inject_theme(&palette_, &fonts_);
        close_btn_.inject_theme(&palette_, &fonts_);
        status_label_.inject_theme(&palette_, &fonts_);

        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            IDC_LAUNCH_ABOUT,
            L"Show About (modal)",
            0, 0, kButtonW, kButtonH,
        };
        if (!about_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_LAUNCH_PREFS;
        params.text       = L"Show Preferences (modeless)";
        if (!prefs_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_CLOSE_PREFS;
        params.text       = L"Close modeless";
        if (!close_btn_.create(params)) {
            return false;
        }

        params.control_id = IDC_STATUS_LABEL;
        params.text       = L"";
        params.style      = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        if (!status_label_.create(params)) {
            return false;
        }
        // CP22: subtle text colour for the status strip so it reads as
        // secondary chrome against the painted background. single_line=true
        // matches SettingsDemo's status_label.
        nfui::TextStyle status_style{};
        status_style.foreground     = palette_.text_secondary;
        status_style.background     = palette_.background;
        status_style.font_size_pt   = 9;
        status_style.align_v        = nfui::StaticTextAlignV::middle;
        status_style.horizontal_padding = 4;
        status_style.vertical_padding   = 2;
        status_label_.set_style(status_style);

        apply_native_fonts();
        refresh_status();
        layout_children();
        return true;
    }

    void apply_native_fonts() noexcept {
        // CP22: nfui::StaticText is self-painting but Button relies on
        // WM_SETFONT for its caption measure. Keep the cached UI font in
        // sync so the labels stay Segoe UI across DPI swaps.
        const HFONT ui_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::ui);
        SendMessageW(about_btn_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(prefs_btn_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(close_btn_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
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
        const int button_h = dpi_.logical_to_pixels(kButtonH);
        const int button_w = dpi_.logical_to_pixels(kButtonW);
        // CP22: center uses the DPI-scaled button_w so the buttons stay
        // centred at 125/150/175/200% DPI. Previously cx used the raw
        // logical kButtonW (=220) while the MoveWindow used the scaled
        // button_w, which at >96% DPI pushed the buttons right of centre.
        const int cx = (rc.right - rc.left - button_w) / 2;
        int y        = dpi_.logical_to_pixels(kPadY);
        const int button_gap = dpi_.logical_to_pixels(8);
        MoveWindow(about_btn_.hwnd(), cx, y, button_w, button_h, TRUE);
        y += button_h + button_gap;
        MoveWindow(prefs_btn_.hwnd(), cx, y, button_w, button_h, TRUE);
        y += button_h + button_gap;
        MoveWindow(close_btn_.hwnd(), cx, y, button_w, button_h, TRUE);
        y += button_h + dpi_.logical_to_pixels(12);
        // CP22: anchor the status strip to the bottom of the client area
        // so a tall window doesn't leave a blank palette.background band
        // below the status. The strip's height scales with DPI; the gap
        // between the strip and the window bottom matches kPadX.
        const int status_h = dpi_.logical_to_pixels(22);
        const int status_bottom_pad = dpi_.logical_to_pixels(kPadY);
        const int status_top = std::max(static_cast<int>(y), static_cast<int>(rc.bottom - status_bottom_pad - status_h));
        const int status_x = dpi_.logical_to_pixels(kPadX);
        const int status_w = rc.right - rc.left - 2 * status_x;
        MoveWindow(status_label_.hwnd(),
                   status_x,
                   status_top,
                   status_w,
                   status_h, TRUE);
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
                    // CP15: validation no longer pops a native MessageBoxW
                    // (the old chrome bypasses every visual contract in the
                    // framework). An empty name is encoded as "<empty>" in
                    // the payload so the main window's status strip reports
                    // the truth without flashing native chrome.
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
                    // Modeless path: DestroyWindow tears down the HWND that
                    // CreateDialogParamW allocated; the framework's
                    // OwnedHwnd RAII cleans up via WM_NCDESTROY.
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
    int last_about_result_{-1};
    std::wstring last_payload_{L"<none>"};
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
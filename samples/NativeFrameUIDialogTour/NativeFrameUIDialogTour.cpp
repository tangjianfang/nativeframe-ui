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

#include <nfui/Application.hpp>
#include <nfui/Dialog.hpp>
#include <nfui/Handle.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <string>
#include <windowsx.h>

#include "NativeFrameUIResource.h"

namespace {

constexpr int IDC_LAUNCH_ABOUT = 1001;
constexpr int IDC_LAUNCH_PREFS = 1002;
constexpr int IDC_CLOSE_PREFS  = 1003;

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
    explicit TourWindow(HINSTANCE instance) : instance_(instance) {}

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
            220,
        };
        if (!create(params)) {
            return false;
        }
        ShowWindow(hwnd(), cmd_show);
        build_status_strip();
        layout_children();
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
        if (prefs_.valid()) {
            // Already open — bring it to the foreground instead of stacking.
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
        if (prefs_.valid()) {
            prefs_.end_modeless(IDCANCEL);
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
            case WM_CREATE: {
                HWND about_btn = CreateWindowExW(
                    0, L"BUTTON", L"Show About (modal)",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0, 0, kButtonW, kButtonH,
                    hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LAUNCH_ABOUT)),
                    instance_, nullptr);
                HWND prefs_btn = CreateWindowExW(
                    0, L"BUTTON", L"Show Preferences (modeless)",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0, 0, kButtonW, kButtonH,
                    hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LAUNCH_PREFS)),
                    instance_, nullptr);
                HWND close_btn = CreateWindowExW(
                    0, L"BUTTON", L"Close modeless",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0, 0, kButtonW, kButtonH,
                    hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CLOSE_PREFS)),
                    instance_, nullptr);
                about_hwnd_ = about_btn;
                prefs_hwnd_ = prefs_btn;
                close_hwnd_ = close_btn;
                return 0;
            }
            case WM_SIZE:
                layout_children();
                return 0;
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
                mmi->ptMinTrackSize.y = 200;
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
    void build_status_strip() {
        status_hwnd_ = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
            0, 0, 0, 0,
            hwnd(), nullptr, instance_, nullptr);
        refresh_status();
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
        SetWindowTextW(status_hwnd_, text.c_str());
    }

    void layout_children() {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int cx = (rc.right - rc.left - kButtonW) / 2;
        int y        = kPadY;
        MoveWindow(about_hwnd_, cx, y, kButtonW, kButtonH, TRUE);
        y += kButtonH + 8;
        MoveWindow(prefs_hwnd_, cx, y, kButtonW, kButtonH, TRUE);
        y += kButtonH + 8;
        MoveWindow(close_hwnd_, cx, y, kButtonW, kButtonH, TRUE);
        y += kButtonH + 12;
        MoveWindow(status_hwnd_, kPadX, y,
                   rc.right - rc.left - 2 * kPadX,
                   rc.bottom - y - 8, TRUE);
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
                    if (name[0] == L'\0') {
                        MessageBoxW(dlg,
                            L"Please enter a display name.",
                            L"Preferences",
                            MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(dlg, IDC_NFUI_PREFS_NAME));
                        return TRUE;
                    }
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
    int last_about_result_{-1};
    std::wstring last_payload_{L"<none>"};
    HWND about_hwnd_{};
    HWND prefs_hwnd_{};
    HWND close_hwnd_{};
    HWND status_hwnd_{};
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
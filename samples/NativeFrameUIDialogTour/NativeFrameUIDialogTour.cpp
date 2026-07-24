// CP12 sample: NativeFrameUIDialogTour
//
// Exercises the nfui::Dialog wrapper (src/core/Dialog.cpp + include/nfui/Dialog.hpp)
// in both modal and modeless modes. None of the existing ten samples uses
// nfui::Dialog directly; this fills that gap and gives consumers a copy-
// pasteable pattern for DLGPROC wiring, owned-hwnd modeless lifecycle, and
// the modal_result / end_modeless contracts.
//
// Three launch points on the main window:
//   - "Show About (modal)"    : Dialog::show_modal with IDD_NFUI_ABOUT
//   - "Show Preferences..."   : Dialog::show_modeless with IDD_NFUI_PREFS
//   - "Close modeless"        : end_modeless() for the prefs dialog
//
// CP35 polish: the tour window is now a single centered card with a coral
// N brand mark, a 28pt page title, a muted description, three coral
// primary buttons, a 2px divider, and a mono debug line. The Dialog
// wrapper plumbing (modal_result, end_modeless, modeless DLGPROC routing)
// is unchanged; only the surface presentation is redrawn.

#include <nfui/Application.hpp>
#include <nfui/Dialog.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Handle.hpp>
#include <nfui/Icon.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <algorithm>
#include <string>
#include <windowsx.h>

#include "NativeFrameUIResource.h"

namespace {

// Custom WM_USER message used by the prefs DLGPROC to deliver a submitted
// payload back to the main window. Routed via SendMessageW so the DLGPROC
// stays free of any static pointer to the TourWindow.
constexpr UINT WM_NFUI_PREFS_SUBMITTED = WM_USER + 1;

// The modeless dialog HWND lives in a process-wide slot because the message
// loop needs to feed it to IsDialogMessageW. Only one modeless dialog is
// alive at a time in this sample, so a single pointer is sufficient.
HWND g_modeless_dlg = nullptr;

// CP35: card + window layout in logical px. The window is sized so the
// centered 640x480 card has even margins (~120 left/right, ~60 top/bottom).
// CP36: trim card dimensions to suit the unified 940×700 compact window.
// The previous 640×480 ran the buttons almost to the window borders; the
// smaller 480×360 card sits comfortably centred with breathing room.
constexpr int kCardW      = 480;
constexpr int kCardH      = 360;
// CP36: unified compact demo size — every demo window is 940×700 logical px
// so the surface fits any 1080-tall monitor with breathing room and the
// suite presents a consistent silhouette.
constexpr int kWindowW    = 940;
constexpr int kWindowH    = 700;
constexpr int kPadX       = 24;
constexpr int kPadY       = 24;
constexpr int kBrandSize  = 32;
constexpr int kBrandGap   = 16;
constexpr int kButtonH    = 44;
constexpr int kButtonGap  = 12;
constexpr int kDividerGap = 40;     // gap below last button
constexpr int kDebugGap   = 16;     // gap below divider

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

// CP35: hit-test cache for the three coral primary buttons. Each one
// remembers its physical (DPI-scaled) rectangle plus hover state. The
// main window owns these so the WM_PAINT path and the mouse path agree
// without going through child HWNDs.
struct ButtonSlot {
    RECT rect{};
    bool hover{false};
};

class TourWindow : public nfui::Window {
public:
    explicit TourWindow(HINSTANCE instance)
        : instance_(instance),
          theme_mode_(nfui::ThemeMode::light),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {}

    ~TourWindow() noexcept override = default;

    // CP32: lets wWinMain seed the mode before create_main paints the
    // chrome. Without this, --theme dark still captures light.
    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return;
        theme_mode_ = mode;
        palette_ = nfui::theme_palette(mode);
    }

    bool create_main(int cmd_show) {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIDialogTourWindow",
            L"NativeFrame UI Dialog Tour",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            kWindowW,
            kWindowH,
        };
        if (!create(params)) {
            return false;
        }
        apply_window_icon();
        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        layout_card();
        ShowWindow(hwnd(), cmd_show);
        return true;
    }

    void launch_about() {
        // CP35: record the event BEFORE the modal blocks, so a fast user
        // who watches the status line sees the click registered even if
        // the modal has not yet dismissed.
        last_event_ = L"about_open";
        invalidate_debug();
        last_about_result_ = about_.show_modal(
            instance_, MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
            hwnd(), &TourWindow::about_dlg_proc);
        last_event_ = (last_about_result_ == IDOK) ? L"about_ok"
                                                    : L"about_cancel";
        about_done_ = true;
        about_ok_   = (last_about_result_ == IDOK);
        invalidate_debug();
    }

    void launch_prefs() {
        // CP22: OwnedHwnd::valid() only checks the cached pointer, not that
        // the HWND is still alive. When the prefs DLGPROC destroys the HWND
        // directly (IDOK / IDCANCEL / WM_CLOSE paths in prefs_dlg_proc
        // below) without going through prefs_.end_modeless(), the wrapper
        // keeps the dead pointer and a second click on "Open Modeless"
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
            prefs_open_ = true;
            last_event_ = L"prefs_opened";
            invalidate_debug();
        }
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
        if (prefs_open_) {
            prefs_open_ = false;
            last_event_ = L"prefs_closed";
            invalidate_debug();
        }
    }

    // Called by the prefs DLGPROC via WM_NFUI_PREFS_SUBMITTED.
    void on_prefs_submitted(const std::wstring& payload) {
        last_payload_ = payload;
        last_event_ = L"prefs_submitted";
        invalidate_debug();
    }

protected:
    LRESULT handle_message(UINT msg, WPARAM wp, LPARAM lp) noexcept override {
        switch (msg) {
            case WM_SIZE:
                layout_card();
                return 0;
            case WM_DPICHANGED: {
                dpi_ = nfui::DpiScale(HIWORD(wp));
                layout_card();
                InvalidateRect(hwnd(), nullptr, TRUE);
                return 0;
            }
            case WM_MOUSEMOVE: {
                const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                update_hover(pt);
                return 0;
            }
            case WM_MOUSELEAVE: {
                if (about_btn_.hover || prefs_btn_.hover || close_btn_.hover) {
                    about_btn_.hover = false;
                    prefs_btn_.hover = false;
                    close_btn_.hover = false;
                    InvalidateRect(hwnd(), nullptr, FALSE);
                }
                tracking_leave_ = false;
                return 0;
            }
            case WM_LBUTTONDOWN: {
                // Capture the mouse so a drag that leaves the button still
                // releases inside our handler.
                SetCapture(hwnd());
                return 0;
            }
            case WM_LBUTTONUP: {
                ReleaseCapture();
                const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                if (PtInRect(&about_btn_.rect, pt)) {
                    launch_about();
                } else if (PtInRect(&prefs_btn_.rect, pt)) {
                    launch_prefs();
                } else if (PtInRect(&close_btn_.rect, pt)) {
                    close_prefs();
                }
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT paint{};
                HDC hdc = BeginPaint(hwnd(), &paint);
                RECT client{};
                GetClientRect(hwnd(), &client);
                paint_surface(hdc, client);
                EndPaint(hwnd(), &paint);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
                mmi->ptMinTrackSize.x = kWindowW;
                mmi->ptMinTrackSize.y = kWindowH;
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
    void apply_window_icon() noexcept {
        small_icon_ = nfui::IconHandle{static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR))};
        if (small_icon_.valid()) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL,
                         reinterpret_cast<LPARAM>(small_icon_.get()));
        }
        large_icon_ = nfui::IconHandle{static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
            LR_DEFAULTCOLOR))};
        if (large_icon_.valid()) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(large_icon_.get()));
        }
    }

    // CP35: recompute the card geometry + button rects from the current
    // DPI. The card stays centered in the client area; the inner padding
    // drives every downstream rectangle so a DPI swap cannot desync the
    // hit-test cache from what WM_PAINT draws.
    void layout_card() noexcept {
        if (hwnd() == nullptr) {
            return;
        }
        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);

        const int card_w = dpi_.logical_to_pixels(kCardW);
        const int card_h = dpi_.logical_to_pixels(kCardH);
        const int card_x = (client.right - client.left - card_w) / 2;
        const int card_y = (client.bottom - client.top - card_h) / 2;
        card_rect_ = {client.left + card_x, client.top + card_y,
                      client.left + card_x + card_w,
                      client.top + card_y + card_h};

        const int pad_x   = dpi_.logical_to_pixels(kPadX);
        const int brand   = dpi_.logical_to_pixels(kBrandSize);
        const int gap     = dpi_.logical_to_pixels(kBrandGap);
        const int btn_h   = dpi_.logical_to_pixels(kButtonH);
        const int btn_gap = dpi_.logical_to_pixels(kButtonGap);

        brand_rect_ = {
            card_rect_.left + pad_x,
            card_rect_.top  + pad_x,
            card_rect_.left + pad_x + brand,
            card_rect_.top  + pad_x + brand,
        };
        title_rect_ = {
            brand_rect_.right + gap,
            brand_rect_.top,
            card_rect_.right - pad_x,
            brand_rect_.bottom,
        };

        // Description block. Leave a comfortable gap below the brand row
        // (16 logical px) and let DT_WORDBREAK wrap to ~2 lines.
        const int desc_top = brand_rect_.bottom + dpi_.logical_to_pixels(16);
        const int desc_h   = dpi_.logical_to_pixels(40);
        desc_rect_ = {
            card_rect_.left + pad_x,
            desc_top,
            card_rect_.right - pad_x,
            desc_top + desc_h,
        };

        // Buttons: equal width across the inner card, stacked with the
        // design gap.
        const int btn_left  = card_rect_.left + pad_x;
        const int btn_right = card_rect_.right - pad_x;
        int y = desc_rect_.bottom + dpi_.logical_to_pixels(20);
        about_btn_.rect = {btn_left, y, btn_right, y + btn_h};
        y += btn_h + btn_gap;
        prefs_btn_.rect = {btn_left, y, btn_right, y + btn_h};
        y += btn_h + btn_gap;
        close_btn_.rect = {btn_left, y, btn_right, y + btn_h};

        divider_y_ = close_btn_.rect.bottom + dpi_.logical_to_pixels(kDividerGap);
        debug_rect_ = {
            card_rect_.left + pad_x,
            divider_y_ + dpi_.logical_to_pixels(2) + dpi_.logical_to_pixels(kDebugGap),
            card_rect_.right - pad_x,
            card_rect_.bottom - pad_x,
        };
    }

    // CP35: drive the card chrome + content in a single WM_PAINT path.
    // Everything is a fill_rounded_rect + draw_text, so there is no
    // dependency on the framework's nfui::Button / nfui::StaticText
    // chrome — the sample owns its presentation.
    void paint_surface(HDC hdc, const RECT& client) noexcept {
        nfui::MemoryDC mem(hdc, client);
        HDC target = mem.valid() ? mem.dc() : hdc;

        // 1. Window background. This is the canvas behind the card.
        nfui::fill_rect(target, client, palette_.background);

        // 2. Card drop shadow (elevation 1) then the card itself.
        // CP34: dropped elevation from 2 → 1 so the shadow's alpha falloff
        // at the bottom-right rounded corner no longer reads as a faint
        // diagonal "grip" mark in the captured screenshot. With blur=2
        // (elevation 1) the shadow's per-corner pixels stay subtle and the
        // panel corner reads as a clean rounded curve.
        const int radius = dpi_.logical_to_pixels(12);
        nfui::paint_drop_shadow(target, card_rect_, radius, 1, palette_.shadow);
        nfui::fill_rounded_rect(target, card_rect_, radius,
                                palette_.surface, palette_.border);

        // 3. N brand square. Coral fill with the design radius (6 logical
        // px = small pill, matches the rest of the framework's buttons).
        const int brand_radius = dpi_.logical_to_pixels(6);
        nfui::fill_rounded_rect(target, brand_rect_, brand_radius,
                                palette_.accent, palette_.accent);

        // Draw "N" centred in the square using the brand-mark font (bold
        // xl). Use a slightly smaller pt so the glyph clears the square's
        // edges on every DPI.
        HFONT brand_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::lg);
        nfui::draw_text(target, brand_rect_, L"N", brand_font,
                        palette_.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 4. Page title. xl bold, left aligned next to the brand square.
        HFONT title_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::xl);
        nfui::draw_text(target, title_rect_, L"Dialog Tour", title_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 5. Description. sm muted, word-wrapped.
        HFONT desc_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        nfui::draw_text(target, desc_rect_,
                        L"Click each button to see modal, modeless, and "
                        L"dismissed dialogs. The status line below tracks the "
                        L"last event.",
                        desc_font, palette_.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        // 6. Three stacked coral buttons. Hover swaps to accent_hover.
        const int btn_radius = dpi_.logical_to_pixels(6);
        HFONT btn_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::base);
        paint_button(target, about_btn_, L"Show About (modal)",
                     btn_radius, btn_font);
        paint_button(target, prefs_btn_, L"Show Preferences (modeless)",
                     btn_radius, btn_font);
        paint_button(target, close_btn_, L"Close modeless",
                     btn_radius, btn_font);

        // 7. 2px horizontal divider just below the button group.
        const int div_top    = divider_y_;
        const int div_bottom = div_top + dpi_.logical_to_pixels(2);
        RECT divider_rect{card_rect_.left + dpi_.logical_to_pixels(kPadX),
                          div_top,
                          card_rect_.right - dpi_.logical_to_pixels(kPadX),
                          div_bottom};
        nfui::fill_rect(target, divider_rect, palette_.border);

        // 8. Debug line. xs mono, text_secondary. The format is fixed and
        // the only field that varies per click is `last=`.
        HFONT debug_font = fonts_.mono(dpi_.dpi(), nfui::font_pt::xs);
        nfui::draw_text(target, debug_rect_, debug_text(), debug_font,
                        palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_button(HDC target, const ButtonSlot& slot,
                      const wchar_t* label, int radius, HFONT font) noexcept {
        const nfui::Color fill = slot.hover ? palette_.accent_hover
                                            : palette_.accent;
        nfui::fill_rounded_rect(target, slot.rect, radius, fill, fill);
        nfui::draw_text(target, slot.rect, label, font,
                        palette_.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    [[nodiscard]] std::wstring debug_text() const {
        std::wstring text = L"about=";
        if (!about_done_) {
            text += L"unset";
        } else {
            text += about_ok_ ? L"ok" : L"cancel";
        }
        text += prefs_open_ ? L" prefs_open=yes" : L" prefs_open=no";
        text += L" last=";
        text += last_event_;
        return text;
    }

    void invalidate_debug() noexcept {
        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }

    // CP35: re-evaluate hover state for the three buttons against the
    // cursor position, then redraw whatever changed. Arms a one-shot
    // WM_MOUSELEAVE so a cursor that leaves the window still clears the
    // highlight.
    void update_hover(POINT pt) noexcept {
        const bool about_hit  = PtInRect(&about_btn_.rect,  pt) != FALSE;
        const bool prefs_hit  = PtInRect(&prefs_btn_.rect,  pt) != FALSE;
        const bool close_hit  = PtInRect(&close_btn_.rect,  pt) != FALSE;
        if (about_hit  != about_btn_.hover ||
            prefs_hit  != prefs_btn_.hover ||
            close_hit  != close_btn_.hover) {
            about_btn_.hover = about_hit;
            prefs_btn_.hover = prefs_hit;
            close_btn_.hover = close_hit;
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
        if (!tracking_leave_ && (about_hit || prefs_hit || close_hit)) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = hwnd();
            TrackMouseEvent(&tme);
            tracking_leave_ = true;
        }
    }

    static INT_PTR CALLBACK about_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
        if (msg == WM_INITDIALOG) {
            // CP31: present the dismiss action as a plain close label instead
            // of the stock "OK" jargon.
            SetDlgItemTextW(dlg, IDOK, L"Close");
            return TRUE;
        }
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
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};
    nfui::FontCache fonts_;
    nfui::IconHandle small_icon_;
    nfui::IconHandle large_icon_;
    nfui::DpiScale dpi_{96};

    // CP35: card geometry + hit-test cache. All rectangles are device
    // pixels; layout_card() refreshes them on every WM_SIZE / WM_DPICHANGED.
    RECT    card_rect_{};
    RECT    brand_rect_{};
    RECT    title_rect_{};
    RECT    desc_rect_{};
    int     divider_y_{};
    RECT    debug_rect_{};
    ButtonSlot about_btn_{};
    ButtonSlot prefs_btn_{};
    ButtonSlot close_btn_{};
    bool    tracking_leave_{false};

    // CP35: state driving the debug line.
    bool         about_done_{false};
    bool         about_ok_{false};
    bool         prefs_open_{false};
    std::wstring last_event_{L"none"};
    std::wstring last_payload_{L"<none>"};
    int          last_about_result_{-1};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR cmd_line, int cmd_show) {
    nfui::Application app({instance, cmd_show});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme seeds the palette before create_main. Audit quotes
    // the value (--theme "dark"); strip the leading quote.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::light;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::light;
        tag += 7;
        while (*tag == L' ' || *tag == L'\t') ++tag;
        if (*tag == L'"') ++tag;
        if (wcsncmp(tag, L"dark", 4) == 0 && (tag[4] == L' ' || tag[4] == 0 || tag[4] == L'"')) return nfui::ThemeMode::dark;
        if (wcsncmp(tag, L"high_contrast", 13) == 0) return nfui::ThemeMode::high_contrast;
        return nfui::ThemeMode::light;
    };
    const nfui::ThemeMode initial_mode = parse_theme(cmd_line);

    TourWindow window(instance);
    window.set_initial_theme(initial_mode);
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
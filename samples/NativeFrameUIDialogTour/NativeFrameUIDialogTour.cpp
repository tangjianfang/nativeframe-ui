// CP-B2 sample: NativeFrameUIDialogTour
//
// Polished from the original "3-button stack + debug string" launcher
// into a real product tour (title + description + primary / secondary
// actions + status card). The Dialog wrapper plumbing (modal_result,
// end_modeless, modeless DLGPROC routing) is unchanged; the visible
// surface now reads as a product UI.
//
// Tour model (see TourStage.hpp):
//   - Primary  (filled accent, control_height_lg): "Show About (modal)"
//   - Secondary (ghost outline, control_height_md): "Show Preferences..."
//   - Tertiary  (text-link style, control_height_md): "Close active dialog"
//                — only visible when the modeless is open (CP-B2 fix).
//   - Status card: stage label + last submitted payload + runtime theme
//     switcher (Light / Dark / HC). Replaces the previous
//     "about=unset prefs_open=no last=<none>" debug string.
//
// Theme plumbing: the polished sample routes `--theme X` argv through
// ThemeBroker (CP-A1) and registers its HWND so the cross-window
// `WM_THEMECHANGED` broadcast re-paints the customer area on every
// runtime theme switch. The runtime theme chips in the status card
// call `ThemeBroker::instance().set_theme(...)` for the same reason.

#include <nfui/Application.hpp>
#include <nfui/Dialog.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Handle.hpp>
#include <nfui/Icon.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/Window.hpp>
#include <nfui/design_tokens.hpp>

#include "NativeFrameUIResource.h"
#include "TourStage.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <windowsx.h>

namespace {

// CP-B2: custom WM_USER message used by the prefs DLGPROC to deliver
// a submitted payload back to the main window. Routed via SendMessageW
// so the DLGPROC stays free of any static pointer to the TourWindow.
constexpr UINT WM_NFUI_PREFS_SUBMITTED = WM_USER + 1;

// CP-B2: the modeless dialog HWND lives in a process-wide slot
// because the message loop needs to feed it to IsDialogMessageW. Only
// one modeless dialog is alive at a time in this sample, so a single
// pointer is sufficient.
HWND g_modeless_dlg = nullptr;

// CP-B2: theme command IDs reuse the framework's tokens from
// <nfui/ThemeBroker.hpp>. The broker is the single source of truth
// for the process-wide theme; routing through it ensures every chrome
// HWND receives WM_THEMECHANGED on the same message-loop turn.
constexpr int idm_theme_light   = nfui::ID_THEME_LIGHT;
constexpr int idm_theme_dark    = nfui::ID_THEME_DARK;
constexpr int idm_theme_hc      = nfui::ID_THEME_HIGH_CONTRAST;

// CP-B2: hit-test IDs for the three action buttons + the three theme
// chips inside the status card. Negative IDs avoid colliding with any
// real Win32 command ID — the WindowProc routes them through the
// hit-test cache, not through WM_COMMAND.
constexpr int kHitNone       = 0;
constexpr int kHitPrimary    = 1;
constexpr int kHitSecondary  = 2;
constexpr int kHitTertiary   = 3;
constexpr int kHitThemeLight = 4;
constexpr int kHitThemeDark  = 5;
constexpr int kHitThemeHc    = 6;

// CP-B2: card geometry in logical px. The card is centered in the
// customer area; the inner padding drives every downstream rectangle
// so a DPI swap cannot desync the hit-test cache from what WM_PAINT
// draws. The card is sized to fit the welcome copy + a primary /
// secondary / (optional) tertiary action stack + a status card with
// a label, a payload, and a theme chip row.
constexpr int kCardW   = 560;
constexpr int kCardH   = 480;
constexpr int kWindowW = 940;
constexpr int kWindowH = 700;

// CP-B2: brand mark + divider dimensions. All sizes are logical;
// DpiScale resolves them at layout time.
constexpr int kBrandSize  = 32;
constexpr int kBrandGap   = 16;

// CP-B2: design-token-driven vertical rhythm. Layout reads the
// design tokens by name rather than magic numbers so a token bump
// flows through without a layout rewrite.
constexpr int kDescHeight   = 40;                                   // 2 lines of sm
constexpr int kStatusCardH  = 152;                                  // label + payload + theme row
constexpr int kThemeChipGap = nfui::design::spacing_xs;             // 4
constexpr int kDividerH     = 1;
constexpr int kPayloadGap   = nfui::design::spacing_sm;             // 8

// CP-B2: hit-test cache entry. Each button / chip shares a single
// slot struct so the WM_PAINT path and the mouse path agree without
// going through child HWNDs.
struct ButtonSlot {
    RECT rect{};
    bool hover{false};
};

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
          mode_(nfui::ThemeMode::light),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {}

    ~TourWindow() noexcept override {
        // CP-B2: unregister from ThemeBroker so the broker's HWND list
        // doesn't keep a dangling pointer after the window is destroyed.
        if (hwnd() != nullptr) {
            nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
        }
    }

    // CP-B2: seeds the broker + the local palette before create_main
    // paints the chrome. Without this, --theme dark would first paint
    // light, then the broker's later broadcast would only repaint the
    // framework's wrapped controls — the sample's own paint path would
    // stay light. set_theme() is idempotent (no-op when broker already
    // matches), so seeding here is safe.
    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        nfui::ThemeBroker::instance().set_theme(mode);
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
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

        // CP-B2: register the HWND with ThemeBroker so the broker's
        // WM_THEMECHANGED broadcast reaches this window on every
        // runtime theme switch. Unregister is in WM_NCDESTROY via
        // the destructor guard — the broker handles HWND lifetime
        // (HWND is the registry key, no C++ pointer stored).
        nfui::ThemeBroker::instance().register_hwnd(
            hwnd(), [this](nfui::ThemeMode mode) { apply_theme(mode); });

        layout_card();
        ShowWindow(hwnd(), cmd_show);
        return true;
    }

    void launch_about() {
        // CP-B2: record the event BEFORE the modal blocks, so a fast
        // user who watches the status card sees the click registered
        // even if the modal has not yet dismissed.
        stage_ = dialog_tour::TourStage::about_opened;
        invalidate_redraw();
        last_about_result_ = about_.show_modal(
            instance_, MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
            hwnd(), &TourWindow::about_dlg_proc);
        stage_ = (last_about_result_ == IDOK)
                     ? dialog_tour::TourStage::about_ok
                     : dialog_tour::TourStage::about_cancel;
        about_done_ = true;
        about_ok_   = (last_about_result_ == IDOK);
        invalidate_redraw();
    }

    void launch_prefs() {
        // CP-B2: same IsWindow() guard as before — OwnedHwnd::valid()
        // only checks the cached pointer, not that the HWND is still
        // alive. When the prefs DLGPROC destroys the HWND directly
        // (IDOK / IDCANCEL / WM_CLOSE) without going through
        // prefs_.end_modeless(), the wrapper keeps a dead pointer and
        // a second click on "Show Preferences" silently no-ops.
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
            stage_ = dialog_tour::TourStage::prefs_opened;
            invalidate_redraw();
        }
    }

    void close_active() {
        // CP-B2: combined "close modeless" + "dismiss modal" path.
        // The modeless is the only dialog the user can drive the
        // tertiary close on (the modal About closes itself). We could
        // also reach here from the prefs dialog's WM_CLOSE path.
        if (prefs_.valid() && IsWindow(prefs_.hwnd()) != FALSE) {
            prefs_.end_modeless(IDCANCEL);
        }
        g_modeless_dlg = nullptr;
        prefs_open_ = false;
        stage_ = dialog_tour::TourStage::prefs_closed;
        invalidate_redraw();
    }

    void on_prefs_submitted(const std::wstring& payload) {
        last_payload_ = payload;
        stage_ = dialog_tour::TourStage::prefs_submitted;
        invalidate_redraw();
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
            // CP-B2: ThemeBroker broadcasts WM_THEMECHANGED to every
            // registered HWND when set_theme() is called. Resync the
            // local palette + redraw so the customer area tracks the
            // broker. The broker's callback (registered in
            // create_main) drives apply_theme() directly; this arm
            // is here for completeness + in case the window is
            // targeted by an external broker (e.g. another demo).
            case WM_THEMECHANGED:
                apply_theme(nfui::ThemeBroker::instance().current());
                return 0;
            case WM_MOUSEMOVE: {
                const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                update_hover(pt);
                return 0;
            }
            case WM_MOUSELEAVE: {
                if (primary_slot_.hover || secondary_slot_.hover ||
                    close_slot_.hover   || theme_light_slot_.hover ||
                    theme_dark_slot_.hover || theme_hc_slot_.hover) {
                    primary_slot_.hover = false;
                    secondary_slot_.hover = false;
                    close_slot_.hover = false;
                    theme_light_slot_.hover = false;
                    theme_dark_slot_.hover = false;
                    theme_hc_slot_.hover = false;
                    InvalidateRect(hwnd(), nullptr, FALSE);
                }
                tracking_leave_ = false;
                return 0;
            }
            case WM_LBUTTONDOWN: {
                // Capture the mouse so a drag that leaves the button
                // still releases inside our handler.
                SetCapture(hwnd());
                return 0;
            }
            case WM_LBUTTONUP: {
                ReleaseCapture();
                const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                const int hit = hit_test(pt);
                switch (hit) {
                    case kHitPrimary:    launch_about(); break;
                    case kHitSecondary:  launch_prefs(); break;
                    case kHitTertiary:   close_active(); break;
                    case kHitThemeLight:
                        nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::light);
                        break;
                    case kHitThemeDark:
                        nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::dark);
                        break;
                    case kHitThemeHc:
                        nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::high_contrast);
                        break;
                    default: break;
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
            case WM_NCDESTROY:
                // CP-B2: unregister from the broker so a later
                // set_theme() does not dereference this HWND.
                nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
                return nfui::Window::handle_message(msg, wp, lp);
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

    // CP-B2: refresh the local palette + chrome. The ThemeBroker
    // callback fires on every runtime theme switch; the public
    // set_initial_theme() also calls this on create. The workbench
    // mirrors this exact pattern (see NativeFrameUIWorkbench.cpp
    // apply_theme).
    void apply_theme(nfui::ThemeMode mode) noexcept {
        if (mode_ == mode) {
            return;
        }
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // CP-B2: recompute the card geometry + button rects from the
    // current DPI. The card stays centered in the client area; the
    // inner padding drives every downstream rectangle so a DPI swap
    // cannot desync the hit-test cache from what WM_PAINT draws.
    void layout_card() noexcept {
        if (hwnd() == nullptr) {
            return;
        }
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);

        const int card_w = dpi_.logical_to_pixels(kCardW);
        const int card_h = dpi_.logical_to_pixels(kCardH);
        const int card_x = (client.right - client.left - card_w) / 2;
        const int card_y = (client.bottom - client.top - card_h) / 2;
        card_rect_ = {client.left + card_x, client.top + card_y,
                      client.left + card_x + card_w,
                      client.top + card_y + card_h};

        const int pad_x   = dpi_.logical_to_pixels(nfui::design::spacing_lg);
        const int pad_y   = dpi_.logical_to_pixels(nfui::design::spacing_lg);
        const int brand   = dpi_.logical_to_pixels(kBrandSize);
        const int gap     = dpi_.logical_to_pixels(kBrandGap);

        brand_rect_ = {
            card_rect_.left + pad_x,
            card_rect_.top  + pad_y,
            card_rect_.left + pad_x + brand,
            card_rect_.top  + pad_y + brand,
        };
        title_rect_ = {
            brand_rect_.right + gap,
            brand_rect_.top,
            card_rect_.right - pad_x,
            brand_rect_.bottom,
        };

        // Description block. Two lines of sm/body text wrapped to
        // the card's inner width.
        const int desc_top = brand_rect_.bottom + dpi_.logical_to_pixels(nfui::design::spacing_md);
        const int desc_h   = dpi_.logical_to_pixels(kDescHeight);
        desc_rect_ = {
            card_rect_.left + pad_x,
            desc_top,
            card_rect_.right - pad_x,
            desc_top + desc_h,
        };

        // Buttons: full inner width, stacked on the design rhythm.
        const int btn_left  = card_rect_.left + pad_x;
        const int btn_right = card_rect_.right - pad_x;
        const int primary_h = dpi_.logical_to_pixels(nfui::design::control_height_lg);
        const int secondary_h = dpi_.logical_to_pixels(nfui::design::control_height_md);
        const int btn_gap   = dpi_.logical_to_pixels(nfui::design::spacing_sm);

        int y = desc_rect_.bottom + dpi_.logical_to_pixels(nfui::design::spacing_lg);
        primary_slot_.rect = {btn_left, y, btn_right, y + primary_h};
        y = primary_slot_.rect.bottom + btn_gap;
        secondary_slot_.rect = {btn_left, y, btn_right, y + secondary_h};
        y = secondary_slot_.rect.bottom + btn_gap;

        // CP-B2: tertiary button is contextual — only when the
        // modeless is open. Layout reserved space; the paint path
        // skips drawing when !show_close_action(stage_).
        close_slot_.rect = {btn_left, y, btn_right, y + secondary_h};
        const int close_bottom = show_close_action(stage_)
                                     ? close_slot_.rect.bottom
                                     : secondary_slot_.rect.bottom;

        // Divider + status card sit below the action stack.
        const int divider_y = close_bottom + dpi_.logical_to_pixels(nfui::design::spacing_lg);
        const int divider_h = dpi_.logical_to_pixels(kDividerH);
        divider_rect_ = {
            card_rect_.left + pad_x,
            divider_y,
            card_rect_.right - pad_x,
            divider_y + divider_h,
        };

        const int status_y = divider_rect_.bottom + dpi_.logical_to_pixels(nfui::design::spacing_md);
        const int status_h = dpi_.logical_to_pixels(kStatusCardH);
        status_card_rect_ = {
            card_rect_.left + pad_x,
            status_y,
            card_rect_.right - pad_x,
            status_y + status_h,
        };
        layout_status_card();
    }

    // CP-B2: layout the inner status card. Two eyebrow rows (stage
    // label + payload) and a theme chip row at the bottom.
    void layout_status_card() noexcept {
        const int pad_x = dpi_.logical_to_pixels(nfui::design::spacing_md);
        const int pad_y = dpi_.logical_to_pixels(nfui::design::spacing_sm);
        const int chip_h = dpi_.logical_to_pixels(nfui::design::control_height_sm);
        const int gap_xs = dpi_.logical_to_pixels(kThemeChipGap);

        // Eyebrow + label rows. The eyebrow is a single line of xs
        // caps; the label is a single line of sm primary that may
        // ellipsize. Height fixed so the chip row at the bottom is
        // anchored.
        const int eyebrow_h = dpi_.logical_to_pixels(nfui::design::font_caption);
        const int label_h   = dpi_.logical_to_pixels(nfui::design::font_body);

        int y = status_card_rect_.top + pad_y;
        stage_eyebrow_rect_ = {
            status_card_rect_.left + pad_x,
            y,
            status_card_rect_.right - pad_x,
            y + eyebrow_h,
        };
        y = stage_eyebrow_rect_.bottom + dpi_.logical_to_pixels(kPayloadGap);
        stage_label_rect_ = {
            status_card_rect_.left + pad_x,
            y,
            status_card_rect_.right - pad_x,
            y + label_h,
        };
        y = stage_label_rect_.bottom + dpi_.logical_to_pixels(nfui::design::spacing_md);
        payload_eyebrow_rect_ = {
            status_card_rect_.left + pad_x,
            y,
            status_card_rect_.right - pad_x,
            y + eyebrow_h,
        };
        y = payload_eyebrow_rect_.bottom + dpi_.logical_to_pixels(kPayloadGap);
        payload_rect_ = {
            status_card_rect_.left + pad_x,
            y,
            status_card_rect_.right - pad_x,
            y + label_h,
        };

        // Theme chip row anchored to the bottom of the status card.
        const int theme_y = status_card_rect_.bottom - pad_y - chip_h;
        theme_eyebrow_rect_ = {
            status_card_rect_.left + pad_x,
            theme_y - eyebrow_h - dpi_.logical_to_pixels(kPayloadGap),
            status_card_rect_.right - pad_x,
            theme_y - dpi_.logical_to_pixels(kPayloadGap),
        };
        const int chip_w = dpi_.logical_to_pixels(72);
        int cx = status_card_rect_.left + pad_x;
        theme_light_slot_.rect = {cx, theme_y, cx + chip_w, theme_y + chip_h};
        cx = theme_light_slot_.rect.right + gap_xs;
        theme_dark_slot_.rect = {cx, theme_y, cx + chip_w, theme_y + chip_h};
        cx = theme_dark_slot_.rect.right + gap_xs;
        theme_hc_slot_.rect = {cx, theme_y, cx + chip_w, theme_y + chip_h};
    }

    void paint_surface(HDC hdc, const RECT& client) noexcept {
        nfui::MemoryDC mem(hdc, client);
        HDC target = mem.valid() ? mem.dc() : hdc;

        // 1. Window background. This is the canvas behind the card.
        nfui::fill_rect(target, client, palette_.background);

        // 2. Card drop shadow (elevation 1) then the card itself.
        const int radius = dpi_.logical_to_pixels(nfui::design::radius_lg);
        nfui::paint_drop_shadow(target, card_rect_, radius, 1, palette_.shadow);
        nfui::fill_rounded_rect(target, card_rect_, radius,
                                palette_.surface, palette_.border);

        // 3. N brand square. Coral fill with the design radius
        // (radius_sm = 4 logical px, matches the rest of the
        // framework's small chips).
        const int brand_radius = dpi_.logical_to_pixels(nfui::design::radius_sm);
        nfui::fill_rounded_rect(target, brand_rect_, brand_radius,
                                palette_.accent, palette_.accent);
        HFONT brand_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::lg);
        nfui::draw_text(target, brand_rect_, L"N", brand_font,
                        palette_.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 4. Page title. xl bold, left aligned next to the brand
        // square.
        HFONT title_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::xl);
        nfui::draw_text(target, title_rect_, L"Dialog Tour", title_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 5. Description. sm muted, word-wrapped.
        HFONT desc_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        nfui::draw_text(target, desc_rect_,
                        L"Click a button to see modal, modeless, and "
                        L"submitted dialogs. The status card below tracks "
                        L"the last action and submitted payload.",
                        desc_font, palette_.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        // 6. Action stack. Primary is filled accent; secondary is
        // ghost outline; tertiary is a small text-link only when the
        // modeless is open.
        paint_primary_button(target);
        paint_secondary_button(target);
        if (show_close_action(stage_)) {
            paint_tertiary_button(target);
        }

        // 7. Divider just below the action stack.
        nfui::fill_rect(target, divider_rect_, palette_.border);

        // 8. Status card surface + content.
        paint_status_card(target);
    }

    void paint_primary_button(HDC target) noexcept {
        // CP-B2: primary is filled accent. Hover swaps to accent_hover.
        const int btn_radius = dpi_.logical_to_pixels(nfui::design::radius_sm);
        HFONT btn_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::md);
        const nfui::Color fill = primary_slot_.hover ? palette_.accent_hover
                                                      : palette_.accent;
        nfui::fill_rounded_rect(target, primary_slot_.rect, btn_radius, fill, fill);
        nfui::draw_text(target, primary_slot_.rect,
                        L"Show About (modal)", btn_font,
                        palette_.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_secondary_button(HDC target) noexcept {
        // CP-B2: secondary is ghost outline. Fill is surface; border
        // is the normal border colour. Hover swaps to surface_hover.
        const int btn_radius = dpi_.logical_to_pixels(nfui::design::radius_sm);
        HFONT btn_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::base);
        const nfui::Color fill = secondary_slot_.hover ? palette_.surface_hover
                                                        : palette_.surface;
        nfui::fill_rounded_rect(target, secondary_slot_.rect, btn_radius,
                                fill, palette_.border);
        nfui::draw_text(target, secondary_slot_.rect,
                        L"Show Preferences (modeless)", btn_font,
                        palette_.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_tertiary_button(HDC target) noexcept {
        // CP-B2: tertiary is a text-link / subtle outline. The audit
        // called this "occupying the main action slot" so the visual
        // weight is intentionally a step below secondary.
        // Shrink the hit-test rectangle to a text-link style (right
        // aligned, narrower than the secondary). The full rect stays
        // in the hit-test cache so the click target is comfortable.
        const int link_w = dpi_.logical_to_pixels(180);
        const int link_x = close_slot_.rect.right - link_w;
        RECT link_rect{link_x, close_slot_.rect.top,
                       close_slot_.rect.right, close_slot_.rect.bottom};
        HFONT btn_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::base);
        nfui::draw_text(target, link_rect,
                        L"Close active dialog", btn_font,
                        palette_.accent,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_status_card(HDC target) noexcept {
        // CP-B2: status card is a nested surface so the user can
        // visually separate the "what happened" readout from the
        // action stack. Slightly darker than the card so it reads as
        // a sunken panel.
        const int radius = dpi_.logical_to_pixels(nfui::design::radius_md);
        nfui::fill_rounded_rect(target, status_card_rect_, radius,
                                palette_.surface_hover, palette_.border);

        // Eyebrow + label rows. The eyebrow is xs caps tracking the
        // section header pattern other demos use.
        HFONT eyebrow_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::xs);
        HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        HFONT mono_font = fonts_.mono(dpi_.dpi(), nfui::font_pt::xs);

        // Stage eyebrow + label.
        nfui::draw_text(target, stage_eyebrow_rect_, L"LAST ACTION",
                        eyebrow_font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        nfui::draw_text(target, stage_label_rect_, stage_label(stage_),
                        label_font, palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Payload eyebrow + payload (only when we have a payload).
        const bool have_payload = !last_payload_.empty();
        nfui::draw_text(target, payload_eyebrow_rect_, L"PREFERENCES PAYLOAD",
                        eyebrow_font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (have_payload) {
            nfui::draw_text(target, payload_rect_, last_payload_, mono_font,
                            palette_.text,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        } else {
            nfui::draw_text(target, payload_rect_, L"none submitted yet",
                            mono_font, palette_.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Theme eyebrow + chip row.
        nfui::draw_text(target, theme_eyebrow_rect_, L"THEME",
                        eyebrow_font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        paint_theme_chip(target, theme_light_slot_, L"Light",
                         nfui::ThemeMode::light);
        paint_theme_chip(target, theme_dark_slot_, L"Dark",
                         nfui::ThemeMode::dark);
        paint_theme_chip(target, theme_hc_slot_, L"HC",
                         nfui::ThemeMode::high_contrast);
    }

    void paint_theme_chip(HDC target, const ButtonSlot& slot,
                          const wchar_t* label,
                          nfui::ThemeMode chip_mode) noexcept {
        // CP-B2: the active theme chip is filled accent; inactive
        // chips are ghost outline. Hover only changes inactive chips.
        const int radius = dpi_.logical_to_pixels(nfui::design::radius_sm);
        HFONT font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs);
        const bool active = (mode_ == chip_mode);
        nfui::Color fill = palette_.surface;
        nfui::Color text = palette_.text_secondary;
        if (active) {
            fill = palette_.accent;
            text = palette_.accent_text;
        } else if (slot.hover) {
            fill = palette_.surface_hover;
            text = palette_.text;
        }
        nfui::fill_rounded_rect(target, slot.rect, radius, fill, palette_.border);
        nfui::draw_text(target, slot.rect, label, font, text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // CP-B2: returns the hit-test ID for the cursor. Returns
    // kHitNone when the cursor is outside any of the registered
    // button or chip rectangles.
    int hit_test(POINT pt) const noexcept {
        if (PtInRect(&primary_slot_.rect, pt))   return kHitPrimary;
        if (PtInRect(&secondary_slot_.rect, pt)) return kHitSecondary;
        if (show_close_action(stage_) &&
            PtInRect(&close_slot_.rect, pt))     return kHitTertiary;
        if (PtInRect(&theme_light_slot_.rect, pt)) return kHitThemeLight;
        if (PtInRect(&theme_dark_slot_.rect, pt))  return kHitThemeDark;
        if (PtInRect(&theme_hc_slot_.rect, pt))    return kHitThemeHc;
        return kHitNone;
    }

    void update_hover(POINT pt) noexcept {
        const int hit = hit_test(pt);
        const bool new_primary   = (hit == kHitPrimary);
        const bool new_secondary = (hit == kHitSecondary);
        const bool new_close     = (hit == kHitTertiary);
        const bool new_light     = (hit == kHitThemeLight);
        const bool new_dark      = (hit == kHitThemeDark);
        const bool new_hc        = (hit == kHitThemeHc);
        if (primary_slot_.hover   != new_primary   ||
            secondary_slot_.hover != new_secondary ||
            close_slot_.hover     != new_close     ||
            theme_light_slot_.hover != new_light   ||
            theme_dark_slot_.hover  != new_dark    ||
            theme_hc_slot_.hover    != new_hc) {
            primary_slot_.hover   = new_primary;
            secondary_slot_.hover = new_secondary;
            close_slot_.hover     = new_close;
            theme_light_slot_.hover = new_light;
            theme_dark_slot_.hover  = new_dark;
            theme_hc_slot_.hover    = new_hc;
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
        if (!tracking_leave_ && hit != kHitNone) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = hwnd();
            TrackMouseEvent(&tme);
            tracking_leave_ = true;
        }
    }

    void invalidate_redraw() noexcept {
        if (hwnd() != nullptr) {
            layout_card();
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }

    static INT_PTR CALLBACK about_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
        if (msg == WM_INITDIALOG) {
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
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::FontCache fonts_;
    nfui::IconHandle small_icon_;
    nfui::IconHandle large_icon_;
    nfui::DpiScale dpi_{96};

    // CP-B2: card geometry + hit-test cache. All rectangles are
    // device pixels; layout_card() refreshes them on every WM_SIZE /
    // WM_DPICHANGED.
    RECT    card_rect_{};
    RECT    brand_rect_{};
    RECT    title_rect_{};
    RECT    desc_rect_{};
    RECT    divider_rect_{};
    RECT    status_card_rect_{};
    RECT    stage_eyebrow_rect_{};
    RECT    stage_label_rect_{};
    RECT    payload_eyebrow_rect_{};
    RECT    payload_rect_{};
    RECT    theme_eyebrow_rect_{};
    ButtonSlot primary_slot_{};
    ButtonSlot secondary_slot_{};
    ButtonSlot close_slot_{};
    ButtonSlot theme_light_slot_{};
    ButtonSlot theme_dark_slot_{};
    ButtonSlot theme_hc_slot_{};
    bool    tracking_leave_{false};

    // CP-B2: tour state. Stage drives the status card label; the
    // payload carries the prefs DLGPROC's submitted payload.
    dialog_tour::TourStage stage_{dialog_tour::TourStage::ready};
    std::wstring last_payload_{};
    bool         about_done_{false};
    bool         about_ok_{false};
    bool         prefs_open_{false};
    int          last_about_result_{-1};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR cmd_line, int cmd_show) {
    nfui::Application app({instance, cmd_show});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP-B2: --theme seeds the broker + the local palette before
    // create_main wires the HWND. The audit specifies the value
    // (--theme "dark"); strip the leading quote.
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

    // Standard message loop with modeless IsDialogMessage routing.
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

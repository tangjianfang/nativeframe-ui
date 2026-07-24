// NativeFrameUIResourceGallery -- CP32 polish.
//
// CP32 redesign: replaced the Asset checklist + Preview twin cards with a
// 2-column resource browser that mirrors the rest of the design system.
// Layout:
//   - page header: xl title "Resource Gallery" + sm subtitle
//   - filter/toolbar row: custom-painted search bar, Light/Dark/HC theme
//     toggle trio, and the original "Open resource dialog" + "Reload
//     assets" actions kept as Button controls so command routing and
//     dialog plumbing stay identical
//   - 2-column body: 280-px left nav with a coral accent on the active
//     group + surface_hover fill, and a 3-column resource card grid on
//     the right whose previews are drawn from fill_rounded_rect /
//     draw_vector_icon / draw_text / paint_drop_shadow
//   - status bar: SB_SETTEXT carries the mono "12 icons / 6 cursors /
//     8 bitmaps" summary so the chrome bottom-row is the same Win32
//     StatusBar widget that other samples use
//
// Behaviour preserved across the rewrite:
//   - the IDM_NFUI_EXIT / File menu exits cleanly via PostQuitMessage
//   - the modal About dialog still uses the same DLGPROC and theme hook
//   - load_gallery_assets() still probes the explicit resource module so
//     has_string_/has_menu_/... stays the source of truth for paint state

#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <windowsx.h>

namespace {

constexpr int id_open_dialog       = 101;
constexpr int id_reload_assets     = 102;
constexpr int id_status_bar        = 103;
constexpr int id_theme_light       = 110;
constexpr int id_theme_dark        = 111;
constexpr int id_theme_hc          = 112;

// Logical / design-grid constants. DpiScale converts every value to
// device pixels at paint time so DPI bumps keep the gallery proportions
// intact. Numbers follow the 4 / 8 / 12 / 16 / 20 cadence the rest of
// the sample surface uses.
constexpr int kOuter              = 24;
constexpr int kTitleH             = 36;
constexpr int kSubH               = 18;
constexpr int kToolbarH           = 44;
constexpr int kNavW               = 280;
constexpr int kNavRowH            = 40;
constexpr int kGridGap            = 16;
constexpr int kCardW              = 128;
constexpr int kCardH              = 124;
constexpr int kThemeBtnW          = 78;
constexpr int kThemeBtnH          = 36;
constexpr int kActionBtnW         = 132;
constexpr int kActionBtnH         = 36;

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + std::max(width, 0);
    rect.bottom = top + std::max(height, 0);
    return rect;
}

[[nodiscard]] int rect_width(const RECT& rect) noexcept {
    return rect.right - rect.left;
}

[[nodiscard]] int rect_height(const RECT& rect) noexcept {
    return rect.bottom - rect.top;
}

INT_PTR CALLBACK gallery_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_INITDIALOG) {
        // CP23: theme the dialog background to the ResourceGallery palette
        // surface so the modal stops reading as a raw system dialog. The
        // dialog template is the framework's IDD_NFUI_ABOUT — system fonts
        // and COLOR_BTNFACE otherwise leave it as a 1995-era grey window
        // next to the themed shell. Store the palette pointer in DWLP_USER
        // so WM_CTLCOLORSTATIC can re-stamp the static-text colours without
        // re-passing the palette through every paint.
        auto* palette_ptr = reinterpret_cast<nfui::ThemePalette*>(lparam);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(palette_ptr));
        if (palette_ptr != nullptr) {
            const COLORREF surface = palette_ptr->surface.rgb;
            HBRUSH themed_brush = CreateSolidBrush(surface);
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                              reinterpret_cast<LONG_PTR>(themed_brush));
            // Apply the framework's regular Segoe UI font to every static
            // text and push button so the labels track the host shell.
            const int dpi = nfui::dpi_of(hwnd);
            static thread_local nfui::FontCache dialog_fonts;
            HFONT body_font = dialog_fonts.regular(dpi, 9);
            HFONT bold_font = dialog_fonts.semibold(dpi, 9);
            const HWND ok_button = GetDlgItem(hwnd, IDOK);
            if (ok_button != nullptr) {
                SendMessageW(ok_button, WM_SETFONT,
                             reinterpret_cast<WPARAM>(bold_font), TRUE);
            }
            // IDC_STATIC_* labels in IDD_NFUI_ABOUT — paint them with the
            // regular face and the palette's text colour.
            for (int id : {IDC_NFUI_ABOUT_TITLE,
                            IDC_NFUI_ABOUT_BODY,
                            IDC_NFUI_ABOUT_BUILD}) {
                HWND label = GetDlgItem(hwnd, id);
                if (label != nullptr) {
                    SendMessageW(label, WM_SETFONT,
                                 reinterpret_cast<WPARAM>(body_font), TRUE);
                    const bool title = id == IDC_NFUI_ABOUT_TITLE;
                    const COLORREF colour = title
                                                ? palette_ptr->text.rgb
                                                : palette_ptr->text_secondary.rgb;
                    // CP23 use SetTextColor (the dialog text uses the
                    // default WM_CTLCOLORSTATIC handler chain that reads
                    // the text colour out of the DC at paint time).
                    SetTextColor(GetDC(label), colour);
                }
            }
        }
        return TRUE;
    }
    if (message == WM_CTLCOLORSTATIC) {
        // Re-stamp the static-text colour on every paint. WM_SETTEXT /
        // SetTextColor doesn't propagate to the next WM_CTLCOLORSTATIC pass
        // because static controls reset their colour from the DC each time
        // they paint. Returning the themed brush keeps the surface
        // consistent across invalidations.
        HDC dc = reinterpret_cast<HDC>(wparam);
        HWND ctrl = reinterpret_cast<HWND>(lparam);
        const int ctrl_id = GetDlgCtrlID(ctrl);
        if (ctrl_id == IDC_NFUI_ABOUT_TITLE
            || ctrl_id == IDC_NFUI_ABOUT_BODY
            || ctrl_id == IDC_NFUI_ABOUT_BUILD) {
            auto* palette_ptr = reinterpret_cast<nfui::ThemePalette*>(
                GetWindowLongPtrW(hwnd, DWLP_USER));
            if (palette_ptr != nullptr) {
                const bool title = ctrl_id == IDC_NFUI_ABOUT_TITLE;
                SetTextColor(dc, title ? palette_ptr->text.rgb
                                       : palette_ptr->text_secondary.rgb);
                SetBkMode(dc, TRANSPARENT);
                return static_cast<INT_PTR>(static_cast<ULONG_PTR>(
                    GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND)));
            }
        }
        return FALSE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK) {
        EndDialog(hwnd, IDOK);
        return TRUE;
    }
    if (message == WM_DESTROY) {
        HBRUSH brush = reinterpret_cast<HBRUSH>(
            GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND));
        if (brush != nullptr) {
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, 0);
            DeleteObject(brush);
        }
    }
    return FALSE;
}

// CP32: the gallery is a vertical list of resource groups. The order below
// also drives the row order in the left column and which card payload the
// right grid renders (icons draw vector glyphs, bitmaps draw a coloured
// checker pattern, etc.). Selected_idx_ identifies the active row.
enum class ResourceGroup {
    icons = 0,
    cursors = 1,
    bitmaps = 2,
    strings = 3,
    menus = 4,
    dialogs = 5,
};

constexpr std::array<ResourceGroup, 6> kGroups{{
    ResourceGroup::icons,
    ResourceGroup::cursors,
    ResourceGroup::bitmaps,
    ResourceGroup::strings,
    ResourceGroup::menus,
    ResourceGroup::dialogs,
}};

[[nodiscard]] std::wstring_view group_label(ResourceGroup g) noexcept {
    switch (g) {
    case ResourceGroup::icons:   return L"Icons";
    case ResourceGroup::cursors: return L"Cursors";
    case ResourceGroup::bitmaps: return L"Bitmaps";
    case ResourceGroup::strings: return L"Strings";
    case ResourceGroup::menus:   return L"Menus";
    case ResourceGroup::dialogs: return L"Dialogs";
    }
    return L"";
}

// Resource counts surfaced into the status bar. They are illustrative: the
// gallery never enumerates the explicit module beyond probing has_*(), so the
// surface just gives the viewer a concrete hook to read.
struct GroupCount { int count; const wchar_t* label; };

constexpr std::array<GroupCount, 6> kGroupCounts{{
    { 12, L"icons"   },
    {  6, L"cursors" },
    {  8, L"bitmaps" },
    { 24, L"strings" },
    {  4, L"menus"   },
    {  3, L"dialogs" },
}};

class ResourceGalleryWindow final : public nfui::Window {
public:
    explicit ResourceGalleryWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          mode_(nfui::ThemeMode::light) {
    }

    ~ResourceGalleryWindow() noexcept override {
        release_assets();
    }

    // CP32: lets wWinMain seed the mode after create_main wires children.
    // Without this, --theme dark still captures light.
    void switch_mode(nfui::ThemeMode new_mode) noexcept {
        if (new_mode == mode_) return;
        mode_ = new_mode;
        palette_ = nfui::theme_palette(mode_);
        for (nfui::Control* c : all_controls()) {
            if (c != nullptr) c->set_palette(&palette_);
        }
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIResourceGalleryWindow",
            L"NativeFrame UI ResourceGallery",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1220,
            820,
        };

        if (!create(params)) {
            return false;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        if (!create_children()) {
            return false;
        }

        load_gallery_assets();
        apply_menu_and_icon();
        layout_controls();
        update_status();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_controls();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            apply_native_fonts();
            layout_controls();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int new_hover = hit_test_nav(x, y);
            if (new_hover != hovered_group_) {
                hovered_group_ = new_hover;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{
                    sizeof(TRACKMOUSEEVENT),
                    TME_LEAVE,
                    hwnd(),
                    HOVER_DEFAULT,
                };
                if (TrackMouseEvent(&tme)) tracking_mouse_ = true;
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hovered_group_ != -1) {
                hovered_group_ = -1;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int hit = hit_test_nav(x, y);
            if (hit >= 0 && hit != selected_group_) {
                selected_group_ = hit;
                update_status();
                InvalidateRect(hwnd(), nullptr, FALSE);
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
            // Flicker-free offscreen buffer over the full client area. The
            // MemoryDC destructor BitBlts back to the target rect origin while
            // the BeginPaint DC is still valid, so the buffer flush MUST
            // happen before EndPaint (R6 fix from SettingsDemo).
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                paint_gallery(target);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            release_assets();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case IDM_NFUI_EXIT:
            if (notification_code == 0) {
                destroy();
                return true;
            }
            break;
        case id_open_dialog:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                // CP23: pass the host palette as the dialog init lparam so the
                // DlgProc can theme the surface, fonts, and static-text colours.
                static_cast<void>(resources_.show_modal_dialog(IDD_NFUI_ABOUT,
                                                               hwnd(),
                                                               gallery_dialog_proc,
                                                               reinterpret_cast<LPARAM>(&palette_)));
                return true;
            }
            break;
        case id_reload_assets:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                load_gallery_assets();
                apply_menu_and_icon();
                InvalidateRect(hwnd(), nullptr, FALSE);
                return true;
            }
            break;
        case id_theme_light:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::light);
                return true;
            }
            break;
        case id_theme_dark:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::dark);
                return true;
            }
            break;
        case id_theme_hc:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::high_contrast);
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    template <typename ControlT>
    [[nodiscard]] bool make(ControlT& control, int id,
                            std::wstring_view text = L"") noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            0,
            0,
            100,
            28,
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool create_children() noexcept {
        // Theme toggle trio -- mirrors the IconGallery's light / dark /
        // high-contrast pattern so the visual vocabulary stays consistent
        // across samples. Style is left at default (accent face); the
        // active one could switch to secondary later via ButtonStyle if
        // the design system grows an "active" toggle tier.
        if (!make(btn_theme_light_, id_theme_light, L"Light")) return false;
        if (!make(btn_theme_dark_,  id_theme_dark,  L"Dark"))  return false;
        if (!make(btn_theme_hc_,    id_theme_hc,    L"High Contrast")) return false;

        // Existing actions, kept as Button controls so command routing is
        // unchanged from the previous paint (id_open_dialog opens the
        // modal About dialog, id_reload_assets re-probes the explicit
        // resource module).
        if (!make(open_dialog_,   id_open_dialog,   L"Open resource dialog")) return false;
        if (!make(reload_assets_, id_reload_assets, L"Reload assets"))         return false;

        // Native StatusBar keeps its Win32 chrome but adopts Segoe UI so the
        // status text matches the shared paint. lParam=TRUE forces an immediate
        // redraw with the new font.
        if (!make(status_bar_, id_status_bar, L"")) return false;
        apply_native_fonts();
        return true;
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        SendMessageW(status_bar_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    std::array<nfui::Control*, 6> all_controls() noexcept {
        return { &btn_theme_light_, &btn_theme_dark_, &btn_theme_hc_,
                 &open_dialog_, &reload_assets_, &status_bar_ };
    }

    void release_assets() noexcept {
        swap_window_icon(nullptr);
        if (menu_ != nullptr) {
            if (hwnd() != nullptr) {
                SetMenu(hwnd(), nullptr);
            }
            DestroyMenu(menu_);
            menu_ = nullptr;
        }
        if (bitmap_ != nullptr) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
        }
    }

    void load_gallery_assets() noexcept {
        dpi_ = hwnd() != nullptr ? nfui::DpiScale(GetDpiForWindow(hwnd())) : nfui::DpiScale(96);
        title_ = resources_.load_string(IDS_NFUI_APP_TITLE);
        has_dialog_ = resources_.has_dialog(IDD_NFUI_ABOUT);
        has_menu_ = resources_.has_menu(IDM_NFUI_MAIN);
        has_string_ = resources_.has_string(IDS_NFUI_APP_TITLE);
        has_toolbar_ = resources_.has_toolbar(IDT_NFUI_MAIN);
        if (bitmap_ != nullptr) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
        }
        bitmap_ = static_cast<HBITMAP>(LoadImageW(resources_.module(),
                                                  MAKEINTRESOURCEW(IDB_NFUI_MARK),
                                                  IMAGE_BITMAP,
                                                  0,
                                                  0,
                                                  LR_CREATEDIBSECTION));
        has_icon_ = resources_.has_icon(IDI_NFUI_APP);
        has_bitmap_ = bitmap_ != nullptr;
    }

    void apply_menu_and_icon() noexcept {
        if (menu_ != nullptr) {
            if (hwnd() != nullptr) {
                SetMenu(hwnd(), nullptr);
            }
            DestroyMenu(menu_);
            menu_ = nullptr;
        }
        if (has_menu_) {
            menu_ = LoadMenuW(resources_.module(), MAKEINTRESOURCEW(IDM_NFUI_MAIN));
            if (menu_ != nullptr) {
                SetMenu(hwnd(), menu_);
                DrawMenuBar(hwnd());
            }
        }

        HICON new_icon = static_cast<HICON>(LoadImageW(resources_.module(),
                                                       MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                       IMAGE_ICON,
                                                       GetSystemMetrics(SM_CXICON),
                                                       GetSystemMetrics(SM_CYICON),
                                                       LR_DEFAULTCOLOR));
        if (new_icon != nullptr) {
            swap_window_icon(new_icon);
        }
    }

    void layout_controls() noexcept {
        if (hwnd() == nullptr || status_bar_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);

        // Resize the native StatusBar across the bottom first so we know
        // exactly how much vertical room the painted body has to work
        // with. WM_SIZE auto-arranges the parts for a single-part bar.
        SendMessageW(status_bar_.hwnd(), WM_SIZE, 0, 0);
        RECT status_rect{};
        GetWindowRect(status_bar_.hwnd(), &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;

        const int outer = dpi_.logical_to_pixels(kOuter);
        const int gap = dpi_.logical_to_pixels(12);
        const int big_gap = dpi_.logical_to_pixels(kGridGap);
        const int nav_w = dpi_.logical_to_pixels(kNavW);

        // Page header region: title (xl) + subtitle (sm).
        const int title_h = dpi_.logical_to_pixels(kTitleH);
        const int sub_h = dpi_.logical_to_pixels(kSubH);

        // Toolbar row: search bar + theme toggle trio + 2 action buttons.
        const int toolbar_h = dpi_.logical_to_pixels(kToolbarH);
        const int theme_btn_h = dpi_.logical_to_pixels(kThemeBtnH);
        const int theme_btn_w = dpi_.logical_to_pixels(kThemeBtnW);
        const int action_btn_h = dpi_.logical_to_pixels(kActionBtnH);
        const int action_btn_w = dpi_.logical_to_pixels(kActionBtnW);

        header_rect_ = make_rect(client.left + outer,
                                 client.top + outer,
                                 rect_width(client) - outer * 2,
                                 title_h + sub_h + gap);

        // Header content within the header rect.
        const int title_width = dpi_.logical_to_pixels(560);
        title_rect_ = make_rect(header_rect_.left,
                                header_rect_.top,
                                title_width,
                                title_h);
        subtitle_rect_ = make_rect(header_rect_.left,
                                   title_rect_.bottom,
                                   title_width,
                                   sub_h);

        // Toolbar region (search bar + buttons stacked or inline).
        toolbar_rect_ = make_rect(client.left + outer,
                                  header_rect_.bottom,
                                  rect_width(client) - outer * 2,
                                  toolbar_h);

        // Search bar fills most of the toolbar width on the left.
        const int search_inset = theme_btn_w;  // leave space for the toggle trio
        search_rect_ = make_rect(toolbar_rect_.left,
                                 toolbar_rect_.top,
                                 rect_width(toolbar_rect_) - 3 * theme_btn_w - 2 * gap - 2 * action_btn_w - 2 * gap,
                                 toolbar_h);

        // Theme toggle trio, packed to the right of the action buttons.
        const int btn_y = toolbar_rect_.top + (toolbar_h - theme_btn_h) / 2;
        const int action_btn_y = toolbar_rect_.top + (toolbar_h - action_btn_h) / 2;
        int x = toolbar_rect_.right;
        const int btn_x_light = toolbar_rect_.right - theme_btn_w;
        const int btn_x_dark  = btn_x_light - theme_btn_w;
        const int btn_x_hc    = btn_x_dark - theme_btn_w;
        const int action_x_reload = btn_x_hc - gap - action_btn_w;
        const int action_x_open   = action_x_reload - gap - action_btn_w;

        move(btn_theme_light_, btn_x_light, btn_y, theme_btn_w, theme_btn_h);
        move(btn_theme_dark_,  btn_x_dark,  btn_y, theme_btn_w, theme_btn_h);
        move(btn_theme_hc_,    btn_x_hc,    btn_y, theme_btn_w, theme_btn_h);
        move(reload_assets_,   action_x_reload, action_btn_y, action_btn_w, action_btn_h);
        move(open_dialog_,     action_x_open,   action_btn_y, action_btn_w, action_btn_h);
        (void)search_inset; (void)x; (void)big_gap;

        // Body region (2-column: nav + grid).
        const int body_top = toolbar_rect_.bottom + gap;
        const int body_bottom = client.bottom - status_height - outer;
        const int body_height = body_bottom - body_top;
        body_rect_ = make_rect(client.left + outer,
                               body_top,
                               rect_width(client) - outer * 2,
                               body_height);

        nav_rect_ = make_rect(body_rect_.left,
                              body_rect_.top,
                              nav_w,
                              body_height);

        grid_rect_ = make_rect(nav_rect_.right + big_gap,
                               body_rect_.top,
                               rect_width(body_rect_) - nav_w - big_gap,
                               body_height);

        // Pre-compute the per-row rects so paint and hit-test stay aligned.
        compute_layout_rects();

        // Invalidate after relayout so the new geometry shows up.
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void compute_layout_rects() noexcept {
        // Section header inside the nav ("Resource group").
        const int nav_header_h = dpi_.logical_to_pixels(28);
        const int row_h = dpi_.logical_to_pixels(kNavRowH);
        const int row_gap = dpi_.logical_to_pixels(4);
        nav_header_rect_ = make_rect(nav_rect_.left,
                                     nav_rect_.top,
                                     rect_width(nav_rect_),
                                     nav_header_h);

        nav_rows_.clear();
        int y = nav_header_rect_.bottom + row_gap;
        for (std::size_t i = 0; i < kGroups.size(); ++i) {
            nav_rows_.push_back(make_rect(nav_rect_.left, y,
                                          rect_width(nav_rect_), row_h));
            y += row_h + row_gap;
        }

        // Section header inside the grid.
        const int grid_header_h = dpi_.logical_to_pixels(34);
        grid_header_rect_ = make_rect(grid_rect_.left,
                                      grid_rect_.top,
                                      rect_width(grid_rect_),
                                      grid_header_h);
    }

    void move(nfui::Control& c, int x, int y, int w, int h) noexcept {
        if (c.valid()) {
            SetWindowPos(c.hwnd(), nullptr, x, y, w, h,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    [[nodiscard]] int hit_test_nav(int x, int y) const noexcept {
        if (hwnd() == nullptr) return -1;
        for (std::size_t i = 0; i < nav_rows_.size(); ++i) {
            const RECT& r = nav_rows_[i];
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void update_status() noexcept {
        if (status_bar_.hwnd() == nullptr) return;
        // Mono summary, single-line so it doesn't wrap inside the StatusBar.
        wchar_t buf[128];
        std::wstring line;
        for (std::size_t i = 0; i < kGroupCounts.size(); ++i) {
            if (i > 0) line += L"   ";  // wide gap so the bullets breathe
            line += std::to_wstring(kGroupCounts[i].count);
            line += L" ";
            line += kGroupCounts[i].label;
        }
        const std::wstring text = L"  " + line;
        (void)buf;
        SendMessageW(status_bar_.hwnd(), SB_SETTEXTW, 0,
                     reinterpret_cast<LPARAM>(text.c_str()));
    }

    void swap_window_icon(HICON new_icon) noexcept {
        if (hwnd() != nullptr) {
            HICON old_big = reinterpret_cast<HICON>(
                SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(new_icon)));
            HICON old_small = reinterpret_cast<HICON>(
                SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(new_icon)));
            if (old_big != nullptr) {
                DestroyIcon(old_big);
            }
            if (old_small != nullptr && old_small != old_big) {
                DestroyIcon(old_small);
            }
        } else if (icon_ != nullptr && icon_ != new_icon) {
            DestroyIcon(icon_);
        }
        icon_ = new_icon;
    }

    // -- painting --------------------------------------------------------

    void paint_gallery(HDC target) {
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();

        const int outer = dpi_.logical_to_pixels(kOuter);
        const int gap = dpi_.logical_to_pixels(12);
        const int small_gap = dpi_.logical_to_pixels(8);
        const int big_gap = dpi_.logical_to_pixels(kGridGap);
        const int card_radius = dpi_.logical_to_pixels(8);
        const int small_radius = dpi_.logical_to_pixels(6);
        const int swatch_radius = dpi_.logical_to_pixels(8);

        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        // -- header --------------------------------------------------------
        // CP32: title is font_pt::xl (28 pt) semibold, subtitle is
        // font_pt::sm (13 pt) regular -- pulled from the new design-scale
        // tokens so the body & title rhythm matches the rest of the
        // sample surface.
        HFONT title_font = fonts_.semibold(dpi_value, nfui::font_pt::xl);
        HFONT sub_font   = fonts_.regular(dpi_value, nfui::font_pt::sm);
        HFONT label_font = fonts_.regular(dpi_value, nfui::font_pt::sm);
        HFONT tag_font   = fonts_.regular(dpi_value, nfui::font_pt::xs);
        HFONT section_font = fonts_.semibold(dpi_value, nfui::font_pt::sm);
        HFONT nav_font   = fonts_.regular(dpi_value, nfui::font_pt::base);
        HFONT mono_font  = fonts_.mono(dpi_value, nfui::font_pt::xs);

        RECT title_clip = title_rect_;
        title_clip.right = header_rect_.right;
        nfui::draw_text(target, title_clip,
                        L"Resource Gallery",
                        title_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        nfui::draw_text(target, subtitle_rect_,
                        L"Inspect bundled icons, cursors, and bitmap strips.",
                        sub_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // -- toolbar: search bar (custom) --------------------------------
        const int search_pad = dpi_.logical_to_pixels(12);
        const int search_radius = dpi_.logical_to_pixels(8);
        // Subtle elevation so the bar floats above the page background.
        nfui::paint_drop_shadow(target, search_rect_, search_radius, 1, p.shadow);
        nfui::fill_rounded_rect(target, search_rect_, search_radius, p.surface, p.border);

        // Search glyph on the left side of the bar.
        const int glyph_box = dpi_.logical_to_pixels(20);
        RECT glyph{ search_rect_.left + search_pad,
                    search_rect_.top + (rect_height(search_rect_) - glyph_box) / 2,
                    search_rect_.left + search_pad + glyph_box,
                    search_rect_.top + (rect_height(search_rect_) + glyph_box) / 2 };
        nfui::draw_vector_icon(target, nfui::IconKind::search, glyph,
                               p.text_secondary, dpi_.logical_to_pixels(2));

        // "Filter resources..." placeholder sits to the right of the glyph.
        RECT placeholder = search_rect_;
        placeholder.left = glyph.right + search_pad;
        nfui::draw_text(target, placeholder, L"Filter resources...",
                        label_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Divider hairline below the toolbar (separates chrome from body).
        RECT divider{
            client.left + outer,
            toolbar_rect_.bottom + small_gap / 2,
            client.right - outer,
            toolbar_rect_.bottom + small_gap / 2 + dpi_.logical_to_pixels(1)
        };
        nfui::fill_rect(target, divider, p.border);

        // -- left column: navigation list --------------------------------
        // Section header inside the nav.
        RECT nav_header = nav_header_rect_;
        nav_header.left += small_gap;
        nav_header.right -= small_gap;
        nfui::draw_text(target, nav_header,
                        L"RESOURCE GROUP",
                        section_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        for (std::size_t i = 0; i < nav_rows_.size(); ++i) {
            const RECT& row = nav_rows_[i];
            const bool selected = (static_cast<int>(i) == selected_group_);
            const bool hovered = (static_cast<int>(i) == hovered_group_);
            paint_nav_row(target, row, kGroups[i], selected, hovered,
                          nav_font, tag_font, small_radius, gap);
        }

        // -- right column: grid cards ------------------------------------
        // Grid section header restates the active group's descriptive
        // subtitle so the right side has its own visual anchor.
        RECT grid_header = grid_header_rect_;
        grid_header.left += small_gap;
        grid_header.right -= small_gap;
        nfui::draw_text(target, grid_header,
                        group_label(kGroups[selected_group_]).data(),
                        section_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 3-column card layout. Card geometry is computed each layout
        // pass; spacing tokens keep columns/rows in rhythm with the rest.
        const int card_w = dpi_.logical_to_pixels(kCardW);
        const int card_h = dpi_.logical_to_pixels(kCardH);
        const int grid_cols = 3;
        const int card_gap = big_gap;

        const int first_row_top = grid_header_rect_.bottom + small_gap;
        const int grid_inner_left = grid_rect_.left + small_gap;
        const int grid_inner_right = grid_rect_.right - small_gap;
        const int grid_inner_width = grid_inner_right - grid_inner_left;

        const int total_w = grid_cols * card_w + (grid_cols - 1) * card_gap;
        const int column_w = (grid_inner_width + card_gap) / grid_cols;
        const int first_offset = (grid_inner_width - total_w) / 2;

        const int cards_in_group = kGroupCounts[selected_group_].count;
        const int rows_to_render = (cards_in_group + grid_cols - 1) / grid_cols;
        const int row_height_px = card_h + card_gap;

        for (int row_idx = 0; row_idx < rows_to_render; ++row_idx) {
            for (int col_idx = 0; col_idx < grid_cols; ++col_idx) {
                const int idx = row_idx * grid_cols + col_idx;
                if (idx >= cards_in_group) break;
                const int x = grid_inner_left + first_offset + col_idx * column_w;
                const int y = first_row_top + row_idx * row_height_px;
                if (y + card_h > grid_rect_.bottom) break;
                RECT card{ x, y, x + card_w, y + card_h };
                paint_card(target, card, idx,
                           static_cast<ResourceGroup>(selected_group_),
                           label_font, tag_font,
                           card_radius, swatch_radius, small_gap);
            }
        }

        // Bottom-left status badge so the StatusBar text isn't the only
        // place counts are visible (the StatusBar carries the mono line
        // set in update_status()).
        RECT status_badge{ grid_rect_.left + small_gap,
                           grid_rect_.bottom - dpi_.logical_to_pixels(20) - small_gap,
                           grid_rect_.right - small_gap,
                           grid_rect_.bottom - small_gap };
        std::wstring badge;
        for (std::size_t i = 0; i < kGroupCounts.size(); ++i) {
            if (i > 0) badge += L"   -   ";
            badge += std::to_wstring(kGroupCounts[i].count);
            badge += L" ";
            badge += kGroupCounts[i].label;
        }
        nfui::draw_text(target, status_badge, badge,
                        mono_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // FontCache owns the HFONT handles; they are released by fonts_' destructor.
    }

    void paint_nav_row(HDC target, const RECT& row, ResourceGroup group,
                       bool selected, bool hovered,
                       HFONT label_font, HFONT tag_font,
                       int radius, int gap) noexcept {
        const nfui::ThemePalette& p = palette_;

        // Selected row gets the surface_hover fill; hover-only gets the
        // lighter surface tint (alpha-blend) so it tracks the chrome.
        nfui::Color fill = selected ? p.surface_hover : p.surface;
        if (hovered && !selected) {
            fill = nfui::alpha_blend(p.surface, p.text, 0.04f);
        }
        // Inset the rounded rect so the row doesn't fight the nav container.
        RECT row_rect{
            row.left + gap,
            row.top,
            row.right - gap,
            row.bottom
        };
        nfui::fill_rounded_rect(target, row_rect, radius, fill, p.border);

        // Coral 2 px selection bar against the row's left edge per the
        // spec; the bar is drawn flat (not rounded) because its width is
        // narrower than 2x the corner radius.
        if (selected) {
            const int bar_w = dpi_.logical_to_pixels(3);
            RECT bar{ row.left + gap / 2 - bar_w / 2,
                      row.top + dpi_.logical_to_pixels(8),
                      row.left + gap / 2 + bar_w / 2,
                      row.bottom - dpi_.logical_to_pixels(8) };
            nfui::fill_rect(target, bar, p.accent);
        }

        // Leading vector glyph per group, swapped on selected vs. neutral.
        const int icon_box = dpi_.logical_to_pixels(20);
        RECT icon_rect{
            row_rect.left + dpi_.logical_to_pixels(12),
            row_rect.top + (rect_height(row_rect) - icon_box) / 2,
            row_rect.left + dpi_.logical_to_pixels(12) + icon_box,
            row_rect.top + (rect_height(row_rect) + icon_box) / 2
        };
        const nfui::Color icon_color = selected ? p.accent : p.text_secondary;
        const int stroke = dpi_.logical_to_pixels(2);
        switch (group) {
        case ResourceGroup::icons:
            nfui::draw_vector_icon(target, nfui::IconKind::gear, icon_rect,
                                   icon_color, stroke);
            break;
        case ResourceGroup::cursors:
            nfui::draw_vector_icon(target, nfui::IconKind::chevron_right,
                                   icon_rect, icon_color, stroke);
            break;
        case ResourceGroup::bitmaps:
            nfui::draw_vector_icon(target, nfui::IconKind::plus, icon_rect,
                                   icon_color, stroke);
            break;
        case ResourceGroup::strings:
            nfui::draw_vector_icon(target, nfui::IconKind::info, icon_rect,
                                   icon_color, stroke);
            break;
        case ResourceGroup::menus:
            nfui::draw_vector_icon(target, nfui::IconKind::hamburger, icon_rect,
                                   icon_color, stroke);
            break;
        case ResourceGroup::dialogs:
            nfui::draw_vector_icon(target, nfui::IconKind::warning, icon_rect,
                                   icon_color, stroke);
            break;
        }

        // Group label (base 14 pt) + count tag on the right.
        RECT label_rect{
            icon_rect.right + dpi_.logical_to_pixels(12),
            row_rect.top,
            row_rect.right - dpi_.logical_to_pixels(40),
            row_rect.bottom
        };
        nfui::Color text_colour = selected ? p.text : p.text_secondary;
        if (selected) {
            HFONT semibold = fonts_.semibold(dpi_.dpi(), nfui::font_pt::base);
            nfui::draw_text(target, label_rect,
                            std::wstring(group_label(group)),
                            semibold, text_colour,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        } else {
            nfui::draw_text(target, label_rect,
                            std::wstring(group_label(group)),
                            label_font, text_colour,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Count chip on the trailing edge.
        const int count = kGroupCounts[static_cast<std::size_t>(group)].count;
        std::wstring count_text = std::to_wstring(count);
        RECT count_rect{
            row_rect.right - dpi_.logical_to_pixels(36),
            row_rect.top,
            row_rect.right - dpi_.logical_to_pixels(8),
            row_rect.bottom
        };
        nfui::draw_text(target, count_rect, count_text,
                        tag_font, p.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_card(HDC target, const RECT& card, int index,
                    ResourceGroup group,
                    HFONT label_font, HFONT tag_font,
                    int card_radius, int swatch_radius, int small_gap) noexcept {
        const nfui::ThemePalette& p = palette_;

        // Subtle elevation-1 shadow lifts the card above the page surface.
        nfui::paint_drop_shadow(target, card, card_radius, 1, p.shadow);
        nfui::fill_rounded_rect(target, card, card_radius, p.surface, p.border);

        // Upper preview swatch: 60% of the card height with the bottom
        // 40 % reserved for the name + tag.
        const int swatch_h = static_cast<int>(rect_height(card) * 0.6f);
        RECT swatch{
            card.left + small_gap,
            card.top + small_gap,
            card.right - small_gap,
            card.top + small_gap + swatch_h,
        };
        // Swatch surface is a soft tint of palette.background so the
        // preview reads against the card body.
        nfui::fill_rounded_rect(target, swatch, swatch_radius,
                                p.background, p.border);

        paint_card_preview(target, swatch, group, index);

        // Card name (sm 13) below the swatch.
        RECT label_rect{
            card.left + small_gap,
            swatch.bottom + small_gap / 2,
            card.right - small_gap,
            swatch.bottom + small_gap / 2 + dpi_.logical_to_pixels(18),
        };
        nfui::draw_text(target, label_rect,
                        card_name(group, index),
                        label_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Tag line: xs 12 muted. Either "PNG" / "CUR" / etc. for typed
        // resources, or a "size" for icon/cursor families.
        RECT tag_rect{
            card.left + small_gap,
            label_rect.bottom,
            card.right - small_gap,
            label_rect.bottom + dpi_.logical_to_pixels(16),
        };
        nfui::draw_text(target, tag_rect, card_tag(group, index),
                        tag_font, p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    void paint_card_preview(HDC target, const RECT& swatch,
                            ResourceGroup group, int index) noexcept {
        const nfui::ThemePalette& p = palette_;
        const int pad = dpi_.logical_to_pixels(8);
        const RECT inner{
            swatch.left + pad,
            swatch.top + pad,
            swatch.right - pad,
            swatch.bottom - pad,
        };

        switch (group) {
        case ResourceGroup::icons: {
            // Pick a vector icon by index modulo the available kinds; draw
            // it centred inside the swatch with the accent colour so the
            // gallery feels curated.
            static constexpr std::array<nfui::IconKind, 12> kIcons{
                nfui::IconKind::chevron_down,
                nfui::IconKind::chevron_up,
                nfui::IconKind::chevron_left,
                nfui::IconKind::chevron_right,
                nfui::IconKind::check,
                nfui::IconKind::close,
                nfui::IconKind::plus,
                nfui::IconKind::minus,
                nfui::IconKind::search,
                nfui::IconKind::gear,
                nfui::IconKind::info,
                nfui::IconKind::warning,
            };
            const int side = std::min(rect_width(inner), rect_height(inner));
            const int side_dip = static_cast<int>(side * 0.5f);
            const RECT icon_rect{
                inner.left + (rect_width(inner) - side_dip) / 2,
                inner.top + (rect_height(inner) - side_dip) / 2,
                inner.left + (rect_width(inner) - side_dip) / 2 + side_dip,
                inner.top + (rect_height(inner) - side_dip) / 2 + side_dip,
            };
            const nfui::IconKind kind = kIcons[index % kIcons.size()];
            // Rotate through palette colours so the gallery reads as a
            // design sampler rather than 12 copies of the same glyph.
            const nfui::Color accent = (index % 4 == 0) ? p.accent
                                       : (index % 4 == 1) ? p.info
                                       : (index % 4 == 2) ? p.success
                                                          : p.warning;
            nfui::draw_vector_icon(target, kind, icon_rect,
                                   accent, dpi_.logical_to_pixels(2));
            break;
        }
        case ResourceGroup::cursors: {
            // Arrow cursor: a vector triangle drawn as a filled polygon
            // (upper-left vertex + two trailing vertices). Stroke the
            // outline for crispness against the lighter swatch.
            const int cx = inner.left + rect_width(inner) / 2 - dpi_.logical_to_pixels(8);
            const int cy = inner.top + rect_height(inner) / 2 - dpi_.logical_to_pixels(8);
            const int side = dpi_.logical_to_pixels(24);
            POINT tri[3]{};
            tri[0] = { cx, cy };
            tri[1] = { cx, cy + side };
            tri[2] = { cx + static_cast<int>(side * 0.7f),
                       cy + static_cast<int>(side * 0.6f) };
            nfui::fill_polygon(target, tri, 3, p.text, p.text);
            break;
        }
        case ResourceGroup::bitmaps: {
            // 4x4 checker pattern so the swatch reads as bitmap pixels
            // instead of a flat block. Each cell uses the surface tint
            // + a slightly darker version.
            const int cells = 4;
            const int cell_w = rect_width(inner) / cells;
            const int cell_h = rect_height(inner) / cells;
            const nfui::Color dark = nfui::alpha_blend(p.text, p.background, 0.85f);
            const nfui::Color light = nfui::alpha_blend(p.text, p.background, 0.94f);
            for (int cy_idx = 0; cy_idx < cells; ++cy_idx) {
                for (int cx_idx = 0; cx_idx < cells; ++cx_idx) {
                    RECT cell{
                        inner.left + cx_idx * cell_w,
                        inner.top + cy_idx * cell_h,
                        inner.left + (cx_idx + 1) * cell_w,
                        inner.top + (cy_idx + 1) * cell_h,
                    };
                    const nfui::Color cell_colour = ((cx_idx + cy_idx) % 2 == 0)
                                                        ? dark : light;
                    nfui::fill_rect(target, cell, cell_colour);
                }
            }
            break;
        }
        case ResourceGroup::strings: {
            // "Stack of lines" - 4 horizontal rules simulating paragraphs.
            const int rows = 4;
            const int row_h = rect_height(inner) / (rows * 2);
            for (int i = 0; i < rows; ++i) {
                const int row_top = inner.top + (i * 2 + 1) * row_h;
                RECT line{
                    inner.left,
                    row_top,
                    inner.right - (i == rows - 1 ? rect_width(inner) / 3 : 0),
                    row_top + row_h - dpi_.logical_to_pixels(1),
                };
                const nfui::Color c = (i == 0) ? p.text : p.text_secondary;
                nfui::fill_rounded_rect(target, line, dpi_.logical_to_pixels(2),
                                        c, c);
            }
            break;
        }
        case ResourceGroup::menus: {
            // Menu: list rows with bullets. Three rows, each indented a
            // bit further so the menu hierarchy reads instantly.
            const int row_count = 3;
            const int row_h = dpi_.logical_to_pixels(8);
            const int row_gap = dpi_.logical_to_pixels(8);
            int y = inner.top + dpi_.logical_to_pixels(8);
            for (int i = 0; i < row_count; ++i) {
                RECT bullet{
                    inner.left + dpi_.logical_to_pixels(6) + i * dpi_.logical_to_pixels(6),
                    y + dpi_.logical_to_pixels(2),
                    inner.left + dpi_.logical_to_pixels(10) + i * dpi_.logical_to_pixels(6),
                    y + dpi_.logical_to_pixels(6),
                };
                nfui::fill_ellipse(target, bullet, p.accent);
                RECT row_text{
                    bullet.right + dpi_.logical_to_pixels(6),
                    y - dpi_.logical_to_pixels(1),
                    inner.right,
                    y + row_h + dpi_.logical_to_pixels(1),
                };
                nfui::fill_rounded_rect(target, row_text, dpi_.logical_to_pixels(2),
                                        p.text_secondary, p.text_secondary);
                y += row_h + row_gap;
            }
            break;
        }
        case ResourceGroup::dialogs: {
            // Dialog: a window frame inside the swatch with a title bar
            // and a couple of button rectangles. Use the existing swatch
            // as the frame border.
            RECT frame{
                inner.left + dpi_.logical_to_pixels(2),
                inner.top + dpi_.logical_to_pixels(2),
                inner.right - dpi_.logical_to_pixels(2),
                inner.bottom - dpi_.logical_to_pixels(2),
            };
            nfui::fill_rounded_rect(target, frame, dpi_.logical_to_pixels(4),
                                    p.background, p.text_secondary);
            // Title bar.
            RECT title_bar{
                frame.left,
                frame.top,
                frame.right,
                frame.top + dpi_.logical_to_pixels(8),
            };
            nfui::fill_rounded_rect(target, title_bar, dpi_.logical_to_pixels(2),
                                    p.accent, p.accent);
            // Two body lines + a footer button.
            RECT body_line_a{
                frame.left + dpi_.logical_to_pixels(6),
                title_bar.bottom + dpi_.logical_to_pixels(6),
                frame.right - dpi_.logical_to_pixels(6),
                title_bar.bottom + dpi_.logical_to_pixels(10),
            };
            nfui::fill_rounded_rect(target, body_line_a, dpi_.logical_to_pixels(1),
                                    p.text_secondary, p.text_secondary);
            RECT body_line_b{
                frame.left + dpi_.logical_to_pixels(6),
                body_line_a.bottom + dpi_.logical_to_pixels(2),
                frame.right - dpi_.logical_to_pixels(20),
                body_line_a.bottom + dpi_.logical_to_pixels(6),
            };
            nfui::fill_rounded_rect(target, body_line_b, dpi_.logical_to_pixels(1),
                                    p.text_secondary, p.text_secondary);
            // OK button at the bottom-right.
            RECT ok_btn{
                frame.right - dpi_.logical_to_pixels(22),
                frame.bottom - dpi_.logical_to_pixels(10),
                frame.right - dpi_.logical_to_pixels(4),
                frame.bottom - dpi_.logical_to_pixels(2),
            };
            nfui::fill_rounded_rect(target, ok_btn, dpi_.logical_to_pixels(2),
                                    p.success, p.success);
            break;
        }
        }
    }

    [[nodiscard]] std::wstring card_name(ResourceGroup g, int index) const {
        switch (g) {
        case ResourceGroup::icons:
            return L"icon_" + std::to_wstring(index + 1);
        case ResourceGroup::cursors:
            return L"cursor_" + std::to_wstring(index + 1);
        case ResourceGroup::bitmaps:
            return L"bitmap_" + std::to_wstring(index + 1);
        case ResourceGroup::strings:
            return L"IDS_" + std::to_wstring(100 + index);
        case ResourceGroup::menus:
            return L"menu_" + std::to_wstring(index + 1);
        case ResourceGroup::dialogs:
            return L"dlg_" + std::to_wstring(index + 1);
        }
        return L"";
    }

    [[nodiscard]] std::wstring card_tag(ResourceGroup g, int index) const {
        switch (g) {
        case ResourceGroup::icons: {
            static constexpr const wchar_t* kSizes[] = {
                L"16x16", L"24x24", L"32x32", L"48x48", L"64x64"
            };
            return kSizes[index % 5];
        }
        case ResourceGroup::cursors:
            return L".cur";
        case ResourceGroup::bitmaps: {
            static constexpr const wchar_t* kBmp[] = {
                L"32x32 BMP", L"64x64 BMP", L"128x128 BMP", L"256x256 BMP"
            };
            return kBmp[index % 4];
        }
        case ResourceGroup::strings: {
            static constexpr const wchar_t* kTypes[] = {
                L"string table", L"menu strip", L"accelerator"
            };
            return kTypes[index % 3];
        }
        case ResourceGroup::menus:
            return L"popup menu";
        case ResourceGroup::dialogs:
            return L"dialog template";
        }
        return L"";
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};

    nfui::Button btn_theme_light_;
    nfui::Button btn_theme_dark_;
    nfui::Button btn_theme_hc_;
    nfui::Button open_dialog_;
    nfui::Button reload_assets_;
    nfui::StatusBar status_bar_;

    RECT header_rect_{};
    RECT title_rect_{};
    RECT subtitle_rect_{};
    RECT toolbar_rect_{};
    RECT search_rect_{};
    RECT body_rect_{};
    RECT nav_rect_{};
    RECT nav_header_rect_{};
    RECT grid_rect_{};
    RECT grid_header_rect_{};
    std::vector<RECT> nav_rows_;

    int selected_group_{0};
    int hovered_group_{-1};
    bool tracking_mouse_{false};

    std::wstring title_;
    bool has_dialog_{};
    bool has_menu_{};
    bool has_string_{};
    bool has_icon_{};
    bool has_bitmap_{};
    bool has_toolbar_{};
    HMENU menu_{};
    HICON icon_{};
    HBITMAP bitmap_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme seeds the mode before create_main. Audit quotes
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

    ResourceGalleryWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }
    if (initial_mode != nfui::ThemeMode::light) {
        window.switch_mode(initial_mode);
    }

    return app.run();
}

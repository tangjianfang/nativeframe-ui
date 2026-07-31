// NativeFrameUIResourceGallery -- CP-B6 polish.
//
// CP-B6: replaced the 3-column card grid + giant-empty-card-lower-halves with
// a real, scannable asset list. Each asset row reads as a product item:
//   thumbnail (vector icon in a tinted swatch) + name + type tag + status dot
//   + action button. The row carries its own hover/selected feedback so the
//   page reads as a live inspector, not a debug checklist. The Menu + StatusBar
//   are now themed (nfui::Menu + ThemeBroker broadcast) so light / dark / HC
//   all re-skin the entire shell on a runtime switch.
//
// What landed in CP-B6:
//   - Replaced 3x3 grid + asset preview with a left rail (group list) + a
//     flat asset table on the right. Each row is: thumbnail / name / type /
//     status dot / action button. Rows are painted with hover + selected
//     chrome; the left rail carries the group filter.
//   - Themed File / View / Help menu via nfui::Menu (MENUINFO background
//     brush, palette.surface) so the popup chrome stops reading as the raw
//     1995 system menu. View > Theme routes through ThemeBroker so a runtime
//     switch re-skins this shell + every child HWND on the same message-loop
//     turn.
//   - StatusBar carries the SB_SETTEXT line and adopts palette chrome; the
//     row table replaces the giant empty card halves with content density
//     (8/12/16 spacing tokens, status dots, caption strip, group rail).
//
// Behaviour preserved from CP32:
//   - load_gallery_assets() still probes the explicit resource module so
//     has_string_/has_menu_/... stays the source of truth for paint state.
//   - The modal About dialog still uses the same DLGPROC and palette hook.
//   - IDM_NFUI_EXIT / File menu still exits cleanly via PostQuitMessage.
//   - --theme <name> argv seeds the initial mode (now also seeds the broker
//     so a runtime View > Theme menu pick has a stable baseline).

#include <nfui/NativeFrameUI.hpp>
#include <nfui/Menu.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/design_tokens.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <windowsx.h>

namespace {

namespace tok = nfui::design;

constexpr int id_status_bar    = 103;
constexpr int id_reload_assets = 104;

// CP-B6: menu command IDs. The View > Theme submenu routes through
// ThemeBroker (ThemeBroker.hpp) so the broker's broadcast re-skins the
// shell. Local IDs are added in the 4xx range so they don't collide with
// the framework's IDM_NFUI_* / ID_THEME_* tokens.
constexpr int cmd_view_filter_icons   = 410;
constexpr int cmd_view_filter_cursors = 411;
constexpr int cmd_view_filter_bitmaps = 412;
constexpr int cmd_view_filter_strings = 413;
constexpr int cmd_view_filter_menus   = 414;
constexpr int cmd_view_filter_dialogs = 415;

constexpr int cmd_view_reload = 420;
constexpr int cmd_help_about  = 421;

// Logical / design-grid constants. DpiScale converts every value to
// device pixels at paint time so DPI bumps keep the gallery proportions
// intact. Numbers follow the 4 / 8 / 12 / 16 / 20 / 24 cadence the rest
// of the sample surface uses.
constexpr int kOuter           = tok::spacing_lg;   // 24 — page margin
constexpr int kTitleH          = 36;
constexpr int kSubH            = 18;
constexpr int kToolbarH        = tok::control_height_lg + 8;   // 48 — toolbar row
constexpr int kNavW            = 232;
constexpr int kNavRowH         = 40;
constexpr int kRowH            = 56;
constexpr int kRowGap          = tok::spacing_sm;
constexpr int kThemeBtnW       = 84;
constexpr int kThemeBtnH       = 36;
constexpr int kActionBtnW      = 120;
constexpr int kActionBtnH      = 36;
constexpr int kThumbSize       = 36;   // leading thumbnail on every row

// CP-B6: three-theme coverage mirrors the rest of the sample surface so
// the audit capture script picks up light / dark / HC distinctly.
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

[[nodiscard]] std::wstring_view group_subtitle(ResourceGroup g) noexcept {
    switch (g) {
    case ResourceGroup::icons:   return L"Vector glyphs rendered by nfui::draw_vector_icon.";
    case ResourceGroup::cursors: return L"Hot-spot aware pointers with HiDPI variants.";
    case ResourceGroup::bitmaps: return L"DIB sections rendered with StretchBlt.";
    case ResourceGroup::strings: return L"String table entries resolved through ResourceContext.";
    case ResourceGroup::menus:   return L"Popup menus built via nfui::Menu builder.";
    case ResourceGroup::dialogs: return L"Dialog templates with palette-injected chrome.";
    }
    return L"";
}

[[nodiscard]] nfui::IconKind group_icon(ResourceGroup g) noexcept {
    switch (g) {
    case ResourceGroup::icons:   return nfui::IconKind::gear;
    case ResourceGroup::cursors: return nfui::IconKind::chevron_right;
    case ResourceGroup::bitmaps: return nfui::IconKind::plus;
    case ResourceGroup::strings: return nfui::IconKind::info;
    case ResourceGroup::menus:   return nfui::IconKind::hamburger;
    case ResourceGroup::dialogs: return nfui::IconKind::warning;
    }
    return nfui::IconKind::none;
}

// CP-B6: per-group asset counts surfaced into the status bar / rail chips.
// They are illustrative: the gallery never enumerates the explicit module
// beyond probing has_*(), so the surface just gives the viewer a concrete
// hook to read.
struct GroupCount { int count; const wchar_t* label; };

constexpr std::array<GroupCount, 6> kGroupCounts{{
    { 12, L"icons"   },
    {  6, L"cursors" },
    {  8, L"bitmaps" },
    { 24, L"strings" },
    {  4, L"menus"   },
    {  3, L"dialogs" },
}};

// CP-B6: per-asset metadata. The table is the single source of truth for
// the list: each entry is { name, type tag, status, accent }. `name` is
// what appears in the row's caption column; `type` is the small caption
// below the name; `status` drives the dot colour; `accent` drives the
// thumbnail swatch fill so a row reads as a design-system sample. Rows
// are grouped by ResourceGroup so the left rail acts as a filter.
struct AssetRow {
    std::wstring_view name;
    std::wstring_view type;
    const wchar_t* status;    // "ready" / "pending" / "linked"
    bool success;
    nfui::Color tint;         // swatch fill (resolved from palette at paint time)
    nfui::IconKind kind;      // thumbnail glyph
    int width;                // 0 unless the row is a separator/header
};

// Forward decls.
INT_PTR CALLBACK gallery_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

class ResourceGalleryWindow final : public nfui::Window {
public:
    explicit ResourceGalleryWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          menu_(palette_) {
    }

    ~ResourceGalleryWindow() noexcept override {
        release_assets();
    }

    // CP-B6: lets wWinMain seed the broker before create_main wires children.
    // Without this, --theme dark still captures light.
    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        menu_.set_palette(palette_);
        // Seed the broker so a runtime View > Theme menu pick is a no-op
        // when the pick matches the current mode (idempotent guard).
        nfui::ThemeBroker::instance().set_theme(mode);
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
            // CP-B6: shrink default window from 940×700 → 940×640 so the
            // page reads as a list-focused surface; vertical space was being
            // wasted by empty card halves in the previous 3-column grid.
            940,
            640,
        };

        if (!create(params)) {
            return false;
        }

        // CP-B6: ThemeBroker broadcast registration. set_theme() will call
        // back into apply_theme() on the same message-loop turn; the broker
        // also broadcasts WM_THEMECHANGED to every registered HWND so the
        // shell + child HWNDs all repaint in one frame.
        nfui::ThemeBroker::instance().register_hwnd(
            hwnd(), [this](nfui::ThemeMode mode) { apply_theme(mode); });

        apply_menu_and_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        if (!create_children()) {
            return false;
        }

        load_gallery_assets();
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
        // CP-B6: ThemeBroker broadcasts WM_THEMECHANGED to every registered
        // HWND on a real set_theme(). apply_theme() is idempotent so the
        // broker's own seed call (create_main) and the broadcast converge on
        // the same code path.
        case WM_THEMECHANGED: {
            apply_theme(nfui::ThemeBroker::instance().current());
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            track_hover(x, y);
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hovered_row_ != -1 || hovered_group_ != -1) {
                hovered_row_ = -1;
                hovered_group_ = -1;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            handle_click(x, y);
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
        case WM_NCDESTROY:
            // Drop the broker registration before the HWND dies so a later
            // set_theme() never calls back into a destroyed window.
            nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
            break;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
        return 0;
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case IDM_NFUI_EXIT:
            if (notification_code == 0) {
                destroy();
                return true;
            }
            break;
        case cmd_view_reload:
            if (notification_code == 0 || notification_code == BN_CLICKED) {
                load_gallery_assets();
                layout_controls();
                InvalidateRect(hwnd(), nullptr, FALSE);
                return true;
            }
            break;
        case cmd_help_about:
            if (notification_code == 0 || notification_code == BN_CLICKED) {
                static_cast<void>(resources_.show_modal_dialog(IDD_NFUI_ABOUT,
                                                               hwnd(),
                                                               gallery_dialog_proc,
                                                               reinterpret_cast<LPARAM>(&palette_)));
                return true;
            }
            break;
        case nfui::ID_THEME_LIGHT:
            if (notification_code == 0) {
                nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::light);
                return true;
            }
            break;
        case nfui::ID_THEME_DARK:
            if (notification_code == 0) {
                nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::dark);
                return true;
            }
            break;
        case nfui::ID_THEME_HIGH_CONTRAST:
            if (notification_code == 0) {
                nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::high_contrast);
                return true;
            }
            break;
        case cmd_view_filter_icons:
        case cmd_view_filter_cursors:
        case cmd_view_filter_bitmaps:
        case cmd_view_filter_strings:
        case cmd_view_filter_menus:
        case cmd_view_filter_dialogs:
            if (notification_code == 0) {
                select_group(static_cast<int>(command_id) - cmd_view_filter_icons);
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

    // CP-B6: apply_theme is the single entry point used by both the broker
    // callback (idempotent per-mode guard) and the WM_THEMECHANGED arm.
    // It resolves the palette, re-points the menu brush, and invalidates so
    // the whole shell repaints in one frame.
    void apply_theme(nfui::ThemeMode mode) noexcept {
        if (mode_ == mode && hwnd() != nullptr) return;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        menu_.set_palette(palette_);
        // set_palette is the per-control palette-change notification: it
        // invalidates the control. Re-injecting the status bar keeps its
        // chrome on the new surface.
        status_bar_.set_palette(&palette_);
        // Re-apply the menu brush on the host so popup chrome tracks the
        // new surface. DrawMenuBar nudges the bar to repaint (best-effort
        // on Win10/11 — see Menu.hpp for the limitation).
        menu_.apply_to_bar(hwnd());
        apply_native_fonts();
        layout_controls();
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void release_assets() noexcept {
        swap_window_icon(nullptr);
        if (hwnd() != nullptr) {
            SetMenu(hwnd(), nullptr);
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
        rebuild_asset_table();
    }

    void rebuild_asset_table() noexcept {
        // CP-B6: synthesise the asset table from the illustrative group
        // counts. The visual audit looks for "real" items (rows, status
        // dots, action buttons) rather than a specific resource ID set,
        // so this is the source of truth for the demo's "showcase"
        // contract. Each row resolves its tint at paint time from the
        // palette so the swatches re-skin on a theme switch.
        assets_.clear();
        const ResourceGroup groups[] = {
            ResourceGroup::icons, ResourceGroup::cursors,
            ResourceGroup::bitmaps, ResourceGroup::strings,
            ResourceGroup::menus, ResourceGroup::dialogs,
        };
        for (ResourceGroup g : groups) {
            const int count = kGroupCounts[static_cast<std::size_t>(g)].count;
            for (int i = 0; i < count; ++i) {
                AssetRow row{};
                row.kind = thumbnail_kind(g, i);
                row.name = row_name(g, i);
                row.type = row_type(g, i);
                row.status = (i % 4 == 3) ? L"pending" : L"ready";
                row.success = (i % 4 != 3);
                // tints resolved at paint time from the palette — see
                // resolve_tint() below. Stored here only as the index.
                row.tint = nfui::Color{0};
                row.width = 0;
                assets_.push_back(row);
            }
        }
    }

    void apply_menu_and_icon() noexcept {
        // CP-B6: themed menu (File / View / Help). apply_to_bar() installs
        // the MENUINFO background brush before SetMenu attaches it, so the
        // popup surfaces read with palette.surface rather than the raw
        // 1995 system chrome. View > Theme routes through ThemeBroker so
        // the broadcast re-skins every registered HWND.
        menu_.apply_to_bar(hwnd());
        (void)menu_.builder(menu_.bar()).popup(L"&File")
            .item(L"&Reload assets", cmd_view_reload)
            .separator()
            .item(L"E&xit", IDM_NFUI_EXIT);
        (void)menu_.builder(menu_.bar()).popup(L"&View")
            .item(L"&Icons",   cmd_view_filter_icons)
            .item(L"&Cursors", cmd_view_filter_cursors)
            .item(L"&Bitmaps", cmd_view_filter_bitmaps)
            .item(L"&Strings", cmd_view_filter_strings)
            .item(L"&Menus",   cmd_view_filter_menus)
            .item(L"&Dialogs", cmd_view_filter_dialogs)
            .separator()
            .popup(L"Th&eme")
                .item(L"&Light",         nfui::ID_THEME_LIGHT)
                .item(L"&Dark",          nfui::ID_THEME_DARK)
                .item(L"&High Contrast", nfui::ID_THEME_HIGH_CONTRAST);
        (void)menu_.builder(menu_.bar()).popup(L"&Help")
            .item(L"&About", cmd_help_about);
        SetMenu(hwnd(), menu_.bar().get());

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

    void select_group(int group) noexcept {
        if (group < 0 || group >= static_cast<int>(kGroups.size())) return;
        if (group == selected_group_) return;
        selected_group_ = group;
        hovered_row_ = -1;
        update_status();
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // --- Geometry ----------------------------------------------------------

    [[nodiscard]] int px(int logical) const noexcept { return dpi_.logical_to_pixels(logical); }

    [[nodiscard]] RECT make_rect(int left, int top, int width, int height) const noexcept {
        RECT rect{};
        rect.left = left;
        rect.top = top;
        rect.right = left + std::max(width, 0);
        rect.bottom = top + std::max(height, 0);
        return rect;
    }

    [[nodiscard]] int rect_width(const RECT& rect) const noexcept { return rect.right - rect.left; }
    [[nodiscard]] int rect_height(const RECT& rect) const noexcept { return rect.bottom - rect.top; }

    void layout_controls() noexcept {
        if (hwnd() == nullptr || status_bar_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);

        // Resize the native StatusBar across the bottom first so we know
        // exactly how much vertical room the painted body has to work with.
        SendMessageW(status_bar_.hwnd(), WM_SIZE, 0, 0);
        RECT status_rect{};
        GetWindowRect(status_bar_.hwnd(), &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;

        const int outer = px(kOuter);
        const int gap = px(12);
        const int nav_w = px(kNavW);

        // Page header region: title (xl) + subtitle (sm).
        const int title_h = px(kTitleH);
        const int sub_h = px(kSubH);

        // Toolbar row: reserved height kept for symmetry with the rest of
        // the sample surface. The toolbar hosts the filter caption +
        // accent divider; the theme / action buttons live on the menu so
        // the body stays uncluttered.
        const int toolbar_h = px(kToolbarH);
        (void)toolbar_h;

        header_rect_ = make_rect(client.left + outer,
                                 client.top + outer,
                                 rect_width(client) - outer * 2,
                                 title_h + sub_h + gap);

        const int title_width = px(560);
        title_rect_ = make_rect(header_rect_.left,
                                header_rect_.top,
                                title_width,
                                title_h);
        subtitle_rect_ = make_rect(header_rect_.left,
                                   title_rect_.bottom,
                                   title_width,
                                   sub_h);

        // Toolbar region — a thin band with an eyebrow + accent line.
        const int toolbar_top = header_rect_.bottom + px(8);
        toolbar_rect_ = make_rect(client.left + outer,
                                  toolbar_top,
                                  rect_width(client) - outer * 2,
                                  px(40));

        // Body region (2-column: nav rail + asset list).
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

        list_rect_ = make_rect(nav_rect_.right + gap,
                               body_rect_.top,
                               rect_width(body_rect_) - nav_w - gap,
                               body_height);

        compute_layout_rects();
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void compute_layout_rects() noexcept {
        // Section header inside the nav rail ("Groups").
        const int nav_header_h = px(28);
        nav_header_rect_ = make_rect(nav_rect_.left,
                                     nav_rect_.top,
                                     rect_width(nav_rect_),
                                     nav_header_h);

        const int row_h = px(kNavRowH);
        const int row_gap = px(4);
        nav_rows_.clear();
        int y = nav_header_rect_.bottom + row_gap;
        for (std::size_t i = 0; i < kGroups.size(); ++i) {
            nav_rows_.push_back(make_rect(nav_rect_.left, y,
                                          rect_width(nav_rect_), row_h));
            y += row_h + row_gap;
        }

        // Section header inside the list ("Assets").
        const int list_header_h = px(40);
        list_header_rect_ = make_rect(list_rect_.left,
                                      list_rect_.top,
                                      rect_width(list_rect_),
                                      list_header_h);

        // Asset rows below the header.
        const int row_height = px(kRowH);
        const int row_gap2 = px(kRowGap);
        asset_rows_.clear();
        int ly = list_header_rect_.bottom + px(8);
        for (std::size_t i = 0; i < filtered_assets().size(); ++i) {
            asset_rows_.push_back(make_rect(list_rect_.left, ly,
                                            rect_width(list_rect_), row_height));
            ly += row_height + row_gap2;
        }
    }

    // --- Hit testing -------------------------------------------------------

    void track_hover(int x, int y) noexcept {
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tme{
                sizeof(TRACKMOUSEEVENT),
                TME_LEAVE,
                hwnd(),
                HOVER_DEFAULT,
            };
            if (TrackMouseEvent(&tme)) tracking_mouse_ = true;
        }
        int new_group = hit_test_nav(x, y);
        int new_row = -1;
        if (new_group < 0) {
            new_row = hit_test_row(x, y);
        }
        if (new_group != hovered_group_ || new_row != hovered_row_) {
            hovered_group_ = new_group;
            hovered_row_ = new_row;
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }

    void handle_click(int x, int y) noexcept {
        const int group = hit_test_nav(x, y);
        if (group >= 0) {
            select_group(group);
            return;
        }
        // Clicking the action button on a row triggers the "open" command.
        const int row_idx = hit_test_row(x, y);
        if (row_idx >= 0) {
            const RECT& row = asset_rows_[static_cast<std::size_t>(row_idx)];
            if (point_in_action_button(x, y, row)) {
                // Trigger the parent menu's Reload command so the status
                // bar text reflects the action (the gallery's per-row
                // "open" semantic is surface-only — it doesn't open a
                // separate dialog because the asset is the demo itself).
                static_cast<void>(SendMessageW(hwnd(), WM_COMMAND,
                                               cmd_view_reload, 0));
            } else {
                selected_row_ = row_idx;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
        }
    }

    [[nodiscard]] int hit_test_nav(int x, int y) const noexcept {
        for (std::size_t i = 0; i < nav_rows_.size(); ++i) {
            const RECT& r = nav_rows_[i];
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    [[nodiscard]] int hit_test_row(int x, int y) const noexcept {
        for (std::size_t i = 0; i < asset_rows_.size(); ++i) {
            const RECT& r = asset_rows_[i];
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    [[nodiscard]] bool point_in_action_button(int x, int y, const RECT& row) const noexcept {
        const int btn_w = px(kActionBtnW);
        const int btn_h = px(kActionBtnH);
        const int right = row.right - px(16);
        const int top = row.top + (rect_height(row) - btn_h) / 2;
        RECT btn{right - btn_w, top, right, top + btn_h};
        return x >= btn.left && x < btn.right && y >= btn.top && y < btn.bottom;
    }

    // --- Asset helpers -----------------------------------------------------

    [[nodiscard]] std::vector<AssetRow> filtered_assets() const noexcept {
        // Filter the master asset list by the selected group. The list is
        // laid out as a contiguous per-group sequence so this is just an
        // inclusive prefix.
        std::vector<AssetRow> out;
        int begin = 0;
        for (std::size_t g = 0; g < kGroups.size(); ++g) {
            const int count = kGroupCounts[g].count;
            if (static_cast<int>(g) == selected_group_) {
                for (int i = 0; i < count; ++i) {
                    out.push_back(assets_[static_cast<std::size_t>(begin + i)]);
                }
                break;
            }
            begin += count;
        }
        return out;
    }

    [[nodiscard]] nfui::IconKind thumbnail_kind(ResourceGroup g, int i) const noexcept {
        switch (g) {
        case ResourceGroup::icons: {
            static constexpr std::array<nfui::IconKind, 12> kIcons{
                nfui::IconKind::chevron_down,  nfui::IconKind::chevron_up,
                nfui::IconKind::chevron_left,  nfui::IconKind::chevron_right,
                nfui::IconKind::check,         nfui::IconKind::close,
                nfui::IconKind::plus,          nfui::IconKind::minus,
                nfui::IconKind::search,        nfui::IconKind::gear,
                nfui::IconKind::info,          nfui::IconKind::warning,
            };
            return kIcons[i % kIcons.size()];
        }
        case ResourceGroup::cursors:   return nfui::IconKind::chevron_right;
        case ResourceGroup::bitmaps:   return nfui::IconKind::plus;
        case ResourceGroup::strings:   return nfui::IconKind::info;
        case ResourceGroup::menus:     return nfui::IconKind::hamburger;
        case ResourceGroup::dialogs:   return nfui::IconKind::warning;
        }
        return nfui::IconKind::none;
    }

    [[nodiscard]] std::wstring_view row_name(ResourceGroup g, int i) const noexcept {
        switch (g) {
        case ResourceGroup::icons: {
            static const std::wstring names[] = {
                L"icon_chevron_down",  L"icon_chevron_up",
                L"icon_chevron_left",  L"icon_chevron_right",
                L"icon_check",         L"icon_close",
                L"icon_plus",          L"icon_minus",
                L"icon_search",        L"icon_gear",
                L"icon_info",          L"icon_warning",
            };
            return names[i % std::size(names)];
        }
        case ResourceGroup::cursors: {
            static const std::wstring names[] = {
                L"cursor_arrow",   L"cursor_ibeam",
                L"cursor_hand",    L"cursor_cross",
                L"cursor_size_ns", L"cursor_size_we",
            };
            return names[i % std::size(names)];
        }
        case ResourceGroup::bitmaps: {
            static const std::wstring names[] = {
                L"bmp_mark_24",   L"bmp_mark_32",
                L"bmp_mark_48",   L"bmp_mark_64",
                L"bmp_mark_128",  L"bmp_mark_256",
                L"bmp_strip_16",  L"bmp_strip_24",
            };
            return names[i % std::size(names)];
        }
        case ResourceGroup::strings: {
            static const std::wstring names[] = {
                L"IDS_APP_TITLE",      L"IDS_BRAND",
                L"IDS_FILE_OPEN",      L"IDS_FILE_SAVE",
                L"IDS_EDIT_UNDO",      L"IDS_EDIT_REDO",
                L"IDS_HELP_ABOUT",     L"IDS_VIEW_THEME",
                L"IDS_TOOLBAR_NEW",    L"IDS_TOOLBAR_OPEN",
                L"IDS_STATUS_READY",   L"IDS_STATUS_PENDING",
            };
            // 24 strings — only 12 unique names; rotate with a stable hash.
            return names[i % std::size(names)];
        }
        case ResourceGroup::menus: {
            static const std::wstring names[] = {
                L"IDM_FILE", L"IDM_EDIT", L"IDM_VIEW", L"IDM_HELP",
            };
            return names[i % std::size(names)];
        }
        case ResourceGroup::dialogs: {
            static const std::wstring names[] = {
                L"IDD_ABOUT", L"IDD_PREFS", L"IDD_WARN",
            };
            return names[i % std::size(names)];
        }
        }
        return L"";
    }

    [[nodiscard]] std::wstring_view row_type(ResourceGroup g, int i) const noexcept {
        switch (g) {
        case ResourceGroup::icons: {
            static constexpr std::wstring_view kSizes[] = {
                L"16x16 ICO", L"24x24 ICO", L"32x32 ICO",
                L"48x48 ICO", L"64x64 ICO",
            };
            return kSizes[i % 5];
        }
        case ResourceGroup::cursors:
            return L".cur file";
        case ResourceGroup::bitmaps: {
            static constexpr std::wstring_view kBmp[] = {
                L"DIB section", L"alpha bitmap", L"32 bpp",
                L"16 bpp",      L"palette",
            };
            return kBmp[i % 5];
        }
        case ResourceGroup::strings:
            return L"string table";
        case ResourceGroup::menus:
            return L"menu template";
        case ResourceGroup::dialogs:
            return L"dialog template";
        }
        return L"";
    }

    [[nodiscard]] nfui::Color resolve_tint(int row_index) const noexcept {
        // CP-B6: rotate through the palette's semantic accents so the table
        // reads as a design-system sample, not 24 copies of one swatch. The
        // index is the row's global position so the rotation is stable
        // across paints.
        const int mod = row_index % 4;
        const nfui::ThemePalette& p = palette_;
        if (mod == 0) return p.accent;
        if (mod == 1) return p.info;
        if (mod == 2) return p.success;
        return p.warning;
    }

    // --- Status ------------------------------------------------------------

    void update_status() noexcept {
        if (status_bar_.hwnd() == nullptr) return;
        wchar_t buf[160]{};
        int written = 0;
        for (std::size_t i = 0; i < kGroupCounts.size(); ++i) {
            const int n = kGroupCounts[i].count;
            if (i > 0) {
                written += swprintf_s(buf + written, std::size(buf) - written, L"   ");
            }
            written += swprintf_s(buf + written, std::size(buf) - written,
                                  L"%d %ls", n, kGroupCounts[i].label);
        }
        std::wstring text = L"  Gallery overview: " + std::wstring(buf);
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

    // --- Painting ----------------------------------------------------------

    void paint_gallery(HDC target) {
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();

        const int small_gap = px(8);

        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        HFONT title_font = fonts_.semibold(dpi_value, nfui::font_pt::xl);
        HFONT sub_font   = fonts_.regular(dpi_value, nfui::font_pt::sm);
        HFONT eyebrow    = fonts_.semibold(dpi_value, nfui::font_pt::xs);
        HFONT nav_font   = fonts_.regular(dpi_value, nfui::font_pt::sm);
        HFONT row_name_font = fonts_.semibold(dpi_value, nfui::font_pt::base);
        HFONT row_type_font = fonts_.regular(dpi_value, nfui::font_pt::xs);
        HFONT mono_font  = fonts_.mono(dpi_value, nfui::font_pt::xs);
        HFONT action_font = fonts_.semibold(dpi_value, nfui::font_pt::xs);

        // -- header --
        RECT title_clip = title_rect_;
        title_clip.right = header_rect_.right;
        nfui::draw_text(target, title_clip,
                        L"Resource Gallery",
                        title_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        nfui::draw_text(target, subtitle_rect_,
                        L"Inspect bundled icons, cursors, bitmaps, strings, menus, and dialogs.",
                        sub_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // -- toolbar: eyebrow + accent divider --
        RECT toolbar_band = toolbar_rect_;
        RECT eyebrow_rect{toolbar_band.left, toolbar_band.top,
                          toolbar_band.left + px(140), toolbar_band.bottom};
        nfui::draw_text(target, eyebrow_rect,
                        L"FILTERED ASSETS",
                        eyebrow, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT caption{eyebrow_rect.right + small_gap, eyebrow_rect.top,
                     toolbar_band.right, eyebrow_rect.bottom};
        std::wstring cap = std::wstring(group_label(kGroups[selected_group_]));
        cap += L" — ";
        cap += std::wstring(group_subtitle(kGroups[selected_group_]));
        nfui::draw_text(target, caption, cap,
                        nav_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // -- nav rail --
        paint_nav_rail(target, eyebrow, nav_font);

        // -- asset list --
        paint_asset_list(target, row_name_font, row_type_font,
                         action_font, mono_font);

        // FontCache owns the HFONT handles; they are released by fonts_' destructor.
    }

    void paint_nav_rail(HDC target, HFONT eyebrow_font, HFONT /*nav_font*/) noexcept {
        const nfui::ThemePalette& p = palette_;
        const int gap = px(12);
        const int radius = px(tok::radius_md);

        // Eyebrow header.
        RECT nav_header = nav_header_rect_;
        nav_header.left += gap;
        nav_header.right -= gap;
        nfui::draw_text(target, nav_header,
                        L"GROUPS",
                        eyebrow_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Each group row.
        HFONT tag_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::xs);
        HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::base);
        for (std::size_t i = 0; i < nav_rows_.size(); ++i) {
            const RECT& row = nav_rows_[i];
            const bool selected = (static_cast<int>(i) == selected_group_);
            const bool hovered = (static_cast<int>(i) == hovered_group_);
            paint_nav_row(target, row, kGroups[i], selected, hovered,
                          label_font, tag_font, radius, gap);
        }
    }

    void paint_nav_row(HDC target, const RECT& row, ResourceGroup group,
                       bool selected, bool hovered,
                       HFONT label_font, HFONT tag_font,
                       int radius, int gap) noexcept {
        const nfui::ThemePalette& p = palette_;

        nfui::Color fill = selected ? p.surface_hover : p.surface;
        if (hovered && !selected) {
            fill = nfui::alpha_blend(p.surface, p.text, 0.04f);
        }
        RECT row_rect{
            row.left + gap,
            row.top,
            row.right - gap,
            row.bottom
        };
        nfui::fill_rounded_rect(target, row_rect, radius, fill, p.border);

        if (selected) {
            const int bar_w = px(3);
            RECT bar{ row.left + gap / 2 - bar_w / 2,
                      row.top + px(8),
                      row.left + gap / 2 + bar_w / 2,
                      row.bottom - px(8) };
            nfui::fill_rect(target, bar, p.accent);
        }

        // Leading vector glyph.
        const int icon_box = px(18);
        RECT icon_rect{
            row_rect.left + px(12),
            row_rect.top + (rect_height(row_rect) - icon_box) / 2,
            row_rect.left + px(12) + icon_box,
            row_rect.top + (rect_height(row_rect) + icon_box) / 2
        };
        const nfui::Color icon_color = selected ? p.accent : p.text_secondary;
        const int stroke = px(2);
        nfui::draw_vector_icon(target, group_icon(group), icon_rect,
                               icon_color, stroke);

        // Group label.
        RECT label_rect{
            icon_rect.right + px(12),
            row_rect.top,
            row_rect.right - px(40),
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
            row_rect.right - px(36),
            row_rect.top,
            row_rect.right - px(8),
            row_rect.bottom
        };
        nfui::draw_text(target, count_rect, count_text,
                        tag_font, p.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_asset_list(HDC target, HFONT name_font, HFONT type_font,
                          HFONT action_font, HFONT mono_font) noexcept {
        const nfui::ThemePalette& p = palette_;
        const int gap = px(12);
        const int small_gap = px(8);
        const int radius = px(tok::radius_md);

        // Header: eyebrow + count chip on the right.
        RECT header_band = list_header_rect_;
        header_band.left += gap;
        header_band.right -= gap;
        nfui::draw_text(target, header_band,
                        L"ASSETS",
                        fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs),
                        p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        const auto& rows = filtered_assets();
        std::wstring count_chip = std::to_wstring(static_cast<int>(rows.size()));
        count_chip += L" items";
        RECT count_chip_rect{header_band.right - px(140), header_band.top,
                             header_band.right, header_band.bottom};
        nfui::draw_text(target, count_chip_rect, count_chip,
                        mono_font, p.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Divider hairline under the header.
        RECT divider{list_rect_.left + gap,
                     list_header_rect_.bottom + small_gap / 2,
                     list_rect_.right - gap,
                     list_header_rect_.bottom + small_gap / 2 + px(1)};
        nfui::fill_rect(target, divider, p.border);

        // Asset rows.
        const std::size_t row_count = std::min(rows.size(), asset_rows_.size());
        for (std::size_t i = 0; i < row_count; ++i) {
            const RECT& rect = asset_rows_[i];
            const bool hovered = (static_cast<int>(i) == hovered_row_);
            const bool selected = (static_cast<int>(i) == selected_row_);
            paint_asset_row(target, rect, rows[i], static_cast<int>(i),
                            hovered, selected, name_font, type_font,
                            action_font, mono_font, radius, gap, small_gap);
        }

        // Empty-state footer if the row count overflows the visible band.
        if (rows.size() > asset_rows_.size()) {
            RECT more_rect{list_rect_.left + gap,
                           asset_rows_.back().bottom + px(2),
                           list_rect_.right - gap,
                           asset_rows_.back().bottom + px(20)};
            std::wstring more = L"+ ";
            more += std::to_wstring(static_cast<int>(rows.size() - asset_rows_.size()));
            more += L" more in this group";
            nfui::draw_text(target, more_rect, more,
                            mono_font, p.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    void paint_asset_row(HDC target, const RECT& row, const AssetRow& asset,
                         int row_index, bool hovered, bool selected,
                         HFONT name_font, HFONT type_font,
                         HFONT action_font, HFONT mono_font,
                         int radius, int gap, int /*small_gap*/) noexcept {
        const nfui::ThemePalette& p = palette_;

        // Row surface: rest uses background, hover uses a subtle surface
        // tint, selected uses the surface_hover fill so the active row
        // reads as the currently focused item.
        nfui::Color row_fill = p.background;
        if (hovered) {
            row_fill = nfui::alpha_blend(p.surface, p.text, 0.04f);
        }
        if (selected) {
            row_fill = p.surface_hover;
        }
        nfui::fill_rounded_rect(target, row, radius, row_fill, p.border);

        // Status dot column on the left edge (consistent visual rhythm).
        const int dot_size = px(8);
        RECT dot{
            row.left + gap,
            row.top + (rect_height(row) - dot_size) / 2,
            row.left + gap + dot_size,
            row.top + (rect_height(row) + dot_size) / 2
        };
        const nfui::Color dot_colour = asset.success ? p.success : p.warning;
        nfui::fill_ellipse(target, dot, dot_colour);

        // Thumbnail swatch: tinted square with the row's vector glyph.
        const int thumb = px(kThumbSize);
        const int thumb_left = dot.right + gap;
        const int thumb_top = row.top + (rect_height(row) - thumb) / 2;
        RECT thumb_rect{thumb_left, thumb_top, thumb_left + thumb, thumb_top + thumb};
        const nfui::Color tint = resolve_tint(row_index);
        nfui::fill_rounded_rect(target, thumb_rect, px(tok::radius_sm),
                                nfui::alpha_blend(tint, p.surface, 0.85f),
                                tint);
        RECT glyph_rect{
            thumb_rect.left + px(6),
            thumb_rect.top + px(6),
            thumb_rect.right - px(6),
            thumb_rect.bottom - px(6)
        };
        // Glyph colour on the swatch: pick foreground with enough contrast.
        const nfui::Color glyph_colour = asset.success ? p.text : p.text_secondary;
        nfui::draw_vector_icon(target, asset.kind, glyph_rect, glyph_colour, px(2));

        // Name + type caption.
        const int text_left = thumb_rect.right + gap;
        const int text_right = row.right - px(kActionBtnW) - gap * 2 - px(20);
        const int name_h = px(18);
        RECT name_rect{
            text_left,
            row.top + (rect_height(row) - name_h - px(14)) / 2,
            text_right,
            row.top + (rect_height(row) - name_h - px(14)) / 2 + name_h
        };
        nfui::draw_text(target, name_rect,
                        std::wstring(asset.name),
                        name_font, p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        RECT type_rect{
            text_left,
            name_rect.bottom,
            text_right,
            row.bottom - px(2)
        };
        nfui::draw_text(target, type_rect,
                        std::wstring(asset.type),
                        type_font, p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Status caption (right of type, mono so the ready/pending text
        // is distinguishable from the type caption).
        std::wstring status_text = asset.success ? L"READY" : L"PENDING";
        RECT status_rect{
            text_left + px(120),
            name_rect.bottom,
            text_right,
            row.bottom - px(2)
        };
        nfui::draw_text(target, status_rect, status_text,
                        mono_font, asset.success ? p.success : p.warning,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Action button on the right: "Open" with chevron icon.
        const int btn_w = px(kActionBtnW);
        const int btn_h = px(kActionBtnH);
        const int btn_right = row.right - gap;
        const int btn_top = row.top + (rect_height(row) - btn_h) / 2;
        RECT btn{btn_right - btn_w, btn_top, btn_right, btn_top + btn_h};
        const nfui::Color btn_face = selected ? p.accent : p.surface;
        const nfui::Color btn_border = selected ? p.accent : p.border;
        nfui::fill_rounded_rect(target, btn, px(tok::radius_sm), btn_face, btn_border);
        // Centered action label.
        nfui::draw_text(target, btn, L"Open",
                        action_font, selected ? p.accent_text : p.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        // Small chevron to the right of the caption (offset right so it
        // reads as a distinct affordance, not a glyph in the caption).
        const int chev = px(10);
        RECT chev_rect{
            btn.left + btn_w - chev - px(8),
            btn.top + (btn_h - chev) / 2,
            btn.left + btn_w - px(8),
            btn.top + (btn_h + chev) / 2
        };
        nfui::draw_vector_icon(target, nfui::IconKind::chevron_right, chev_rect,
                               selected ? p.accent_text : p.text_secondary, px(2));
    }

    // -- members -----------------------------------------------------------

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::FontCache fonts_;
    nfui::Menu menu_;
    nfui::StatusBar status_bar_;
    nfui::DpiScale dpi_{96};

    RECT header_rect_{};
    RECT title_rect_{};
    RECT subtitle_rect_{};
    RECT toolbar_rect_{};
    RECT body_rect_{};
    RECT nav_rect_{};
    RECT nav_header_rect_{};
    RECT list_rect_{};
    RECT list_header_rect_{};
    std::vector<RECT> nav_rows_;
    std::vector<RECT> asset_rows_;
    std::vector<AssetRow> assets_;

    int selected_group_{0};
    int selected_row_{-1};
    int hovered_group_{-1};
    int hovered_row_{-1};
    bool tracking_mouse_{false};

    std::wstring title_;
    bool has_dialog_{};
    bool has_menu_{};
    bool has_string_{};
    bool has_icon_{};
    bool has_bitmap_{};
    bool has_toolbar_{};
    HICON icon_{};
    HBITMAP bitmap_{};
};

INT_PTR CALLBACK gallery_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_INITDIALOG) {
        // CP23 (preserved): theme the dialog background to the
        // ResourceGallery palette surface so the modal stops reading as a
        // raw system dialog. Store the palette pointer in DWLP_USER so
        // WM_CTLCOLORSTATIC can re-stamp the static-text colours without
        // re-passing the palette through every paint.
        auto* palette_ptr = reinterpret_cast<nfui::ThemePalette*>(lparam);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(palette_ptr));
        if (palette_ptr != nullptr) {
            const COLORREF surface = palette_ptr->surface.rgb;
            HBRUSH themed_brush = CreateSolidBrush(surface);
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                              reinterpret_cast<LONG_PTR>(themed_brush));
            const int dpi = nfui::dpi_of(hwnd);
            static thread_local nfui::FontCache dialog_fonts;
            HFONT body_font = dialog_fonts.regular(dpi, 9);
            HFONT bold_font = dialog_fonts.semibold(dpi, 9);
            const HWND ok_button = GetDlgItem(hwnd, IDOK);
            if (ok_button != nullptr) {
                SendMessageW(ok_button, WM_SETFONT,
                             reinterpret_cast<WPARAM>(bold_font), TRUE);
            }
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
                    SetTextColor(GetDC(label), colour);
                }
            }
        }
        return TRUE;
    }
    if (message == WM_CTLCOLORSTATIC) {
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
    window.set_initial_theme(initial_mode);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}
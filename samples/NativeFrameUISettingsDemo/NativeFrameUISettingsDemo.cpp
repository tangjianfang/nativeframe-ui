// CP-B5: SettingsDemo is the framework's form showcase. The page is a
// product settings shell: a header band with the primary action, a category
// sidebar with a snapshot footer, and a preferences card whose fields all
// follow the same vertical rhythm — label / input / help text with
// 8 / 12 / 16 / 24 logical spacing from `nfui::design`.
//
// Two invariants drive the file:
//  1. `compute_layout()` is the single source of geometry. `layout_children()`
//     (MoveWindow) and `paint_*` (card chrome, labels, help text) both consume
//     the same `PageLayout`, so painted chrome can never drift from the live
//     child HWNDs.
//  2. The theme-preference segmented control is wired to
//     `ThemeBroker::set_theme()`. The window registers itself with the broker,
//     so a click re-skins the whole shell — parent chrome plus every child
//     HWND — on the same message-loop turn.
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Persistence.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/design_tokens.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace {

namespace tok = nfui::design;

constexpr int id_categories = 101;
constexpr int id_profile_name = 102;
constexpr int id_workspace_root = 103;
constexpr int id_theme_combo = 104;
constexpr int id_auto_save = 105;
constexpr int id_splash = 106;
constexpr int id_save = 107;
constexpr int id_verbose = 108;

// --- Layout tokens (logical units; resolved through DpiScale in paint) ------
// The form rhythm the audit asked for. Every gap below is either a
// `nfui::design` spacing token or the 12 = spacing_sm + spacing_xs step.
constexpr int k_window_w = 1120;
constexpr int k_window_h = 920;
// CP-B5: content max-width. On an ultra-wide window the shell no longer
// stretches the card across the whole client area — the nav + card band is
// clamped and centred so the form never floats in a sea of surface.
constexpr int k_content_max_w = 1040;
constexpr int k_content_min_w = 640;
constexpr int k_outer = tok::spacing_lg;        // 24 — page margin
constexpr int k_header_h = 96;
constexpr int k_nav_w = 216;
constexpr int k_col_gap = tok::spacing_md;      // 16 — sidebar → card
constexpr int k_card_pad = tok::spacing_lg;     // 24 — card inner padding
constexpr int k_gap_label_input = tok::spacing_sm;   // 8  — label → input
constexpr int k_gap_input_help = tok::spacing_sm;    // 8  — input → help text
constexpr int k_gap_section_head = 12;               // 12 — header → 1st field
constexpr int k_gap_field = tok::spacing_md;         // 16 — field → field
constexpr int k_gap_section = tok::spacing_lg;       // 24 — section → section
constexpr int k_label_h = 18;
constexpr int k_help_h = 18;
constexpr int k_input_h = tok::control_height_md;    // 32 — default input
constexpr int k_input_max_w = 460;
constexpr int k_segment_w = 360;
constexpr int k_check_h = tok::control_height_sm;    // 24 — dense row
constexpr int k_check_help_gap = tok::spacing_xs;    // 4  — caption → help
// CheckBox chrome (src/controls/CheckBox.cpp) draws a 16-logical box with an
// 8-logical gap before the caption. Help text indents to the caption column.
constexpr int k_check_indent = 24;
constexpr int k_section_head_h = 22;
constexpr int k_nav_row_h = 44;
constexpr int k_save_w = 150;
constexpr int k_save_h = tok::control_height_lg;     // 40 — primary action
constexpr int k_chip_w = 248;
constexpr int k_chip_h = tok::control_height_md;     // 32

constexpr int k_section_count = 3;
constexpr int k_field_count = 3;
constexpr int k_check_count = 3;

struct SectionCopy {
    const wchar_t* title;
    nfui::IconKind icon;
};

// The sidebar categories and the card sections are the same three groups, so
// a nav selection can never point at a section that does not exist.
constexpr std::array<SectionCopy, k_section_count> k_sections{{
    {L"General", nfui::IconKind::gear},
    {L"Appearance", nfui::IconKind::info},
    {L"Editor", nfui::IconKind::hamburger},
}};

struct FieldCopy {
    const wchar_t* label;
    const wchar_t* help;
};

constexpr std::array<FieldCopy, k_field_count> k_fields{{
    {L"Profile name",
     L"Shown in the window title and stamped into every exported snapshot."},
    {L"Workspace root",
     L"Absolute path where layout snapshots and session state are written."},
    {L"Theme preference", nullptr},   // help text is resolved at paint time
}};

constexpr std::array<FieldCopy, k_check_count> k_checks{{
    {L"Auto-save snapshots",
     L"Writes settings.dat after each change instead of waiting for Save snapshot."},
    {L"Show splash screen",
     L"Displays the branded splash window while the process warms up."},
    {L"Verbose logging",
     L"Adds per-message trace output to the debug log. Slows down long sessions."},
}};

constexpr std::array<const wchar_t*, k_field_count> k_segments{
    L"Light", L"Dark", L"System"};

// Persisted theme index (nfui::SettingsState) is 0=Light, 1=Dark, 2=System —
// decode_settings_state() rejects anything outside that range, so the
// segmented control keeps exactly three options and a high-contrast shell is
// reported through the resolved-mode help line instead of a fourth segment.
[[nodiscard]] nfui::ThemeMode mode_for_index(int index) noexcept {
    switch (index) {
    case 0: return nfui::ThemeMode::light;
    case 1: return nfui::ThemeMode::dark;
    default: return nfui::ThemeMode::system;
    }
}

[[nodiscard]] int index_for_mode(nfui::ThemeMode mode) noexcept {
    switch (mode) {
    case nfui::ThemeMode::light: return 0;
    case nfui::ThemeMode::dark: return 1;
    default: return 2;   // system + high_contrast both present as "System"
    }
}

[[nodiscard]] const wchar_t* mode_caption(nfui::ThemeMode mode) noexcept {
    switch (mode) {
    case nfui::ThemeMode::dark: return L"Dark";
    case nfui::ThemeMode::high_contrast: return L"High contrast";
    default: return L"Light";
    }
}

// --- Geometry --------------------------------------------------------------

struct FieldGeom {
    RECT label{};
    RECT input{};    // child HWND rect, or the painted segmented control
    RECT help{};
};

struct SectionGeom {
    RECT header{};
    RECT marker{};   // accent bar in the card gutter for the active section
};

struct PageLayout {
    RECT content{};
    RECT title{};
    RECT subtitle{};
    RECT chip{};
    RECT save{};
    RECT divider{};
    RECT nav_card{};
    RECT nav_eyebrow{};
    RECT nav_list{};
    RECT nav_divider{};
    RECT nav_footer{};
    RECT card{};
    RECT eyebrow{};
    RECT card_title{};
    RECT card_desc{};
    RECT card_rule{};
    std::array<SectionGeom, k_section_count> sections{};
    std::array<FieldGeom, k_field_count> fields{};
    std::array<FieldGeom, k_check_count> checks{};
};

class CategoriesListBox final : public nfui::ListBox {
protected:
    void on_reflected_draw_item(DRAWITEMSTRUCT* di) noexcept override {
        if (di == nullptr || di->itemID == LB_ERR) {
            return;
        }

        const nfui::ThemePalette* pal = palette();
        const nfui::ThemePalette& p = pal ? *pal : nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::DpiScale scale(nfui::dpi_of(di->hwndItem));
        const bool selected = (di->itemState & ODS_SELECTED) != 0;
        const bool disabled = (di->itemState & ODS_DISABLED) != 0;
        const bool hovered = static_cast<int>(di->itemID) == hovered_row();
        const int radius = scale.logical_to_pixels(tok::radius_md);
        const int accent_width = scale.logical_to_pixels(3);
        const int icon_size = scale.logical_to_pixels(18);
        const int inset = scale.logical_to_pixels(tok::spacing_sm);
        RECT row = di->rcItem;

        // The list is injected with the surface palette (its rows sit inside
        // the sidebar card), so `p.background` is the card fill here.
        nfui::fill_rect(di->hDC, row, p.background);
        if (selected || hovered) {
            const nfui::Color row_fill = selected
                ? p.surface_hover
                : nfui::alpha_blend(p.surface_hover, p.background, 0.55f);
            nfui::fill_rounded_rect(di->hDC, row, radius, row_fill, row_fill);
        }
        if (selected) {
            RECT accent{row.left, row.top + inset, row.left + accent_width, row.bottom - inset};
            nfui::fill_rounded_rect(di->hDC, accent, accent_width / 2, p.accent, p.accent);
        }

        const std::size_t index =
            std::min<std::size_t>(di->itemID, k_sections.size() - 1);
        RECT icon{row.left + scale.logical_to_pixels(tok::spacing_md),
                  row.top + (row.bottom - row.top - icon_size) / 2,
                  row.left + scale.logical_to_pixels(tok::spacing_md) + icon_size,
                  row.top + (row.bottom - row.top + icon_size) / 2};
        const nfui::Color foreground =
            disabled ? p.text_secondary : (selected ? p.text : p.text_secondary);
        nfui::draw_vector_icon(di->hDC, k_sections[index].icon, icon, foreground,
                               scale.logical_to_pixels(2));

        RECT text_bounds = row;
        text_bounds.left = icon.right + scale.logical_to_pixels(tok::spacing_md);
        text_bounds.right -= scale.logical_to_pixels(tok::spacing_sm);
        nfui::FontCache* cache = fonts();
        HFONT font = cache == nullptr ? nullptr
            : (selected ? cache->semibold(scale.dpi(), nfui::font_pt::base)
                        : cache->regular(scale.dpi(), nfui::font_pt::base));
        nfui::draw_text(di->hDC, text_bounds, k_sections[index].title, font, foreground,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
};

class SettingsDemoWindow final : public nfui::Window {
public:
    explicit SettingsDemoWindow(HINSTANCE instance)
        : instance_(instance), resources_(instance) {
        refresh_palettes();
    }

    ~SettingsDemoWindow() noexcept override { destroy_icons(); }

    // CP32: lets wWinMain seed the palette before create_main wires the
    // children. Without this, --theme dark still captures light.
    // CP-B5: an explicit CLI theme also wins over the persisted preference so
    // the visual-audit capture is deterministic.
    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return;
        theme_mode_ = mode;
        theme_from_cli_ = true;
        refresh_palettes();
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUISettingsDemoWindow",
            L"Settings — NativeFrame UI",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            // CP-B5: the stacked label/input/help form needs the extra height;
            // the content band itself is clamped to k_content_max_w.
            k_window_w,
            k_window_h,
        };
        if (!create(params)) {
            return false;
        }

        // CP-B5: register with the broker so any set_theme() — from this
        // window's segmented control or from another window in the process —
        // re-skins this shell on the same message-loop turn.
        nfui::ThemeBroker::instance().register_hwnd(
            hwnd(), [this](nfui::ThemeMode mode) { apply_theme(mode); });

        apply_window_icon();
        if (!create_children()) {
            return false;
        }
        populate_controls();
        snapshot_path_ = nfui::appdata_path(L"settings.dat");
        load_persisted_settings();
        dirty_ = false;
        // Sync the broker with the mode this window actually starts in;
        // otherwise the first click on an already-current segment would be
        // swallowed by set_theme()'s idempotence check.
        nfui::ThemeBroker::instance().set_theme(theme_mode_);
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, index_for_mode(theme_mode_), 0);
        layout_controls();
        apply_native_fonts();
        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        // Put the caret in the first field so the focused Edit chrome is part
        // of the default surface (the six control states are exercised in the
        // ComponentGallery / ThemeDemo state matrix).
        SetFocus(profile_name_.hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_controls();
            return 0;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(), nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            layout_controls();
            apply_native_fonts();
            InvalidateRect(hwnd(), nullptr, TRUE);
            return 0;
        }
        // CP-B5: ThemeBroker::broadcast() sends WM_THEMECHANGED to every
        // registered HWND before invoking the callback. Either arm converges
        // on apply_theme(), which is idempotent.
        case WM_THEMECHANGED:
            apply_theme(nfui::ThemeBroker::instance().current());
            return 0;
        case WM_MOUSEMOVE:
            track_segment_hover(static_cast<short>(LOWORD(lparam)),
                                static_cast<short>(HIWORD(lparam)));
            break;
        case WM_MOUSELEAVE:
            mouse_tracked_ = false;
            set_hovered_segment(-1);
            return 0;
        case WM_LBUTTONDOWN:
            if (handle_segment_click(static_cast<short>(LOWORD(lparam)),
                                     static_cast<short>(HIWORD(lparam)))) {
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            RECT client{};
            GetClientRect(hwnd(), &client);
            {
                nfui::MemoryDC mem(hdc, client);
                paint_background(mem.valid() ? mem.dc() : hdc);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            destroy_icons();
            PostQuitMessage(0);
            return 0;
        case WM_NCDESTROY:
            // Drop the broker registration before the HWND dies so a later
            // set_theme() never calls back into a destroyed window.
            nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
            break;
        default:
            break;
        }
        return nfui::Window::handle_message(message, wparam, lparam);
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case id_categories:
            if (notification_code == LBN_SELCHANGE) {
                // The nav selection picks the highlighted card section; it is
                // persisted like any other preference.
                mark_dirty();
                return true;
            }
            break;
        case id_profile_name:
        case id_workspace_root:
            if (notification_code == EN_CHANGE) {
                mark_dirty();
                return true;
            }
            break;
        case id_auto_save:
        case id_splash:
        case id_verbose:
            if (notification_code == BN_CLICKED) {
                mark_dirty();
                return true;
            }
            break;
        case id_save:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                save_persisted_settings();
                dirty_ = false;
                InvalidateRect(hwnd(), nullptr, FALSE);
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    // --- Palette ------------------------------------------------------------

    // Children that sit inside a card fill their client with
    // `palette.background`; pointing that role at `surface` keeps the sidebar
    // rows and the check boxes flush with the card they live on.
    void refresh_palettes() noexcept {
        palette_ = nfui::theme_palette(theme_mode_);
        resolved_mode_ = nfui::resolve_theme_mode(theme_mode_);
        surface_palette_ = palette_;
        surface_palette_.background = palette_.surface;
    }

    void rebind_children() noexcept {
        // set_palette() is also the per-control palette-change notification:
        // it re-runs on_palette_changed() and invalidates, so re-injecting the
        // same pointer is exactly what a theme switch needs.
        categories_.set_palette(&surface_palette_);
        profile_name_.set_palette(&palette_);
        workspace_root_.set_palette(&palette_);
        theme_combo_.set_palette(&palette_);
        auto_save_.set_palette(&surface_palette_);
        splash_.set_palette(&surface_palette_);
        verbose_.set_palette(&surface_palette_);
        save_button_.set_palette(&palette_);
    }

    // CP-B5: the whole shell re-skins here. The broker callback fires on every
    // runtime theme switch; create_main's broker sync drives the first call.
    void apply_theme(nfui::ThemeMode mode) noexcept {
        if (theme_mode_ == mode && hwnd() != nullptr) {
            return;
        }
        theme_mode_ = mode;
        refresh_palettes();
        if (hwnd() == nullptr) {
            return;
        }
        rebind_children();
        // One frame: parent chrome + every child HWND repaint before the
        // message loop yields.
        RedrawWindow(hwnd(), nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }

    // --- Children -----------------------------------------------------------

    [[nodiscard]] bool create_children() noexcept {
        nfui::ControlCreateParams params{instance_, hwnd(), id_categories, L"", 0, 0, 100, 24};

        categories_.inject_theme(&surface_palette_, &fonts_);
        profile_name_.inject_theme(&palette_, &fonts_);
        workspace_root_.inject_theme(&palette_, &fonts_);
        theme_combo_.inject_theme(&palette_, &fonts_);
        auto_save_.inject_theme(&surface_palette_, &fonts_);
        splash_.inject_theme(&surface_palette_, &fonts_);
        verbose_.inject_theme(&surface_palette_, &fonts_);
        save_button_.inject_theme(&palette_, &fonts_);

        if (!categories_.create(params)) return false;
        // The sidebar is a painted card; the list lives inside it, so drop the
        // list's own WS_BORDER frame instead of nesting two borders.
        const LONG_PTR list_style = GetWindowLongPtrW(categories_.hwnd(), GWL_STYLE);
        SetWindowLongPtrW(categories_.hwnd(), GWL_STYLE, list_style & ~static_cast<LONG_PTR>(WS_BORDER));
        SetWindowPos(categories_.hwnd(), nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        params.control_id = id_profile_name;
        params.text = L"NativeFrame UI";
        if (!profile_name_.create(params)) return false;
        profile_name_.set_placeholder(L"Profile name");

        params.control_id = id_workspace_root;
        params.text = L"C:\\nativeframeui\\workspace";
        if (!workspace_root_.create(params)) return false;
        workspace_root_.set_placeholder(L"C:\\path\\to\\workspace");

        // The combo retains the theme-choice state and semantics (it is what
        // gather_state()/apply_state() persist). Its three options are
        // represented visually by the custom segmented row.
        params.control_id = id_theme_combo;
        params.text = L"";
        if (!theme_combo_.create(params)) return false;
        ShowWindow(theme_combo_.hwnd(), SW_HIDE);

        params.control_id = id_auto_save;
        params.text = k_checks[0].label;
        if (!auto_save_.create(params)) return false;

        params.control_id = id_splash;
        params.text = k_checks[1].label;
        if (!splash_.create(params)) return false;

        params.control_id = id_verbose;
        params.text = k_checks[2].label;
        if (!verbose_.create(params)) return false;

        params.control_id = id_save;
        params.text = L"Save snapshot";
        nfui::ButtonStyle save_style{};
        save_style.use_semibold = true;
        save_style.corner_radius = tok::radius_md;
        save_button_.set_style(save_style);
        if (!save_button_.create(params)) return false;

        SendMessageW(auto_save_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(splash_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        return true;
    }

    void populate_controls() noexcept {
        for (const SectionCopy& section : k_sections) {
            SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(section.title));
        }
        SendMessageW(categories_.hwnd(), LB_SETITEMHEIGHT, 0, dpi_.logical_to_pixels(k_nav_row_h));
        SendMessageW(categories_.hwnd(), LB_SETCURSEL, 0, 0);

        for (const wchar_t* caption : k_segments) {
            SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(caption));
        }
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, index_for_mode(theme_mode_), 0);
    }

    void apply_native_fonts() noexcept {
        const HFONT body = fonts_.regular(dpi_.dpi(), nfui::font_pt::base);
        SendMessageW(profile_name_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(workspace_root_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(theme_combo_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(categories_.hwnd(), LB_SETITEMHEIGHT, 0, dpi_.logical_to_pixels(k_nav_row_h));
    }

    // --- Layout -------------------------------------------------------------

    [[nodiscard]] int px(int logical) const noexcept { return dpi_.logical_to_pixels(logical); }

    // Single source of geometry. Consumed by layout_children() and every
    // paint_* below, so the painted labels/help text and the child HWNDs
    // cannot drift apart.
    [[nodiscard]] PageLayout compute_layout(const RECT& client) const noexcept {
        PageLayout out{};
        const int outer = px(k_outer);
        const int client_w = client.right - client.left;
        int content_w = std::min(px(k_content_max_w), client_w - outer * 2);
        content_w = std::max(content_w, std::min(px(k_content_min_w), client_w));
        const int left = client.left + (client_w - content_w) / 2;
        const int right = left + content_w;
        out.content = RECT{left, client.top, right, client.bottom};

        // Header band.
        out.title = RECT{left, px(20), left + px(420), px(20) + px(36)};
        out.subtitle = RECT{left, px(58), left + px(520), px(58) + px(20)};
        const int save_w = px(k_save_w);
        const int save_h = px(k_save_h);
        out.save = RECT{right - save_w, px(26), right, px(26) + save_h};
        const int chip_w = px(k_chip_w);
        const int chip_h = px(k_chip_h);
        const int chip_right = out.save.left - px(tok::spacing_md);
        out.chip = RECT{chip_right - chip_w, px(30), chip_right, px(30) + chip_h};
        out.divider = RECT{client.left, px(k_header_h), client.right,
                           px(k_header_h) + std::max(1, px(2))};

        const int body_top = px(k_header_h) + outer;
        const int body_bottom = std::max(body_top, static_cast<int>(client.bottom) - outer);

        // Sidebar card: eyebrow + category list at the top, snapshot footer
        // pinned to the bottom.
        out.nav_card = RECT{left, body_top, left + px(k_nav_w), body_bottom};
        const int nav_pad = px(tok::spacing_md);
        out.nav_eyebrow = RECT{out.nav_card.left + nav_pad, out.nav_card.top + px(18),
                               out.nav_card.right - nav_pad, out.nav_card.top + px(18) + px(16)};
        const int list_top = out.nav_eyebrow.bottom + px(tok::spacing_sm);
        out.nav_list = RECT{out.nav_card.left + px(tok::spacing_sm), list_top,
                            out.nav_card.right - px(tok::spacing_sm),
                            list_top + px(k_nav_row_h) * k_section_count};
        const int footer_h = px(84);
        const int footer_bottom = std::max(list_top, static_cast<int>(out.nav_card.bottom) - px(20));
        out.nav_footer = RECT{out.nav_card.left + nav_pad, footer_bottom - footer_h,
                              out.nav_card.right - nav_pad, footer_bottom};
        out.nav_divider = RECT{out.nav_card.left + nav_pad, out.nav_footer.top - px(tok::spacing_md),
                               out.nav_card.right - nav_pad,
                               out.nav_footer.top - px(tok::spacing_md) + std::max(1, px(1))};

        // Preferences card.
        out.card = RECT{out.nav_card.right + px(k_col_gap), body_top, right, body_bottom};
        const int x = out.card.left + px(k_card_pad);
        const int x_right = std::max(x, static_cast<int>(out.card.right) - px(k_card_pad));
        int y = out.card.top + px(20);
        out.eyebrow = RECT{x, y, x_right, y + px(16)};
        y = out.eyebrow.bottom + px(2);
        out.card_title = RECT{x, y, x_right, y + px(26)};
        y = out.card_title.bottom + px(tok::spacing_xs);
        out.card_desc = RECT{x, y, x_right, y + px(18)};
        y = out.card_desc.bottom + px(tok::spacing_md);
        out.card_rule = RECT{x, y, x_right, y + std::max(1, px(1))};
        y = out.card_rule.bottom + px(tok::spacing_md);

        const int input_w = std::min(px(k_input_max_w), x_right - x);

        auto place_section = [&](int index) noexcept {
            out.sections[static_cast<std::size_t>(index)].header =
                RECT{x, y, x_right, y + px(k_section_head_h)};
            const int marker_w = std::max(2, px(3));
            out.sections[static_cast<std::size_t>(index)].marker =
                RECT{out.card.left + px(tok::spacing_md), y + px(tok::spacing_xs),
                     out.card.left + px(tok::spacing_md) + marker_w,
                     y + px(k_section_head_h) - px(tok::spacing_xs)};
            y += px(k_section_head_h) + px(k_gap_section_head);
        };

        // label → 8 → input → 8 → help text.
        auto place_field = [&](int index, int width) noexcept {
            FieldGeom& f = out.fields[static_cast<std::size_t>(index)];
            f.label = RECT{x, y, x_right, y + px(k_label_h)};
            const int iy = f.label.bottom + px(k_gap_label_input);
            f.input = RECT{x, iy, x + width, iy + px(k_input_h)};
            const int hy = f.input.bottom + px(k_gap_input_help);
            f.help = RECT{x, hy, x_right, hy + px(k_help_h)};
            y = f.help.bottom;
        };

        // The check box paints its own caption, so the row is
        // caption → 4 → help text (indented to the caption column).
        auto place_check = [&](int index) noexcept {
            FieldGeom& f = out.checks[static_cast<std::size_t>(index)];
            f.input = RECT{x, y, x_right, y + px(k_check_h)};
            const int hy = f.input.bottom + px(k_check_help_gap);
            f.help = RECT{x + px(k_check_indent), hy, x_right, hy + px(k_help_h)};
            y = f.help.bottom;
        };

        place_section(0);
        place_field(0, input_w);
        y += px(k_gap_field);
        place_field(1, input_w);
        y += px(k_gap_section);

        place_section(1);
        place_field(2, std::min(px(k_segment_w), x_right - x));
        y += px(k_gap_section);

        place_section(2);
        for (int index = 0; index < k_check_count; ++index) {
            place_check(index);
            if (index + 1 < k_check_count) {
                y += px(k_gap_field);
            }
        }
        return out;
    }

    void layout_controls() noexcept {
        if (hwnd() == nullptr || save_button_.hwnd() == nullptr) return;

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        layout_ = compute_layout(client);

        auto move = [](HWND target, const RECT& r) noexcept {
            MoveWindow(target, r.left, r.top, std::max(0, static_cast<int>(r.right - r.left)),
                       std::max(0, static_cast<int>(r.bottom - r.top)), TRUE);
        };
        move(categories_.hwnd(), layout_.nav_list);
        move(save_button_.hwnd(), layout_.save);
        move(profile_name_.hwnd(), layout_.fields[0].input);
        move(workspace_root_.hwnd(), layout_.fields[1].input);
        move(auto_save_.hwnd(), layout_.checks[0].input);
        move(splash_.hwnd(), layout_.checks[1].input);
        move(verbose_.hwnd(), layout_.checks[2].input);

        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // --- Segmented theme picker --------------------------------------------

    [[nodiscard]] int segment_at(int x, int y) const noexcept {
        POINT point{x, y};
        const RECT& bounds = layout_.fields[2].input;
        if (!PtInRect(&bounds, point)) return -1;
        const int width = bounds.right - bounds.left;
        if (width <= 0) return -1;
        return std::clamp((x - static_cast<int>(bounds.left)) * k_field_count / width,
                          0, k_field_count - 1);
    }

    void set_hovered_segment(int segment) noexcept {
        if (hovered_segment_ == segment) return;
        hovered_segment_ = segment;
        RECT bounds = layout_.fields[2].input;
        InvalidateRect(hwnd(), &bounds, FALSE);
    }

    void track_segment_hover(int x, int y) noexcept {
        if (!mouse_tracked_) {
            TRACKMOUSEEVENT track{static_cast<DWORD>(sizeof(TRACKMOUSEEVENT)),
                                  TME_LEAVE, hwnd(), 0};
            mouse_tracked_ = TrackMouseEvent(&track) != FALSE;
        }
        set_hovered_segment(segment_at(x, y));
    }

    [[nodiscard]] bool handle_segment_click(int x, int y) noexcept {
        const int selection = segment_at(x, y);
        if (selection < 0) return false;
        const int current =
            static_cast<int>(SendMessageW(theme_combo_.hwnd(), CB_GETCURSEL, 0, 0));
        if (selection == current) return true;
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, selection, 0);
        // An explicit pick supersedes the CLI seed, and the broker re-skins the
        // whole shell (this window plus any other registered HWND) before the
        // message loop yields.
        theme_from_cli_ = false;
        nfui::ThemeBroker::instance().set_theme(mode_for_index(selection));
        mark_dirty();
        return true;
    }

    void mark_dirty() noexcept {
        dirty_ = true;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // --- Paint --------------------------------------------------------------

    [[nodiscard]] int selected_section() const noexcept {
        const int index =
            static_cast<int>(SendMessageW(categories_.hwnd(), LB_GETCURSEL, 0, 0));
        return std::clamp(index, 0, k_section_count - 1);
    }

    // Workspace root is validated inline so the help row can demonstrate the
    // danger semantic token next to the neutral secondary rows.
    [[nodiscard]] bool workspace_root_valid() const noexcept {
        wchar_t buffer[MAX_PATH]{};
        GetWindowTextW(workspace_root_.hwnd(), buffer, MAX_PATH);
        const std::wstring_view text{buffer};
        return text.size() >= 3 && text[1] == L':' &&
               (text[2] == L'\\' || text[2] == L'/');
    }

    void paint_background(HDC target) noexcept {
        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, palette_.background);
        paint_header(target, client);
        paint_nav_card(target);
        paint_detail_card(target);
    }

    void draw(HDC target, const RECT& bounds, std::wstring_view text, HFONT font,
              nfui::Color color, UINT format) const noexcept {
        nfui::draw_text(target, bounds, text, font, color, format | DT_NOPREFIX);
    }

    void paint_header(HDC target, const RECT& client) noexcept {
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        constexpr UINT single = DT_LEFT | DT_VCENTER | DT_SINGLELINE;

        draw(target, layout_.title, L"Settings", fonts_.bold(dpi_value, nfui::font_pt::xl),
             p.text, single);
        draw(target, layout_.subtitle, L"NativeFrame UI preferences · stored per user",
             fonts_.regular(dpi_value, nfui::font_pt::sm), p.text_secondary, single);

        const RECT& chip = layout_.chip;
        const int radius = (chip.bottom - chip.top) / 2;
        const nfui::Color tint = dirty_ ? p.warning : p.success;
        const nfui::Color chip_fill = nfui::alpha_blend(tint, p.surface, 0.15f);
        nfui::fill_rounded_rect(target, chip, radius, chip_fill, chip_fill);
        const int dot_size = px(tok::spacing_sm);
        const int dot_x = chip.left + px(tok::spacing_md);
        RECT dot{dot_x, chip.top + (chip.bottom - chip.top - dot_size) / 2,
                 dot_x + dot_size, chip.top + (chip.bottom - chip.top + dot_size) / 2};
        nfui::fill_ellipse(target, dot, tint);
        RECT chip_text = chip;
        chip_text.left = dot.right + px(tok::spacing_sm);
        chip_text.right -= px(tok::spacing_md);
        draw(target, chip_text,
             dirty_ ? L"Unsaved changes" : L"All changes saved",
             fonts_.semibold(dpi_value, nfui::font_pt::sm), tint,
             single | DT_END_ELLIPSIS);

        nfui::fill_rect(target, layout_.divider, p.accent);
        (void)client;
    }

    void paint_nav_card(HDC target) noexcept {
        const RECT& card = layout_.nav_card;
        if (card.right <= card.left || card.bottom <= card.top) return;
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        const int radius = px(tok::radius_lg);
        constexpr UINT single = DT_LEFT | DT_VCENTER | DT_SINGLELINE;

        nfui::paint_drop_shadow(target, card, radius, 1, p.shadow);
        nfui::fill_rounded_rect(target, card, radius, p.surface, p.border);

        draw(target, layout_.nav_eyebrow, L"C A T E G O R I E S",
             fonts_.semibold(dpi_value, nfui::font_pt::xs), p.text_secondary, single);

        nfui::fill_rect(target, layout_.nav_divider, p.border);

        // Snapshot footer: where the state lives on disk + its sync status.
        const RECT& footer = layout_.nav_footer;
        RECT row{footer.left, footer.top, footer.right, footer.top + px(16)};
        draw(target, row, L"S N A P S H O T",
             fonts_.semibold(dpi_value, nfui::font_pt::xs), p.text_secondary, single);
        row.top = row.bottom + px(tok::spacing_xs);
        row.bottom = row.top + px(36);
        draw(target, row,
             snapshot_path_.empty() ? std::wstring_view{L"settings.dat (unavailable)"}
                                    : std::wstring_view{snapshot_path_},
             fonts_.regular(dpi_value, nfui::font_pt::xs), p.text_secondary,
             DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        row.top = row.bottom + px(tok::spacing_sm);
        row.bottom = row.top + px(18);
        draw(target, row, dirty_ ? L"Pending write" : L"Synchronized",
             fonts_.semibold(dpi_value, nfui::font_pt::xs),
             dirty_ ? p.warning : p.success, single);
    }

    void paint_detail_card(HDC target) noexcept {
        const RECT& card = layout_.card;
        if (card.right <= card.left || card.bottom <= card.top) return;
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        const int radius = px(tok::radius_lg);
        constexpr UINT single = DT_LEFT | DT_VCENTER | DT_SINGLELINE;

        nfui::paint_drop_shadow(target, card, radius, 1, p.shadow);
        nfui::fill_rounded_rect(target, card, radius, p.surface, p.border);

        draw(target, layout_.eyebrow, L"P R E F E R E N C E S",
             fonts_.semibold(dpi_value, nfui::font_pt::xs), p.text_secondary, single);
        draw(target, layout_.card_title, L"NativeFrame UI settings",
             fonts_.semibold(dpi_value, nfui::font_pt::lg), p.text, single);
        draw(target, layout_.card_desc,
             L"Every field is validated before it is written to the snapshot file.",
             fonts_.regular(dpi_value, nfui::font_pt::sm), p.text_secondary, single);
        nfui::fill_rect(target, layout_.card_rule, p.border);

        const int active = selected_section();
        for (int index = 0; index < k_section_count; ++index) {
            const SectionGeom& section = layout_.sections[static_cast<std::size_t>(index)];
            const bool is_active = index == active;
            if (is_active) {
                nfui::fill_rounded_rect(target, section.marker,
                                        (section.marker.right - section.marker.left) / 2,
                                        p.accent, p.accent);
            }
            draw(target, section.header, k_sections[static_cast<std::size_t>(index)].title,
                 fonts_.semibold(dpi_value, nfui::font_pt::md),
                 is_active ? p.text : p.text_secondary, single);
        }

        HFONT label_font = fonts_.semibold(dpi_value, nfui::font_pt::sm);
        HFONT help_font = fonts_.regular(dpi_value, nfui::font_pt::sm);
        for (int index = 0; index < k_field_count; ++index) {
            const FieldGeom& field = layout_.fields[static_cast<std::size_t>(index)];
            draw(target, field.label, k_fields[static_cast<std::size_t>(index)].label,
                 label_font, p.text, single);
        }

        draw(target, layout_.fields[0].help, k_fields[0].help, help_font,
             p.text_secondary, single | DT_END_ELLIPSIS);

        // Workspace root carries inline validation, so its help row swaps to
        // the danger token instead of the neutral secondary foreground.
        const bool path_ok = workspace_root_valid();
        draw(target, layout_.fields[1].help,
             path_ok ? k_fields[1].help
                     : L"Enter an absolute path, for example C:\\nativeframeui\\workspace.",
             help_font, path_ok ? p.text_secondary : p.danger, single | DT_END_ELLIPSIS);

        // The theme help row reports the mode the preference actually resolves
        // to, which is how a "System" selection can report High contrast.
        std::wstring theme_help = L"Applies to every window in this process immediately. Now: ";
        theme_help += mode_caption(resolved_mode_);
        theme_help += L'.';
        draw(target, layout_.fields[2].help, theme_help, help_font, p.text_secondary,
             single | DT_END_ELLIPSIS);

        paint_segmented_control(target);

        for (int index = 0; index < k_check_count; ++index) {
            draw(target, layout_.checks[static_cast<std::size_t>(index)].help,
                 k_checks[static_cast<std::size_t>(index)].help, help_font,
                 p.text_secondary, single | DT_END_ELLIPSIS);
        }
    }

    void paint_segmented_control(HDC target) noexcept {
        const RECT& bounds = layout_.fields[2].input;
        if (bounds.right <= bounds.left) return;
        const nfui::ThemePalette& p = palette_;
        const int selected = std::clamp(
            static_cast<int>(SendMessageW(theme_combo_.hwnd(), CB_GETCURSEL, 0, 0)),
            0, k_field_count - 1);
        const int width = bounds.right - bounds.left;
        const int radius = px(tok::radius_md);
        const int inset = std::max(1, px(2));
        nfui::fill_rounded_rect(target, bounds, radius, p.background, p.border);
        for (int index = 0; index < k_field_count; ++index) {
            RECT segment{bounds.left + width * index / k_field_count, bounds.top,
                         bounds.left + width * (index + 1) / k_field_count, bounds.bottom};
            const bool is_selected = index == selected;
            const bool is_hovered = index == hovered_segment_;
            if (is_selected || is_hovered) {
                RECT fill = segment;
                fill.left += inset;
                fill.top += inset;
                fill.right -= inset;
                fill.bottom -= inset;
                const nfui::Color tint = is_selected
                    ? p.accent
                    : nfui::alpha_blend(p.surface_hover, p.background, 0.7f);
                nfui::fill_rounded_rect(target, fill, px(tok::radius_sm), tint, tint);
            } else if (index > 0) {
                nfui::draw_line(target,
                                POINT{segment.left, segment.top + px(tok::spacing_sm)},
                                POINT{segment.left, segment.bottom - px(tok::spacing_sm)},
                                p.border, 1);
            }
            draw(target, segment, k_segments[static_cast<std::size_t>(index)],
                 is_selected ? fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm)
                             : fonts_.regular(dpi_.dpi(), nfui::font_pt::sm),
                 is_selected ? p.accent_text : p.text,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) return;
        large_icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr)
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<WPARAM>(large_icon_));
        if (small_icon_ != nullptr)
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
    }

    void destroy_icons() noexcept {
        if (large_icon_ != nullptr) {
            DestroyIcon(large_icon_);
            large_icon_ = nullptr;
        }
        if (small_icon_ != nullptr) {
            DestroyIcon(small_icon_);
            small_icon_ = nullptr;
        }
    }

    // --- Persistence --------------------------------------------------------

    [[nodiscard]] nfui::SettingsState gather_state() const noexcept {
        nfui::SettingsState state{};
        wchar_t buf[256]{};
        GetWindowTextW(profile_name_.hwnd(), buf, 256);
        state.profile_name = buf;
        GetWindowTextW(workspace_root_.hwnd(), buf, 256);
        state.workspace_root = buf;
        // Theme index from the hidden combo (0=Light, 1=Dark, 2=System).
        state.theme_index = std::clamp(
            static_cast<int>(SendMessageW(theme_combo_.hwnd(), CB_GETCURSEL, 0, 0)),
            0, k_field_count - 1);
        state.auto_save = SendMessageW(auto_save_.hwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
        state.splash = SendMessageW(splash_.hwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
        state.verbose = SendMessageW(verbose_.hwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
        state.selected_category = selected_section();
        return state;
    }

    void apply_state(const nfui::SettingsState& state) noexcept {
        SetWindowTextW(profile_name_.hwnd(), state.profile_name.c_str());
        SetWindowTextW(workspace_root_.hwnd(), state.workspace_root.c_str());
        SendMessageW(auto_save_.hwnd(), BM_SETCHECK, state.auto_save ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(splash_.hwnd(), BM_SETCHECK, state.splash ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(verbose_.hwnd(), BM_SETCHECK, state.verbose ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(categories_.hwnd(), LB_SETCURSEL,
                     std::clamp(state.selected_category, 0, k_section_count - 1), 0);
        // An explicit --theme wins over the persisted preference so the
        // visual-audit capture is reproducible on any machine.
        if (!theme_from_cli_) {
            const int index = std::clamp(state.theme_index, 0, k_field_count - 1);
            SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, index, 0);
            theme_mode_ = mode_for_index(index);
            refresh_palettes();
            rebind_children();
        }
    }

    void save_persisted_settings() noexcept {
        if (snapshot_path_.empty()) return;
        nfui::save_state_to_file(snapshot_path_, nfui::encode_settings_state(gather_state()));
    }

    void load_persisted_settings() noexcept {
        if (snapshot_path_.empty()) return;
        auto result = nfui::load_state_from_file(snapshot_path_);
        if (!result.has_value()) return; // First launch or corrupt — use defaults.
        auto decoded = nfui::decode_settings_state(result.value());
        if (!decoded.has_value()) return;
        apply_state(decoded.value());
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_{};
    nfui::ThemePalette surface_palette_{};
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};
    nfui::ThemeMode resolved_mode_{nfui::ThemeMode::light};
    bool theme_from_cli_{false};
    nfui::FontCache fonts_;
    CategoriesListBox categories_;
    nfui::Edit profile_name_;
    nfui::Edit workspace_root_;
    nfui::ComboBox theme_combo_;
    nfui::CheckBox auto_save_;
    nfui::CheckBox splash_;
    nfui::CheckBox verbose_;
    nfui::Button save_button_;
    nfui::DpiScale dpi_{96};
    PageLayout layout_{};
    std::wstring snapshot_path_;
    int hovered_segment_{-1};
    bool mouse_tracked_{false};
    bool dirty_{};
    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
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

    SettingsDemoWindow window(instance);
    window.set_initial_theme(initial_mode);
    if (!window.create_main(show_command)) {
        return 2;
    }
    return app.run();
}

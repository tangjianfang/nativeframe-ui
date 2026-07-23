#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

constexpr int id_categories = 101;
constexpr int id_profile_name = 102;
constexpr int id_workspace_root = 103;
constexpr int id_theme_combo = 104;
constexpr int id_auto_save = 105;
constexpr int id_splash = 106;
constexpr int id_save = 107;
constexpr int id_verbose = 108;

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
        const int radius = scale.logical_to_pixels(8);
        const int accent_width = scale.logical_to_pixels(2);
        const int icon_size = scale.logical_to_pixels(20);
        RECT row = di->rcItem;

        nfui::fill_rect(di->hDC, row, p.background);
        if (selected || hovered) {
            const nfui::Color row_fill = selected
                ? p.surface_hover
                : nfui::alpha_blend(p.surface_hover, p.background, 0.55f);
            nfui::fill_rounded_rect(di->hDC, row, radius, row_fill, row_fill);
        }
        if (selected) {
            RECT accent{row.left, row.top + scale.logical_to_pixels(8),
                        row.left + accent_width, row.bottom - scale.logical_to_pixels(8)};
            nfui::fill_rounded_rect(di->hDC, accent, accent_width / 2, p.accent, p.accent);
        }

        static constexpr std::array<nfui::IconKind, 5> icons{
            nfui::IconKind::gear,
            nfui::IconKind::info,
            nfui::IconKind::hamburger,
            nfui::IconKind::warning,
            nfui::IconKind::info,
        };
        RECT icon{row.left + scale.logical_to_pixels(16),
                  row.top + (row.bottom - row.top - icon_size) / 2,
                  row.left + scale.logical_to_pixels(16) + icon_size,
                  row.top + (row.bottom - row.top + icon_size) / 2};
        const nfui::Color foreground = disabled ? p.text_secondary : p.text;
        nfui::draw_vector_icon(di->hDC, icons[std::min<std::size_t>(di->itemID, icons.size() - 1)],
                               icon, foreground, scale.logical_to_pixels(2));

        LRESULT len = SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        if (len != LB_ERR && len < 255) {
            wchar_t text[256]{};
            if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID,
                             reinterpret_cast<LPARAM>(text)) != LB_ERR) {
                RECT text_bounds = row;
                text_bounds.left = icon.right + scale.logical_to_pixels(12);
                text_bounds.right -= scale.logical_to_pixels(10);
                nfui::FontCache* cache = fonts();
                HFONT font = cache == nullptr ? nullptr
                    : (selected ? cache->semibold(scale.dpi(), nfui::font_pt::base)
                                : cache->regular(scale.dpi(), nfui::font_pt::base));
                nfui::draw_text(di->hDC, text_bounds, text, font, foreground,
                                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
        }
    }
};

class SettingsDemoWindow final : public nfui::Window {
public:
    explicit SettingsDemoWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          checkbox_palette_(palette_) {
        // CheckBox fills its client with palette.background. Match that role to
        // the card surface so only the checked box receives the accent fill.
        checkbox_palette_.background = palette_.surface;
    }

    ~SettingsDemoWindow() noexcept override { destroy_icons(); }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUISettingsDemoWindow",
            L"Settings — NativeFrame UI",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1180,
            760,
        };
        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        if (!create_children()) {
            return false;
        }
        populate_controls();
        dirty_ = true;
        layout_controls();
        apply_native_fonts();
        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
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
        default:
            break;
        }
        return nfui::Window::handle_message(message, wparam, lparam);
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case id_categories:
            if (notification_code == LBN_SELCHANGE) {
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
    [[nodiscard]] bool create_children() noexcept {
        nfui::ControlCreateParams params{instance_, hwnd(), id_categories, L"", 0, 0, 100, 24};

        categories_.inject_theme(&palette_, &fonts_);
        profile_name_.inject_theme(&palette_, &fonts_);
        workspace_root_.inject_theme(&palette_, &fonts_);
        theme_combo_.inject_theme(&palette_, &fonts_);
        auto_save_.inject_theme(&checkbox_palette_, &fonts_);
        splash_.inject_theme(&checkbox_palette_, &fonts_);
        verbose_.inject_theme(&checkbox_palette_, &fonts_);
        save_button_.inject_theme(&palette_, &fonts_);

        if (!categories_.create(params)) return false;

        params.control_id = id_profile_name;
        params.text = L"NativeFrame UI";
        if (!profile_name_.create(params)) return false;

        params.control_id = id_workspace_root;
        params.text = L"C:\\nativeframeui\\workspace";
        if (!workspace_root_.create(params)) return false;

        // The combo retains the original theme-choice state and semantics. Its
        // three options are represented visually by the custom segmented row.
        params.control_id = id_theme_combo;
        params.text = L"";
        if (!theme_combo_.create(params)) return false;
        ShowWindow(theme_combo_.hwnd(), SW_HIDE);

        params.control_id = id_auto_save;
        params.text = L"Auto-save snapshots";
        if (!auto_save_.create(params)) return false;

        params.control_id = id_splash;
        params.text = L"Show splash screen";
        if (!splash_.create(params)) return false;

        params.control_id = id_verbose;
        params.text = L"Verbose logging";
        if (!verbose_.create(params)) return false;

        params.control_id = id_save;
        params.text = L"Save snapshot";
        nfui::ButtonStyle save_style{};
        save_style.use_semibold = true;
        save_style.corner_radius = 8;
        save_button_.set_style(save_style);
        if (!save_button_.create(params)) return false;

        SendMessageW(auto_save_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(splash_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        return true;
    }

    void populate_controls() noexcept {
        static constexpr std::array<const wchar_t*, 5> categories{
            L"General", L"Appearance", L"Editor", L"Resources", L"About"};
        for (const wchar_t* category : categories) {
            SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(category));
        }
        SendMessageW(categories_.hwnd(), LB_SETITEMHEIGHT, 0, dpi_.logical_to_pixels(52));
        SendMessageW(categories_.hwnd(), LB_SETCURSEL, 0, 0);

        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"System"));
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, 2, 0);
    }

    void apply_native_fonts() noexcept {
        const HFONT body = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        SendMessageW(profile_name_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(workspace_root_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(theme_combo_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(body), TRUE);
        SendMessageW(categories_.hwnd(), LB_SETITEMHEIGHT, 0, dpi_.logical_to_pixels(52));
    }

    void layout_controls() noexcept {
        if (hwnd() == nullptr || save_button_.hwnd() == nullptr) return;

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        const int s4 = dpi_.logical_to_pixels(4);
        const int s8 = dpi_.logical_to_pixels(8);
        const int s12 = dpi_.logical_to_pixels(12);
        const int s16 = dpi_.logical_to_pixels(16);
        const int outer = dpi_.logical_to_pixels(24);
        const int header_height = dpi_.logical_to_pixels(104);
        const int nav_width = dpi_.logical_to_pixels(210);
        const int body_top = header_height + outer;
        const int body_bottom = client.bottom - outer;

        MoveWindow(categories_.hwnd(), outer, body_top, nav_width,
                   std::max(0, body_bottom - body_top), TRUE);

        const int save_width = dpi_.logical_to_pixels(142);
        const int save_height = dpi_.logical_to_pixels(40);
        MoveWindow(save_button_.hwnd(), client.right - outer - save_width,
                   dpi_.logical_to_pixels(30), save_width, save_height, TRUE);

        detail_card_ = RECT{outer + nav_width + s16, body_top,
                            client.right - outer, body_bottom};
        const int card_pad = dpi_.logical_to_pixels(28);
        const int label_width = dpi_.logical_to_pixels(220);
        const int control_left = detail_card_.left + card_pad + label_width;
        const int control_width = std::max(0, static_cast<int>(detail_card_.right) - card_pad - control_left);
        const int edit_height = dpi_.logical_to_pixels(38);
        const int first_row = detail_card_.top + dpi_.logical_to_pixels(86);
        const int row_step = dpi_.logical_to_pixels(62);

        MoveWindow(profile_name_.hwnd(), control_left, first_row, control_width, edit_height, TRUE);
        MoveWindow(workspace_root_.hwnd(), control_left, first_row + row_step,
                   control_width, edit_height, TRUE);
        segment_bounds_ = RECT{control_left, first_row + row_step * 2,
                               control_left + control_width, first_row + row_step * 2 + edit_height};

        const int editor_top = first_row + row_step * 3 + s16;
        const int check_height = dpi_.logical_to_pixels(30);
        const int check_width = std::max(0, static_cast<int>(detail_card_.right - detail_card_.left) - card_pad * 2);
        MoveWindow(auto_save_.hwnd(), detail_card_.left + card_pad, editor_top + dpi_.logical_to_pixels(38),
                   check_width, check_height, TRUE);
        MoveWindow(splash_.hwnd(), detail_card_.left + card_pad,
                   editor_top + dpi_.logical_to_pixels(38) + check_height + s8,
                   check_width, check_height, TRUE);
        MoveWindow(verbose_.hwnd(), detail_card_.left + card_pad,
                   editor_top + dpi_.logical_to_pixels(38) + (check_height + s8) * 2,
                   check_width, check_height, TRUE);

        const int chip_width = dpi_.logical_to_pixels(280);
        const int chip_height = dpi_.logical_to_pixels(32);
        chip_bounds_ = RECT{client.right - outer - save_width - s12 - chip_width,
                            dpi_.logical_to_pixels(34),
                            client.right - outer - save_width - s12,
                            dpi_.logical_to_pixels(34) + chip_height};
        (void)s4;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    [[nodiscard]] bool handle_segment_click(int x, int y) noexcept {
        POINT point{x, y};
        if (!PtInRect(&segment_bounds_, point)) return false;
        const int width = segment_bounds_.right - segment_bounds_.left;
        if (width <= 0) return false;
        const int selection = std::clamp((x - static_cast<int>(segment_bounds_.left)) * 3 / width, 0, 2);
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, selection, 0);
        mark_dirty();
        return true;
    }

    void mark_dirty() noexcept {
        dirty_ = true;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void paint_background(HDC target) noexcept {
        const nfui::ThemePalette& p = palette_;
        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        paint_header(target, client);
        paint_detail_card(target);
    }

    void paint_header(HDC target, const RECT& client) noexcept {
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        const int outer = dpi_.logical_to_pixels(24);
        const int divider_y = dpi_.logical_to_pixels(102);

        RECT title{outer, dpi_.logical_to_pixels(18), client.right / 2,
                   dpi_.logical_to_pixels(58)};
        nfui::draw_text(target, title, L"Settings", fonts_.bold(dpi_value, nfui::font_pt::xl), p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT subtitle{outer, dpi_.logical_to_pixels(59), client.right / 2,
                      dpi_.logical_to_pixels(82)};
        nfui::draw_text(target, subtitle, L"NativeFrame UI preferences",
                        fonts_.regular(dpi_value, nfui::font_pt::sm), p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        const int radius = (chip_bounds_.bottom - chip_bounds_.top) / 2;
        const nfui::Color chip_fill = nfui::alpha_blend(p.warning, p.surface, 0.15f);
        nfui::fill_rounded_rect(target, chip_bounds_, radius, chip_fill, chip_fill);
        const int dot_size = dpi_.logical_to_pixels(8);
        RECT dot{chip_bounds_.left + dpi_.logical_to_pixels(12),
                 chip_bounds_.top + (chip_bounds_.bottom - chip_bounds_.top - dot_size) / 2,
                 chip_bounds_.left + dpi_.logical_to_pixels(12) + dot_size,
                 chip_bounds_.top + (chip_bounds_.bottom - chip_bounds_.top + dot_size) / 2};
        nfui::fill_ellipse(target, dot, p.warning);
        RECT chip_text = chip_bounds_;
        chip_text.left = dot.right + dpi_.logical_to_pixels(8);
        chip_text.right -= dpi_.logical_to_pixels(10);
        nfui::draw_text(target, chip_text,
                        dirty_ ? L"Saved state: pending changes" : L"Saved state: synchronized",
                        fonts_.semibold(dpi_value, nfui::font_pt::xs), p.warning,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        RECT divider{client.left, divider_y, client.right,
                     divider_y + dpi_.logical_to_pixels(2)};
        nfui::fill_rect(target, divider, p.accent);
    }

    void paint_detail_card(HDC target) noexcept {
        if (detail_card_.right <= detail_card_.left || detail_card_.bottom <= detail_card_.top) return;
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        const int radius = dpi_.logical_to_pixels(12);
        const int pad = dpi_.logical_to_pixels(28);
        const int label_width = dpi_.logical_to_pixels(220);
        const int first_row = detail_card_.top + dpi_.logical_to_pixels(86);
        const int row_step = dpi_.logical_to_pixels(62);

        nfui::paint_drop_shadow(target, detail_card_, radius, 1, p.shadow);
        nfui::fill_rounded_rect(target, detail_card_, radius, p.surface, p.border);

        RECT eyebrow{detail_card_.left + pad, detail_card_.top + dpi_.logical_to_pixels(22),
                     detail_card_.right - pad, detail_card_.top + dpi_.logical_to_pixels(42)};
        nfui::draw_text(target, eyebrow, L"G E N E R A L",
                        fonts_.semibold(dpi_value, nfui::font_pt::xs), p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT panel_title = eyebrow;
        panel_title.top = eyebrow.bottom + dpi_.logical_to_pixels(2);
        panel_title.bottom = panel_title.top + dpi_.logical_to_pixels(30);
        nfui::draw_text(target, panel_title, L"General settings",
                        fonts_.semibold(dpi_value, nfui::font_pt::lg), p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        static constexpr std::array<const wchar_t*, 3> labels{
            L"PROFILE NAME", L"WORKSPACE ROOT", L"THEME PREFERENCE"};
        for (int index = 0; index < 3; ++index) {
            RECT label{detail_card_.left + pad, first_row + index * row_step,
                       detail_card_.left + pad + label_width - dpi_.logical_to_pixels(12),
                       first_row + index * row_step + dpi_.logical_to_pixels(38)};
            nfui::draw_text(target, label, labels[static_cast<std::size_t>(index)],
                            fonts_.semibold(dpi_value, nfui::font_pt::base), p.text,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        paint_segmented_control(target);

        const int editor_top = first_row + row_step * 3 + dpi_.logical_to_pixels(16);
        RECT rule{detail_card_.left + pad, editor_top - dpi_.logical_to_pixels(12),
                  detail_card_.right - pad, editor_top - dpi_.logical_to_pixels(11)};
        nfui::fill_rect(target, rule, p.border);
        RECT editor{detail_card_.left + pad, editor_top,
                    detail_card_.right - pad, editor_top + dpi_.logical_to_pixels(28)};
        nfui::draw_text(target, editor, L"Editor", fonts_.semibold(dpi_value, nfui::font_pt::lg), p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_segmented_control(HDC target) noexcept {
        if (segment_bounds_.right <= segment_bounds_.left) return;
        const nfui::ThemePalette& p = palette_;
        const int selected = std::clamp(
            static_cast<int>(SendMessageW(theme_combo_.hwnd(), CB_GETCURSEL, 0, 0)), 0, 2);
        const int width = segment_bounds_.right - segment_bounds_.left;
        const int radius = dpi_.logical_to_pixels(8);
        nfui::fill_rounded_rect(target, segment_bounds_, radius, p.surface, p.border);
        static constexpr std::array<const wchar_t*, 3> captions{L"Light", L"Dark", L"System"};
        for (int index = 0; index < 3; ++index) {
            RECT segment{segment_bounds_.left + width * index / 3, segment_bounds_.top,
                         segment_bounds_.left + width * (index + 1) / 3, segment_bounds_.bottom};
            if (index == selected) {
                RECT active = segment;
                active.left += dpi_.logical_to_pixels(2);
                active.top += dpi_.logical_to_pixels(2);
                active.right -= dpi_.logical_to_pixels(2);
                active.bottom -= dpi_.logical_to_pixels(2);
                nfui::fill_rounded_rect(target, active, dpi_.logical_to_pixels(6), p.accent, p.accent);
            } else if (index > 0) {
                nfui::draw_line(target, POINT{segment.left, segment.top + dpi_.logical_to_pixels(6)},
                                POINT{segment.left, segment.bottom - dpi_.logical_to_pixels(6)},
                                p.border, 1);
            }
            nfui::draw_text(target, segment, captions[static_cast<std::size_t>(index)],
                            index == selected ? fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm)
                                              : fonts_.regular(dpi_.dpi(), nfui::font_pt::sm),
                            index == selected ? p.accent_text : p.text_secondary,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::ThemePalette checkbox_palette_;
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
    RECT detail_card_{};
    RECT segment_bounds_{};
    RECT chip_bounds_{};
    bool dirty_{};
    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    SettingsDemoWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }
    return app.run();
}

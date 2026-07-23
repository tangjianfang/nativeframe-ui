#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <string_view>

namespace {

constexpr int id_categories = 101;
constexpr int id_profile_name = 102;
constexpr int id_workspace_root = 103;
constexpr int id_theme_combo = 104;
constexpr int id_telemetry = 105;
constexpr int id_startup = 106;
constexpr int id_save = 107;
constexpr int id_status_label = 108;
constexpr int id_description = 109;
constexpr int id_status_bar = 110;
constexpr int id_dirty_chip = 111;

class CategoriesListBox final : public nfui::ListBox {
protected:
    void on_reflected_draw_item(DRAWITEMSTRUCT* di) noexcept override {
        if (di == nullptr) {
            return;
        }

        const nfui::ThemePalette* pal = palette();
        const nfui::ThemePalette& p = pal ? *pal : nfui::theme_palette(nfui::ThemeMode::light);
        const bool selected = (di->itemState & ODS_SELECTED) != 0;
        const bool disabled = (di->itemState & ODS_DISABLED) != 0;
        RECT rc = di->rcItem;

        nfui::Color bg = p.surface;
        if (!selected && static_cast<int>(di->itemID) == hovered_row()) {
            bg = nfui::alpha_blend(p.surface, p.text, 0.06f);
        }

        nfui::fill_rect(di->hDC, rc, p.surface);
        const int radius = nfui::theme_metrics().corner_radius_control;
        nfui::fill_rounded_rect(di->hDC, rc, radius, bg, bg);

        if (selected) {
            const nfui::DpiScale scale(nfui::dpi_of(di->hwndItem));
            const int pad = scale.logical_to_pixels(6);
            const int bar = scale.logical_to_pixels(3);
            RECT bar_rc{rc.left + pad, rc.top + pad, rc.left + pad + bar, rc.bottom - pad};
            nfui::fill_rounded_rect(di->hDC, bar_rc, bar / 2, p.accent, p.accent);
        }

        if (di->itemID == LB_ERR) {
            return;
        }
        LRESULT len = SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        if (len != LB_ERR && len < 255) {
            wchar_t buf[256]{};
            if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, reinterpret_cast<LPARAM>(buf)) != LB_ERR) {
                const nfui::DpiScale scale(nfui::dpi_of(di->hwndItem));
                const int pad = scale.logical_to_pixels(8);
                const int bar = selected ? scale.logical_to_pixels(3) : 0;
                const int text_inset = pad + (selected ? pad + bar : 0);
                RECT text_rc = rc;
                text_rc.left += text_inset;

                nfui::FontCache* cache = fonts();
                HFONT font = nullptr;
                if (cache != nullptr) {
                    font = selected ? cache->semibold(scale.dpi(), nfui::font_pt::ui)
                                    : cache->regular(scale.dpi(), nfui::font_pt::ui);
                }
                const nfui::Color fg = disabled ? p.text_secondary : p.text;
                nfui::draw_text(di->hDC, text_rc, buf, font, fg,
                                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }
};

class SettingsDemoWindow final : public nfui::Window {
public:
    explicit SettingsDemoWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {
        dirty_palette_ = palette_;
        dirty_palette_.accent = palette_.warning;
        dirty_palette_.accent_hover = nfui::darken(palette_.warning, 0.10f);
    }

    ~SettingsDemoWindow() noexcept override {
        destroy_icons();
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUISettingsDemoWindow",
            L"NativeFrame UI SettingsDemo",
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
        update_category_copy();
        update_saved_state();
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
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            layout_controls();
            apply_native_fonts();
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
            // Flicker-free offscreen buffer over the full client area.
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                paint_background(target);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            destroy_icons();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case id_categories:
            if (notification_code == LBN_SELCHANGE) {
                update_category_copy();
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
        case id_theme_combo:
            if (notification_code == CBN_SELCHANGE) {
                mark_dirty();
                return true;
            }
            break;
        case id_telemetry:
        case id_startup:
            if (notification_code == BN_CLICKED) {
                mark_dirty();
                return true;
            }
            break;
        case id_save:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                dirty_ = false;
                update_saved_state();
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
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_categories,
            L"",
            0,
            0,
            100,
            24,
        };

        categories_.inject_theme(&palette_, &fonts_);
        profile_name_.inject_theme(&palette_, &fonts_);
        workspace_root_.inject_theme(&palette_, &fonts_);
        theme_combo_.inject_theme(&palette_, &fonts_);
        telemetry_.inject_theme(&palette_, &fonts_);
        startup_.inject_theme(&palette_, &fonts_);
        save_button_.inject_theme(&palette_, &fonts_);
        status_label_.inject_theme(&palette_, &fonts_);
        description_.inject_theme(&palette_, &fonts_);
        status_bar_.inject_theme(&palette_, &fonts_);
        dirty_chip_.inject_theme(&palette_, &fonts_);

        if (!categories_.create(params)) {
            return false;
        }

        params.control_id = id_profile_name;
        params.text = L"NativeFrame UI";
        if (!profile_name_.create(params)) {
            return false;
        }

        params.control_id = id_workspace_root;
        params.text = L"C:\\nativeframeui\\workspace";
        if (!workspace_root_.create(params)) {
            return false;
        }

        params.control_id = id_theme_combo;
        params.text = L"";
        if (!theme_combo_.create(params)) {
            return false;
        }

        params.control_id = id_telemetry;
        params.text = L"Enable optional startup telemetry";
        if (!telemetry_.create(params)) {
            return false;
        }

        params.control_id = id_startup;
        params.text = L"Restore the previous workspace on launch";
        if (!startup_.create(params)) {
            return false;
        }

        params.control_id = id_save;
        params.text = L"Save snapshot";
        nfui::ButtonStyle save_style{};
        save_style.use_semibold = true;
        save_button_.set_style(save_style);
        if (!save_button_.create(params)) {
            return false;
        }

        params.control_id = id_status_label;
        params.text = L"Saved state";
        if (!status_label_.create(params)) {
            return false;
        }

        nfui::TextStyle chip_style{};
        chip_style.background = palette_.warning;
        chip_style.foreground = palette_.text;
        chip_style.use_semibold = true;
        chip_style.horizontal_padding = 10;
        chip_style.vertical_padding = 4;
        chip_style.align_h = nfui::StaticTextAlignH::center;
        dirty_chip_.set_style(chip_style);

        params.control_id = id_dirty_chip;
        params.text = L"";
        if (!dirty_chip_.create(params)) {
            return false;
        }

        params.control_id = id_description;
        params.text = L"";
        params.style = WS_CHILD | WS_VISIBLE;
        if (!description_.create(params)) {
            return false;
        }
        nfui::TextStyle description_style{};
        description_style.single_line = false;
        description_style.align_v = nfui::StaticTextAlignV::top;
        description_style.vertical_padding = 2;
        description_.set_style(description_style);

        params.control_id = id_status_bar;
        params.text = L"";
        if (!status_bar_.create(params)) {
            return false;
        }

        SendMessageW(telemetry_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(startup_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        return true;
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        SendMessageW(categories_.hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(profile_name_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(workspace_root_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(theme_combo_.hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(telemetry_.hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(startup_.hwnd(),        WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(status_bar_.hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    void populate_controls() noexcept {
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"General"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Workspace"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Release"));
        SendMessageW(categories_.hwnd(), LB_SETCURSEL, 0, 0);

        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Follow system"));
        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
        SendMessageW(theme_combo_.hwnd(), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
        SendMessageW(theme_combo_.hwnd(), CB_SETCURSEL, 0, 0);
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }

        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON),
                                                    LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<WPARAM>(large_icon_));
        }
        if (small_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
        }
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

    [[nodiscard]] int measure_text_width(const wchar_t* text, int point_size) noexcept {
        if (text == nullptr) {
            return 0;
        }
        HDC dc = GetDC(nullptr);
        if (dc == nullptr) {
            return 0;
        }
        HFONT font = fonts_.regular(dpi_.dpi(), point_size);
        HGDIOBJ old = SelectObject(dc, font);
        int width = 0;
        SIZE sz{};
        if (GetTextExtentPoint32W(dc, text, lstrlenW(text), &sz)) {
            width = static_cast<int>(sz.cx);
        }
        if (old != nullptr && old != HGDI_ERROR) {
            SelectObject(dc, old);
        }
        ReleaseDC(nullptr, dc);
        return width;
    }

    void layout_controls() noexcept {
        if (hwnd() == nullptr || status_bar_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        SendMessageW(status_bar_.hwnd(), WM_SIZE, 0, 0);

        RECT status_rect{};
        GetWindowRect(status_bar_.hwnd(), &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;
        const int outer = dpi_.logical_to_pixels(20);
        const int gap = dpi_.logical_to_pixels(16);
        const int category_width = dpi_.logical_to_pixels(220);
        const int label_height = dpi_.logical_to_pixels(22);
        const int edit_height = dpi_.logical_to_pixels(28);
        const int checkbox_height = dpi_.logical_to_pixels(24);
        const int content_width = client.right - client.left - category_width - outer * 3;
        const int left = client.left + outer;
        const int top = client.top + outer;
        const int form_left = left + category_width + outer;
        const int form_width = std::max(content_width, dpi_.logical_to_pixels(360));
        const int safe_height = client.bottom - client.top - status_height;
        const int field_gap = dpi_.logical_to_pixels(18);
        const int first_field_top = top + dpi_.logical_to_pixels(180);
        const int block_height = label_height + edit_height + field_gap;

        MoveWindow(categories_.hwnd(),
                   left,
                   top + dpi_.logical_to_pixels(92),
                   category_width,
                   safe_height - dpi_.logical_to_pixels(132),
                   TRUE);

        // Header: status label on the left, dirty chip + save button packed to the right.
        const int save_width = dpi_.logical_to_pixels(130);
        const int chip_width = dpi_.logical_to_pixels(120);
        const int save_height = dpi_.logical_to_pixels(34);
        const int header_y = top + gap;
        const int save_top = header_y + (label_height - save_height) / 2;
        const int right_edge = client.right - client.left - outer;
        const int save_x = right_edge - save_width;
        const int chip_x = save_x - gap - chip_width;

        MoveWindow(status_label_.hwnd(),
                   form_left,
                   header_y,
                   std::max(chip_x - gap - form_left, dpi_.logical_to_pixels(120)),
                   label_height,
                   TRUE);
        MoveWindow(save_button_.hwnd(),
                   save_x,
                   save_top,
                   save_width,
                   save_height,
                   TRUE);
        MoveWindow(dirty_chip_.hwnd(),
                   chip_x,
                   save_top,
                   chip_width,
                   save_height,
                   TRUE);

        MoveWindow(description_.hwnd(),
                   form_left,
                   top + dpi_.logical_to_pixels(92),
                   form_width,
                   dpi_.logical_to_pixels(56),
                   TRUE);

        int row_top = first_field_top;
        MoveWindow(profile_name_.hwnd(), form_left, row_top + label_height, form_width, edit_height, TRUE);
        row_top += block_height;
        MoveWindow(workspace_root_.hwnd(), form_left, row_top + label_height, form_width, edit_height, TRUE);
        row_top += block_height;
        MoveWindow(theme_combo_.hwnd(), form_left, row_top + label_height, form_width, dpi_.logical_to_pixels(300), TRUE);
        row_top += block_height;

        const int check_extra = dpi_.logical_to_pixels(32);
        const int telemetry_width = measure_text_width(L"Enable optional startup telemetry", nfui::font_pt::ui) + check_extra;
        const int startup_width = measure_text_width(L"Restore the previous workspace on launch", nfui::font_pt::ui) + check_extra;
        MoveWindow(telemetry_.hwnd(), form_left, row_top, std::min(telemetry_width, form_width), checkbox_height, TRUE);
        row_top += checkbox_height + field_gap;
        MoveWindow(startup_.hwnd(), form_left, row_top, std::min(startup_width, form_width), checkbox_height, TRUE);
    }

    void mark_dirty() noexcept {
        dirty_ = true;
        update_saved_state();
    }

    void update_saved_state() noexcept {
        const wchar_t* label = dirty_ ? L"Saved state: pending changes" : L"Saved state: synchronized";
        SetWindowTextW(status_label_.hwnd(), label);
        if (status_bar_.hwnd() != nullptr) {
            SendMessageW(status_bar_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(label));
        }

        dirty_chip_.set_caption(dirty_ ? std::wstring(L"Unsaved changes") : std::wstring());
        ShowWindow(dirty_chip_.hwnd(), dirty_ ? SW_SHOW : SW_HIDE);
        save_button_.set_palette(dirty_ ? &dirty_palette_ : &palette_);

        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }

    void update_category_copy() noexcept {
        const int selected = static_cast<int>(SendMessageW(categories_.hwnd(), LB_GETCURSEL, 0, 0));
        switch (selected) {
        case 1:
            SetWindowTextW(description_.hwnd(), L"Workspace settings emphasize explicit paths, startup restore, and durable split/layout state.");
            break;
        case 2:
            SetWindowTextW(description_.hwnd(), L"Release settings capture theme intent and runtime expectations without hiding native control state.");
            break;
        case 0:
        default:
            SetWindowTextW(description_.hwnd(), L"General settings keep the demo focused on native edits, combos, check boxes, and a visible save signal.");
            break;
        }
    }

    void paint_background(HDC target) noexcept {
        const int dpi_value = dpi_.dpi();
        const nfui::DpiScale scale(dpi_value);
        const nfui::ThemePalette& p = palette_;

        RECT client{};
        GetClientRect(hwnd(), &client);
        RECT banner = client;
        banner.bottom = banner.top + scale.logical_to_pixels(116);

        nfui::fill_rect(target, client, p.background);
        nfui::fill_rect(target, banner, p.background);
        const RECT hairline{banner.left, banner.bottom - 1, banner.right, banner.bottom};
        nfui::fill_rect(target, hairline, p.accent);

        HFONT title_font = fonts_.semibold(dpi_value, 16);
        HFONT body_font = fonts_.regular(dpi_value, 9);

        RECT title = banner;
        title.left += scale.logical_to_pixels(20);
        title.top += scale.logical_to_pixels(18);
        title.right -= scale.logical_to_pixels(20);

        nfui::draw_text(target,
                        title,
                        L"SettingsDemo",
                        title_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        title.top += scale.logical_to_pixels(40);
        nfui::draw_text(target,
                        title,
                        L"Category navigation plus native edit/combo/check controls demonstrate how saved-state feedback can stay obvious in a pure Win32 shell.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        const int outer = scale.logical_to_pixels(20);
        const int top = client.top + outer;
        const int form_left = outer + scale.logical_to_pixels(220) + outer;
        const int label_height = scale.logical_to_pixels(22);
        const int edit_height = scale.logical_to_pixels(28);
        const int field_gap = scale.logical_to_pixels(18);
        const int first_field_top = top + scale.logical_to_pixels(180);
        const int block_height = label_height + edit_height + field_gap;

        RECT labels = client;
        labels.left = form_left;
        labels.top = first_field_top;
        labels.right = client.right - outer;

        RECT profile = labels;
        profile.bottom = profile.top + label_height;
        nfui::draw_text(target,
                        profile,
                        L"Profile name",
                        fonts_.semibold(dpi_value, 9),
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        profile.top += block_height;
        profile.bottom += block_height;
        nfui::draw_text(target,
                        profile,
                        L"Workspace root",
                        fonts_.semibold(dpi_value, 9),
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        profile.top += block_height;
        profile.bottom += block_height;
        nfui::draw_text(target,
                        profile,
                        L"Theme preference",
                        fonts_.semibold(dpi_value, 9),
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        if (dirty_) {
            const int save_width = scale.logical_to_pixels(130);
            const int chip_width = scale.logical_to_pixels(120);
            const int save_height = scale.logical_to_pixels(34);
            const int header_y = top + scale.logical_to_pixels(16);
            const int save_top = header_y + (label_height - save_height) / 2;
            const int right_edge = client.right - outer;
            const int save_x = right_edge - save_width;
            const int chip_x = save_x - scale.logical_to_pixels(16) - chip_width;
            const int dot_size = scale.logical_to_pixels(10);
            const int dot_x = chip_x - scale.logical_to_pixels(16) - dot_size;
            const int dot_y = save_top + (save_height - dot_size) / 2;
            RECT dot{dot_x, dot_y, dot_x + dot_size, dot_y + dot_size};
            nfui::fill_ellipse(target, dot, p.warning);
        }
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::ThemePalette dirty_palette_;
    nfui::FontCache fonts_;
    CategoriesListBox categories_;
    nfui::Edit profile_name_;
    nfui::Edit workspace_root_;
    nfui::ComboBox theme_combo_;
    nfui::CheckBox telemetry_;
    nfui::CheckBox startup_;
    nfui::Button save_button_;
    nfui::StaticText status_label_;
    nfui::StaticText dirty_chip_;
    nfui::StaticText description_;
    nfui::StatusBar status_bar_;
    nfui::DpiScale dpi_{96};
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

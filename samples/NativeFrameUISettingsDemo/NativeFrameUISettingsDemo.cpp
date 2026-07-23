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
constexpr int id_status_bar = 108;

// Owner-drawn toggle checkbox that matches the card surface and uses the
// theme accent for the checked state. BS_PUSHBUTTON keeps BN_CLICKED routing
// simple; the parent toggles the checked state on each click.
class ThemedCheckBox : public nfui::Control {
public:
    [[nodiscard]] bool create(const nfui::ControlCreateParams& params) noexcept {
        nfui::ControlCreateParams p = params;
        p.style |= WS_TABSTOP;
        return create_native(L"BUTTON", p, BS_OWNERDRAW | BS_PUSHBUTTON);
    }

    void set_checked(bool checked) noexcept {
        if (checked_ != checked) {
            checked_ = checked;
            if (valid()) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
        }
    }

    [[nodiscard]] bool checked() const noexcept { return checked_; }

protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override {
        const nfui::ThemePalette& p = palette()
            ? *palette()
            : nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::DpiScale scale(nfui::dpi_of(hwnd()));
        const int box = scale.logical_to_pixels(20);
        const int gap = scale.logical_to_pixels(10);
        const int pad = scale.logical_to_pixels(3);
        const int radius = scale.logical_to_pixels(4);
        const int stroke = scale.logical_to_pixels(2);
        const int height = state.bounds.bottom - state.bounds.top;
        const int box_top = state.bounds.top + (height - box) / 2;
        const RECT box_rc{state.bounds.left, box_top,
                          state.bounds.left + box, box_top + box};

        // Blend with the surrounding card surface.
        nfui::fill_rect(dc, state.bounds, p.surface);

        const nfui::Color box_fill = checked_ ? p.accent : p.surface;
        const nfui::Color box_border = state.focused ? p.accent : p.text_secondary;
        nfui::fill_rounded_rect(dc, box_rc, radius, box_fill, box_border);

        if (checked_) {
            const RECT check_rc{box_rc.left + pad, box_rc.top + pad,
                                box_rc.right - pad, box_rc.bottom - pad};
            nfui::draw_vector_icon(dc, nfui::IconKind::check, check_rc,
                                   p.accent_text, stroke);
        }

        RECT text_rc{state.bounds.left + box + gap, state.bounds.top,
                     state.bounds.right, state.bounds.bottom};
        HFONT font = fonts() ? fonts()->regular(scale.dpi(), nfui::font_pt::base)
                             : nullptr;
        nfui::draw_text(dc, text_rc, caption(), font,
                        state.enabled ? p.text : p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

private:
    bool checked_ = false;
};

// Owner-drawn category list that uses the larger base UI font, removes the
// native listbox border, and adds a small coral accent bar on the selected
// row so the navigation matches the rest of the polished form.
class CategoryListBox : public nfui::ListBox {
public:
    [[nodiscard]] bool create(const nfui::ControlCreateParams& params) noexcept {
        if (!nfui::ListBox::create(params)) {
            return false;
        }
        // Drop the native WS_BORDER so the nav reads as a plain surface list.
        LONG style = GetWindowLongW(hwnd(), GWL_STYLE);
        style &= ~WS_BORDER;
        SetWindowLongW(hwnd(), GWL_STYLE, style);
        SetWindowPos(hwnd(),
                     nullptr,
                     0,
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                         SWP_FRAMECHANGED);
        // Increase row height to fit the base font plus comfortable padding.
        const int row = nfui::font_pixel_height(nfui::font_pt::base,
                                                nfui::dpi_of(hwnd())) +
                        8;
        SendMessageW(hwnd(), LB_SETITEMHEIGHT, 0, static_cast<LPARAM>(row));
        return true;
    }

protected:
    void on_reflected_draw_item(DRAWITEMSTRUCT* di) noexcept override {
        if (di == nullptr) {
            return;
        }
        // Let the base implementation paint the rounded row background and its
        // default (smaller) label.
        nfui::ListBox::draw_item(di);

        // Now redraw the label at the larger base font, re-filling just the text
        // band with the current row background so the old label does not show.
        const nfui::ThemePalette& p = palette()
            ? *palette()
            : nfui::theme_palette(nfui::ThemeMode::light);
        const bool selected = (di->itemState & ODS_SELECTED) != 0;
        const bool disabled = (di->itemState & ODS_DISABLED) != 0;
        const int row = static_cast<int>(di->itemID);
        const nfui::Color bg = selected
            ? p.selection
            : (row == hovered_row()
                   ? nfui::alpha_blend(p.surface, p.text, 0.06f)
                   : p.surface);
        const nfui::Color fg = disabled ? p.text_secondary
                                        : (selected ? p.selection_text : p.text);

        const nfui::DpiScale scale(nfui::dpi_of(di->hwndItem));
        RECT text_rc = di->rcItem;
        text_rc.left += scale.logical_to_pixels(12);
        text_rc.right -= scale.logical_to_pixels(8);
        nfui::fill_rect(di->hDC, text_rc, bg);

        LRESULT len = SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        if (len != LB_ERR && len < 255) {
            wchar_t buf[256]{};
            if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID,
                             reinterpret_cast<LPARAM>(buf)) != LB_ERR) {
                HFONT font = fonts()
                    ? fonts()->regular(nfui::dpi_of(di->hwndItem), nfui::font_pt::base)
                    : nullptr;
                nfui::draw_text(di->hDC, text_rc, buf, font, fg,
                                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }

        // Coral left accent bar for the selected item.
        if (selected) {
            const int bar_w = scale.logical_to_pixels(3);
            const int pad_y = scale.logical_to_pixels(4);
            const RECT bar{di->rcItem.left,
                           di->rcItem.top + pad_y,
                           di->rcItem.left + bar_w,
                           di->rcItem.bottom - pad_y};
            nfui::fill_rounded_rect(di->hDC, bar, bar_w / 2, p.accent, p.accent);
        }
    }
};

class SettingsDemoWindow final : public nfui::Window {
public:
    explicit SettingsDemoWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {
    }

    ~SettingsDemoWindow() noexcept override {
        destroy_icons();
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
            1320,
            860,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        if (!create_children()) {
            return false;
        }

        populate_controls();
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
            if (notification_code == BN_CLICKED) {
                telemetry_.set_checked(!telemetry_.checked());
                mark_dirty();
                return true;
            }
            break;
        case id_startup:
            if (notification_code == BN_CLICKED) {
                startup_.set_checked(!startup_.checked());
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
        status_bar_.inject_theme(&palette_, &fonts_);

        // Left navigation: rounded selection on the theme surface.
        nfui::ListStyle nav_style{};
        nav_style.rounded_selection = true;
        nav_style.horizontal_padding = 12;
        categories_.set_style(nav_style);

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

        params.control_id = id_status_bar;
        params.text = L"";
        if (!status_bar_.create(params)) {
            return false;
        }

        telemetry_.set_checked(true);
        startup_.set_checked(true);
        return true;
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, nfui::font_pt::base);
        SendMessageW(categories_.hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(profile_name_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(workspace_root_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(theme_combo_.hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        // ThemedCheckBox draws its own label at font_pt::base, so no WM_SETFONT
        // broadcast is needed for these two controls.
        SendMessageW(status_bar_.hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    void populate_controls() noexcept {
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"General"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Appearance"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Editor"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Resources"));
        SendMessageW(categories_.hwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"About"));
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
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<WPARAM>(small_icon_));
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

        const int outer = dpi_.logical_to_pixels(24);
        const int gap = dpi_.logical_to_pixels(16);
        const int nav_width = dpi_.logical_to_pixels(220);
        const int banner_height = dpi_.logical_to_pixels(96);
        const int content_top = banner_height + gap;
        const int label_col = dpi_.logical_to_pixels(220);
        const int card_pad = dpi_.logical_to_pixels(24);
        const int label_height = dpi_.logical_to_pixels(18);
        const int input_height = dpi_.logical_to_pixels(36);
        const int checkbox_height = dpi_.logical_to_pixels(24);
        const int row_step = dpi_.logical_to_pixels(72);

        const int nav_left = client.left + outer;
        const int nav_top = content_top;
        const int nav_height = client.bottom - status_height - content_top - outer;
        MoveWindow(categories_.hwnd(), nav_left, nav_top, nav_width, nav_height, TRUE);

        const int card_left = nav_left + nav_width + outer;
        const int card_top = content_top;
        const int card_right = client.right - outer;
        const int card_bottom = client.bottom - status_height - outer;

        const int save_width = dpi_.logical_to_pixels(140);
        const int save_height = dpi_.logical_to_pixels(36);
        const int save_left = card_right - save_width;
        const int save_top = outer + (banner_height - outer - save_height) / 2;
        MoveWindow(save_button_.hwnd(), save_left, save_top, save_width, save_height, TRUE);

        // The status chip is painted on the banner. Size it to fit both possible
        // labels so the text never clips, and keep it a small gap away from the
        // Save button.
        const int chip_height = dpi_.logical_to_pixels(32);
        const int chip_pad_x = dpi_.logical_to_pixels(12);
        const int chip_dot = dpi_.logical_to_pixels(8);
        const int chip_dot_gap = dpi_.logical_to_pixels(8);
        const int chip_font_pt = nfui::font_pt::sm;
        int chip_text_w = 0;
        {
            HDC measure_dc = GetDC(hwnd());
            if (measure_dc != nullptr) {
                HFONT measure_font = fonts_.semibold(dpi_.dpi(), chip_font_pt);
                const HGDIOBJ old = SelectObject(measure_dc, measure_font);
                SIZE sz{};
                const std::wstring pending = L"Saved state: pending changes";
                const std::wstring synced = L"Saved state: synchronized";
                if (GetTextExtentPoint32W(measure_dc, pending.c_str(), static_cast<int>(pending.size()), &sz)) {
                    chip_text_w = sz.cx;
                }
                if (GetTextExtentPoint32W(measure_dc, synced.c_str(), static_cast<int>(synced.size()), &sz) && sz.cx > chip_text_w) {
                    chip_text_w = sz.cx;
                }
                if (old != nullptr && old != HGDI_ERROR) {
                    SelectObject(measure_dc, old);
                }
                ReleaseDC(hwnd(), measure_dc);
            }
        }
        const int chip_width = chip_text_w + chip_dot + chip_dot_gap + 2 * chip_pad_x + dpi_.logical_to_pixels(8);
        chip_rect_ = rect_from(
            card_right - save_width - gap - chip_width,
            save_top + (save_height - chip_height) / 2,
            chip_width,
            chip_height);

        const int form_left = card_left + card_pad + label_col + gap;
        const int form_right = card_right - card_pad;
        const int form_width = std::max(form_right - form_left, dpi_.logical_to_pixels(320));

        // First form row sits below the section eyebrow/title with a 24px gap.
        const int first_row_offset = dpi_.logical_to_pixels(96);
        int row_top = card_top + card_pad + first_row_offset;
        MoveWindow(profile_name_.hwnd(), form_left, row_top + label_height + dpi_.logical_to_pixels(4), form_width, input_height, TRUE);
        row_top += row_step;
        MoveWindow(workspace_root_.hwnd(), form_left, row_top + label_height + dpi_.logical_to_pixels(4), form_width, input_height, TRUE);
        row_top += row_step;
        MoveWindow(theme_combo_.hwnd(), form_left, row_top + label_height + dpi_.logical_to_pixels(4), form_width, input_height, TRUE);
        row_top += row_step;
        const int check_top = row_top + dpi_.logical_to_pixels(8);
        MoveWindow(telemetry_.hwnd(), form_left, check_top, form_width, checkbox_height, TRUE);
        MoveWindow(startup_.hwnd(), form_left, check_top + checkbox_height + dpi_.logical_to_pixels(12), form_width, checkbox_height, TRUE);

        // Stash card geometry so paint_background can draw the surface card and
        // the banner chrome without recomputing.
        card_rect_ = rect_from(card_left, card_top, card_right - card_left, card_bottom - card_top);
    }

    void mark_dirty() noexcept {
        dirty_ = true;
        update_saved_state();
    }

    void update_saved_state() noexcept {
        if (dirty_) {
            chip_text_ = L"Saved state: pending changes";
        } else {
            chip_text_ = L"Saved state: synchronized";
        }
        if (status_bar_.hwnd() != nullptr) {
            SendMessageW(status_bar_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(chip_text_.c_str()));
        }
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void paint_background(HDC target) noexcept {
        const int dpi_value = dpi_.dpi();
        const nfui::ThemePalette& p = palette_;

        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        // Banner area shares the window background and is separated by a 2px
        // coral divider at its bottom edge.
        RECT banner = client;
        banner.bottom = banner.top + dpi_.logical_to_pixels(96);
        nfui::fill_rect(target, banner, p.background);
        const int divider_height = dpi_.logical_to_pixels(2);
        const RECT divider{banner.left, banner.bottom - divider_height, banner.right, banner.bottom};
        nfui::fill_rect(target, divider, p.accent);

        // Title and subtitle on the left.
        const int outer = dpi_.logical_to_pixels(24);
        RECT title_rc = banner;
        title_rc.left += outer;
        title_rc.top += dpi_.logical_to_pixels(18);
        title_rc.right = banner.right - dpi_.logical_to_pixels(320);
        HFONT title_font = fonts_.serif(dpi_value, nfui::font_pt::xl);
        nfui::draw_text(target, title_rc, L"Settings", title_font, p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT subtitle_rc = title_rc;
        subtitle_rc.top += dpi_.logical_to_pixels(40);
        HFONT subtitle_font = fonts_.regular(dpi_value, nfui::font_pt::sm);
        nfui::draw_text(target, subtitle_rc, L"NativeFrame UI preferences", subtitle_font,
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        // Status chip on the right, above the divider.
        paint_status_chip(target);

        // Main content card.
        if (card_rect_.right > card_rect_.left && card_rect_.bottom > card_rect_.top) {
            const int radius = nfui::theme_metrics().corner_radius_card;
            nfui::fill_rect(target, card_rect_, p.background);
            nfui::fill_rounded_rect(target, card_rect_, radius, p.surface, p.border);

            // Section eyebrow and header.
            RECT header_rc = card_rect_;
            header_rc.left += dpi_.logical_to_pixels(24);
            header_rc.top += dpi_.logical_to_pixels(24);
            header_rc.right -= dpi_.logical_to_pixels(24);
            HFONT eyebrow_font = fonts_.semibold(dpi_value, nfui::font_pt::xs);
            nfui::draw_text(target, header_rc, L"GENERAL", eyebrow_font, p.text_secondary,
                            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            RECT section_rc = header_rc;
            section_rc.top += dpi_.logical_to_pixels(20);
            HFONT section_font = fonts_.semibold(dpi_value, nfui::font_pt::md);
            nfui::draw_text(target, section_rc, L"General settings", section_font, p.text,
                            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            // UPPERCASE labels in a 220px left column; right-aligned within the
            // column so the text never crowds the input controls.
            const int label_col_w = dpi_.logical_to_pixels(220);
            const int label_height_px = dpi_.logical_to_pixels(18);
            const int label_top_offset = dpi_.logical_to_pixels(96);
            const int row_step = dpi_.logical_to_pixels(72);
            const int label_left = card_rect_.left + dpi_.logical_to_pixels(24);
            HFONT label_font = fonts_.semibold(dpi_value, nfui::font_pt::xs);

            auto draw_label = [&](int row, const wchar_t* text) noexcept {
                RECT rc{
                    label_left,
                    card_rect_.top + label_top_offset + row * row_step,
                    label_left + label_col_w,
                    card_rect_.top + label_top_offset + row * row_step + label_height_px,
                };
                nfui::draw_text(target, rc, text, label_font, p.text_secondary,
                                DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            };

            draw_label(0, L"PROFILE NAME");
            draw_label(1, L"WORKSPACE ROOT");
            draw_label(2, L"THEME PREFERENCE");
        }
    }

    void paint_status_chip(HDC target) noexcept {
        if (chip_rect_.right <= chip_rect_.left || chip_rect_.bottom <= chip_rect_.top) {
            return;
        }
        const int dpi_value = dpi_.dpi();
        const nfui::ThemePalette& p = palette_;
        const bool pending = dirty_;
        const nfui::Color chip_bg = pending
            ? nfui::lighten(p.warning, 0.45f)
            : nfui::lighten(p.success, 0.45f);
        const nfui::Color dot_color = pending ? p.warning : p.success;
        const nfui::Color text_color = p.text;

        const int radius = nfui::theme_metrics().corner_radius_control;
        nfui::fill_rounded_rect(target, chip_rect_, radius, chip_bg, chip_bg);

        // Measure text so the dot and label are centred as a single unit.
        HFONT font = fonts_.semibold(dpi_value, nfui::font_pt::sm);
        int text_w = 0;
        if (font != nullptr) {
            const HGDIOBJ old = SelectObject(target, font);
            SIZE sz{};
            if (GetTextExtentPoint32W(target, chip_text_.c_str(), static_cast<int>(chip_text_.size()), &sz)) {
                text_w = sz.cx;
            }
            if (old != nullptr && old != HGDI_ERROR) {
                SelectObject(target, old);
            }
        }

        const int dot = dpi_.logical_to_pixels(8);
        const int dot_gap = dpi_.logical_to_pixels(8);
        const int pad_x = dpi_.logical_to_pixels(12);
        const int group_w = dot + dot_gap + text_w;
        const int chip_w = chip_rect_.right - chip_rect_.left;
        const int chip_h = chip_rect_.bottom - chip_rect_.top;
        const int start_x = chip_rect_.left + (chip_w - group_w) / 2;
        const int center_y = chip_rect_.top + chip_h / 2;
        const RECT dot_rc{
            start_x,
            center_y - dot / 2,
            start_x + dot,
            center_y + dot / 2,
        };
        nfui::fill_ellipse(target, dot_rc, dot_color);

        RECT text_rc = chip_rect_;
        text_rc.left = start_x + dot + dot_gap;
        text_rc.right = chip_rect_.right - pad_x;
        nfui::draw_text(target, text_rc, chip_text_, font, text_color,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    // Helpers to build logical rectangles in device-pixel form.
    static RECT rect_from(int x, int y, int w, int h) noexcept {
        return RECT{x, y, x + w, y + h};
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    CategoryListBox categories_;
    nfui::Edit profile_name_;
    nfui::Edit workspace_root_;
    nfui::ComboBox theme_combo_;
    ThemedCheckBox telemetry_;
    ThemedCheckBox startup_;
    nfui::Button save_button_;
    nfui::StatusBar status_bar_;
    nfui::DpiScale dpi_{96};
    bool dirty_{};
    HICON large_icon_{};
    HICON small_icon_{};

    RECT card_rect_{};
    RECT chip_rect_{};
    std::wstring chip_text_{L"Saved state: pending changes"};
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

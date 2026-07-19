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

[[nodiscard]] COLORREF blend(COLORREF from, COLORREF to, int percent_to) noexcept {
    percent_to = std::clamp(percent_to, 0, 100);
    const int percent_from = 100 - percent_to;
    return RGB(
        (GetRValue(from) * percent_from + GetRValue(to) * percent_to) / 100,
        (GetGValue(from) * percent_from + GetGValue(to) * percent_to) / 100,
        (GetBValue(from) * percent_from + GetBValue(to) * percent_to) / 100);
}

void fill_rect_color(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

[[nodiscard]] HFONT create_font(const nfui::DpiScale& dpi, int logical_height, int weight) {
    return CreateFontW(dpi.scale_font_height(logical_height),
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

class SettingsDemoWindow final : public nfui::Window {
public:
    explicit SettingsDemoWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance) {
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
            InvalidateRect(hwnd(), nullptr, TRUE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            paint_background(hdc);
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
        if (!save_button_.create(params)) {
            return false;
        }

        params.control_id = id_status_label;
        params.text = L"Saved state";
        if (!status_label_.create(params)) {
            return false;
        }

        params.control_id = id_description;
        params.text = L"";
        params.style = WS_CHILD | WS_VISIBLE;
        if (!description_.create(params)) {
            return false;
        }

        params.control_id = id_status_bar;
        params.text = L"";
        if (!status_bar_.create(params)) {
            return false;
        }

        SendMessageW(telemetry_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(startup_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        return true;
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
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
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
        const int gap = dpi_.logical_to_pixels(14);
        const int panel_top = dpi_.logical_to_pixels(132);
        const int category_width = dpi_.logical_to_pixels(220);
        const int label_height = dpi_.logical_to_pixels(22);
        const int edit_height = dpi_.logical_to_pixels(28);
        const int content_width = client.right - client.left - category_width - outer * 3;
        const int left = client.left + outer;
        const int top = client.top + outer;
        const int form_left = left + category_width + outer;
        const int form_width = std::max(content_width, dpi_.logical_to_pixels(360));
        const int safe_height = client.bottom - client.top - status_height;

        MoveWindow(categories_.hwnd(),
                   left,
                   top + dpi_.logical_to_pixels(92),
                   category_width,
                   safe_height - panel_top,
                   TRUE);

        MoveWindow(status_label_.hwnd(),
                   form_left,
                   top + gap,
                   form_width - dpi_.logical_to_pixels(140),
                   label_height,
                   TRUE);
        MoveWindow(save_button_.hwnd(),
                   form_left + form_width - dpi_.logical_to_pixels(130),
                   top,
                   dpi_.logical_to_pixels(130),
                   dpi_.logical_to_pixels(34),
                   TRUE);
        MoveWindow(description_.hwnd(),
                   form_left,
                   top + dpi_.logical_to_pixels(92),
                   form_width,
                   dpi_.logical_to_pixels(56),
                   TRUE);

        int row_top = top + dpi_.logical_to_pixels(176);
        MoveWindow(profile_name_.hwnd(), form_left, row_top + label_height, form_width, edit_height, TRUE);
        row_top += dpi_.logical_to_pixels(72);
        MoveWindow(workspace_root_.hwnd(), form_left, row_top + label_height, form_width, edit_height, TRUE);
        row_top += dpi_.logical_to_pixels(72);
        MoveWindow(theme_combo_.hwnd(), form_left, row_top + label_height, form_width, dpi_.logical_to_pixels(300), TRUE);
        row_top += dpi_.logical_to_pixels(76);
        MoveWindow(telemetry_.hwnd(), form_left, row_top, form_width, dpi_.logical_to_pixels(24), TRUE);
        row_top += dpi_.logical_to_pixels(36);
        MoveWindow(startup_.hwnd(), form_left, row_top, form_width, dpi_.logical_to_pixels(24), TRUE);
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

    void paint_background(HDC hdc) const {
        RECT client{};
        GetClientRect(hwnd(), &client);
        RECT banner = client;
        banner.bottom = banner.top + dpi_.logical_to_pixels(116);

        const COLORREF page = RGB(245, 247, 252);
        const COLORREF banner_fill = blend(nfui::theme_tokens(nfui::ThemeMode::light).accent, RGB(255, 255, 255), 88);
        fill_rect_color(hdc, client, page);
        fill_rect_color(hdc, banner, banner_fill);

        HFONT title_font = create_font(dpi_, -26, FW_BOLD);
        HFONT body_font = create_font(dpi_, -14, FW_NORMAL);

        RECT title = banner;
        title.left += dpi_.logical_to_pixels(20);
        title.top += dpi_.logical_to_pixels(18);
        title.right -= dpi_.logical_to_pixels(20);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(30, 36, 46));
        HGDIOBJ old_font = SelectObject(hdc, title_font);
        DrawTextW(hdc, L"SettingsDemo", -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, body_font);
        title.top += dpi_.logical_to_pixels(40);
        SetTextColor(hdc, RGB(84, 96, 116));
        DrawTextW(hdc,
                  L"Category navigation plus native edit/combo/check controls demonstrate how saved-state feedback can stay obvious in a pure Win32 shell.",
                  -1,
                  &title,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(hdc, old_font);

        const int outer = dpi_.logical_to_pixels(20);
        RECT labels = client;
        labels.left = outer + dpi_.logical_to_pixels(220) + outer;
        labels.top = dpi_.logical_to_pixels(200);
        labels.right -= outer;

        HFONT label_font = create_font(dpi_, -13, FW_SEMIBOLD);
        SetTextColor(hdc, RGB(84, 96, 116));
        old_font = SelectObject(hdc, label_font);
        RECT profile = labels;
        profile.bottom = profile.top + dpi_.logical_to_pixels(18);
        DrawTextW(hdc, L"Profile name", -1, &profile, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        profile.top += dpi_.logical_to_pixels(72);
        profile.bottom += dpi_.logical_to_pixels(72);
        DrawTextW(hdc, L"Workspace root", -1, &profile, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        profile.top += dpi_.logical_to_pixels(72);
        profile.bottom += dpi_.logical_to_pixels(72);
        DrawTextW(hdc, L"Theme preference", -1, &profile, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old_font);

        DeleteObject(label_font);
        DeleteObject(body_font);
        DeleteObject(title_font);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ListBox categories_;
    nfui::Edit profile_name_;
    nfui::Edit workspace_root_;
    nfui::ComboBox theme_combo_;
    nfui::CheckBox telemetry_;
    nfui::CheckBox startup_;
    nfui::Button save_button_;
    nfui::StaticText status_label_;
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

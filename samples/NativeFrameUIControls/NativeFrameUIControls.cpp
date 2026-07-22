#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

// P1.4 chrome theming: Tooltip / Panel / Splitter all adopt FrameStyle so
// the consumer can override their fill surfaces. We tag one of each on the
// existing demo controls so the chrome theming is visible without a separate
// page.

namespace {

constexpr int id_ok = 102;
constexpr int id_cancel = 103;
constexpr int id_list = 104;
constexpr int id_view = 105;
constexpr int id_icon = 106;
constexpr int id_theme_toggle = 107;
constexpr int id_panel = 108;
constexpr int id_splitter = 109;
constexpr int id_tooltip_target = 110;

class ControlsWindow final : public nfui::Window {
public:
    explicit ControlsWindow(HINSTANCE instance) noexcept
        : instance_(instance) {
    }

    ~ControlsWindow() noexcept {
        if (app_icon_ != nullptr) {
            DestroyIcon(app_icon_);
            app_icon_ = nullptr;
        }
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIControlsWindow",
            L"NativeFrame UI Controls",
            WS_OVERLAPPEDWINDOW,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            480,
            360,
        };

        if (!create(params)) {
            return false;
        }

        mode_ = nfui::ThemeMode::light;
        palette_ = nfui::theme_palette(mode_);

        if (!create_children()) {
            return false;
        }

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_DPICHANGED: {
            auto* new_rect = reinterpret_cast<RECT*>(lparam);
            if (new_rect != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             new_rect->left,
                             new_rect->top,
                             new_rect->right - new_rect->left,
                             new_rect->bottom - new_rect->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            const int dpi = nfui::dpi_of(hwnd());
            if (view_.hwnd() != nullptr) {
                if (HFONT f = fonts_.regular(dpi, 9)) {
                    SendMessageW(view_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(f), FALSE);
                }
            }
            if (list_.hwnd() != nullptr) {
                SendMessageW(list_.hwnd(),
                             LB_SETITEMHEIGHT,
                             0,
                             static_cast<LPARAM>(nfui::font_pixel_height(9, dpi) + 8));
                if (HFONT list_font = fonts_.regular(dpi, 9)) {
                    SendMessageW(list_.hwnd(),
                                 WM_SETFONT,
                                 reinterpret_cast<WPARAM>(list_font),
                                 TRUE);  // TRUE = redraw immediately
                }
                InvalidateRect(list_.hwnd(), nullptr, TRUE);
            }
            InvalidateRect(ok_.hwnd(), nullptr, FALSE);
            InvalidateRect(cancel_.hwnd(), nullptr, FALSE);
            InvalidateRect(theme_toggle_.hwnd(), nullptr, FALSE);
            InvalidateRect(list_.hwnd(), nullptr, FALSE);
            InvalidateRect(view_.hwnd(), nullptr, FALSE);
            InvalidateRect(icon_.hwnd(), nullptr, FALSE);
            InvalidateRect(panel_.hwnd(), nullptr, FALSE);
            InvalidateRect(splitter_.hwnd(), nullptr, FALSE);
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND: {
            const int command_id = LOWORD(wparam);
            if (command_id == id_theme_toggle) {
                apply_toggle_theme();
                return 0;
            }
            if (command_id == id_ok || command_id == id_cancel) {
                destroy();
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            RECT client{};
            GetClientRect(hwnd(), &client);
            // Flicker-free offscreen buffer over the full client area.
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                nfui::fill_rect(target, client, palette_.background);
                const int dpi = nfui::dpi_of(hwnd());
                const int header_height = nfui::font_pixel_height(16, dpi);
                RECT header_rect{16, 16, client.right - 16, 16 + header_height};
                HFONT header_font = fonts_.serif(dpi, 16);
                nfui::draw_text(target,
                                header_rect,
                                L"NativeFrame UI Controls",
                                header_font,
                                palette_.text,
                                DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY: {
            if (app_icon_ != nullptr) {
                DestroyIcon(app_icon_);
                app_icon_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
        }

        return nfui::Window::handle_message(message, wparam, lparam);
    }

private:
    template<typename ControlType>
    [[nodiscard]] bool create_child(ControlType& control,
                                    int id,
                                    const wchar_t* text,
                                    int x,
                                    int y,
                                    int w,
                                    int h) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            x,
            y,
            w,
            h,
        };
        control.inject_theme(&palette_, &fonts_);
        if (!control.create(params)) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool create_children() noexcept {
        if (!create_child(theme_toggle_, id_theme_toggle, L"Switch to dark", 360, 12, 104, 28)) {
            return false;
        }
        if (!create_child(ok_, id_ok, L"OK", 16, 56, 96, 32)) {
            return false;
        }
        if (!create_child(cancel_, id_cancel, L"Cancel", 120, 56, 96, 32)) {
            return false;
        }
        if (!create_child(icon_, id_icon, L"", 384, 56, 32, 32)) {
            return false;
        }
        app_icon_ = nfui::load_scaled_icon(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), 32, nfui::dpi_of(hwnd()));
        if (app_icon_ != nullptr) {
            icon_.set_icon(app_icon_);
        }
        if (!create_child(list_, id_list, L"", 16, 104, 200, 240)) {
            return false;
        }
        ListBox_AddString(list_.hwnd(), L"Apple");
        ListBox_AddString(list_.hwnd(), L"Banana");
        ListBox_AddString(list_.hwnd(), L"Cherry");
        ListBox_AddString(list_.hwnd(), L"Dates");

        if (!create_child(view_, id_view, L"", 232, 104, 232, 240)) {
            return false;
        }
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.cx = 200;
        column.pszText = const_cast<LPWSTR>(L"Item");
        ListView_InsertColumn(view_.hwnd(), 0, &column);

        static constexpr const wchar_t* rows[] = {L"Row 0", L"Row 1", L"Row 2"};
        int row_index = 0;
        for (const wchar_t* text : rows) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row_index++;
            item.pszText = const_cast<LPWSTR>(text);
            ListView_InsertItem(view_.hwnd(), &item);
        }

        // P1.4 chrome theming: a Panel sitting underneath the OK button and a
        // vertical Splitter drag-handle to the right of it. Both adopt
        // FrameStyle overrides so the consumer can prove the chrome tracks
        // the palette without any subclass work.
        if (!create_child(panel_, id_panel, L"", 16, 96, 96, 244)) {
            return false;
        }
        nfui::FrameStyle panel_style{};
        // Surface_brush falls back to palette.surface if unset, but we set it
        // explicitly to highlight the override path.
        panel_style.surface_brush = palette_.surface_hover;
        panel_style.accent        = palette_.accent;
        panel_.set_style(panel_style);

        if (!create_child(splitter_, id_splitter, L"", 114, 96, 6, 244)) {
            return false;
        }
        nfui::FrameStyle splitter_style{};
        splitter_style.surface_brush = palette_.accent;
        splitter_.set_style(splitter_style);

        // Tooltip — invisible at rest, attached to the OK button via the
        // documented TTM_ADDTOOL message. Demonstrates the chrome_text /
        // chrome_bg FrameStyle fields applied through Tooltip::create().
        if (!create_child(tooltip_, id_tooltip_target, L"", 0, 0, 0, 0)) {
            return false;
        }
        nfui::FrameStyle tooltip_style{};
        tooltip_style.chrome_text = palette_.text;
        tooltip_style.chrome_bg    = palette_.surface;
        tooltip_.set_style(tooltip_style);
        TOOLINFOW tool_info{};
        tool_info.cbSize   = sizeof(tool_info);
        tool_info.uFlags   = TTF_SUBCLASS | TTF_IDISHWND;
        tool_info.hwnd     = hwnd();
        tool_info.uId      = reinterpret_cast<UINT_PTR>(ok_.hwnd());
        tool_info.lpszText = const_cast<LPWSTR>(L"V1.4 chrome — palette.surface tooltip");
        SendMessageW(tooltip_.hwnd(), TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&tool_info));

        return true;
    }

    HINSTANCE instance_{};
    HICON app_icon_{};
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::ThemePalette palette_{};
    nfui::FontCache fonts_{};
    nfui::Button theme_toggle_;
    nfui::Button ok_;
    nfui::Button cancel_;
    nfui::ListBox list_;
    nfui::ListView view_;
    nfui::IconView icon_;
    nfui::Panel panel_;
    nfui::Splitter splitter_;
    nfui::Tooltip tooltip_;

    void apply_toggle_theme() noexcept {
        mode_ = (mode_ == nfui::ThemeMode::dark) ? nfui::ThemeMode::light : nfui::ThemeMode::dark;
        palette_ = nfui::theme_palette(mode_);
        theme_toggle_.set_palette(&palette_);
        ok_.set_palette(&palette_);
        cancel_.set_palette(&palette_);
        list_.set_palette(&palette_);
        view_.set_palette(&palette_);
        icon_.set_palette(&palette_);
        panel_.set_palette(&palette_);
        splitter_.set_palette(&palette_);
        tooltip_.set_palette(&palette_);
        // Refresh FrameStyle overrides so chrome tracks the new palette.
        nfui::FrameStyle panel_style{};
        panel_style.surface_brush = palette_.surface_hover;
        panel_style.accent        = palette_.accent;
        panel_.set_style(panel_style);
        nfui::FrameStyle splitter_style{};
        splitter_style.surface_brush = palette_.accent;
        splitter_.set_style(splitter_style);
        // Tooltip style stores concrete colours, so rebuild it from the new
        // palette before updating the live ComCtl32 tooltip window.
        nfui::FrameStyle tooltip_style{};
        tooltip_style.chrome_text = palette_.text;
        tooltip_style.chrome_bg   = palette_.surface;
        tooltip_.set_style(tooltip_style);
        if (tooltip_.hwnd() != nullptr) {
            SendMessageW(tooltip_.hwnd(), TTM_SETTIPTEXTCOLOR, 0, palette_.text.rgb);
            SendMessageW(tooltip_.hwnd(), TTM_SETTIPBKCOLOR,   0, palette_.surface.rgb);
        }
        SetWindowTextW(theme_toggle_.hwnd(),
                       mode_ == nfui::ThemeMode::dark ? L"Switch to light" : L"Switch to dark");
        InvalidateRect(theme_toggle_.hwnd(), nullptr, FALSE);
        InvalidateRect(ok_.hwnd(), nullptr, FALSE);
        InvalidateRect(cancel_.hwnd(), nullptr, FALSE);
        InvalidateRect(list_.hwnd(), nullptr, FALSE);
        InvalidateRect(view_.hwnd(), nullptr, FALSE);
        InvalidateRect(icon_.hwnd(), nullptr, FALSE);
        InvalidateRect(panel_.hwnd(), nullptr, FALSE);
        InvalidateRect(splitter_.hwnd(), nullptr, FALSE);
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    ControlsWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}

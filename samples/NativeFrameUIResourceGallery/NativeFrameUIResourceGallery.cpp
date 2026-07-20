#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <string>
#include <windowsx.h>

namespace {

constexpr int id_open_dialog = 101;
constexpr int id_reload_assets = 102;
constexpr int id_status_bar = 103;

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

INT_PTR CALLBACK gallery_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM) {
    if (message == WM_INITDIALOG) {
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK) {
        EndDialog(hwnd, IDOK);
        return TRUE;
    }
    return FALSE;
}

class ResourceGalleryWindow final : public nfui::Window {
public:
    explicit ResourceGalleryWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {
    }

    ~ResourceGalleryWindow() noexcept override {
        release_assets();
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

        if (!create_children()) {
            return false;
        }

        load_gallery_assets();
        apply_menu_and_icon();
        layout_controls();
        update_status(L"Resources loaded from explicit framework assets.");

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
            // The status bar's WM_SETFONT handle is DPI-cached, so re-apply it
            // after the DPI bump so the chrome tracks the new font face.
            apply_native_fonts();
            layout_controls();
            InvalidateRect(hwnd(), nullptr, FALSE);
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
        if (command_id == IDM_NFUI_EXIT && notification_code == 0) {
            destroy();
            return true;
        }
        if (command_id == id_open_dialog && (notification_code == BN_CLICKED || notification_code == 0)) {
            static_cast<void>(resources_.show_modal_dialog(IDD_NFUI_ABOUT, hwnd(), gallery_dialog_proc, 0));
            update_status(L"Modal resource dialog opened successfully.");
            return true;
        }
        if (command_id == id_reload_assets && (notification_code == BN_CLICKED || notification_code == 0)) {
            load_gallery_assets();
            apply_menu_and_icon();
            update_status(L"Resource handles reloaded from the explicit HINSTANCE.");
            InvalidateRect(hwnd(), nullptr, FALSE);
            return true;
        }
        return false;
    }

private:
    [[nodiscard]] bool create_children() noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_open_dialog,
            L"Open resource dialog",
            0,
            0,
            100,
            24,
        };

        // Both buttons self-paint in coral via the shared Button path; inject
        // the Claude palette + Segoe UI font so they match the rest of the
        // shell.
        open_dialog_.set_palette(&palette_);
        open_dialog_.set_font_cache(&fonts_);
        reload_assets_.set_palette(&palette_);
        reload_assets_.set_font_cache(&fonts_);

        if (!open_dialog_.create(params)) {
            return false;
        }

        params.control_id = id_reload_assets;
        params.text = L"Reload assets";
        if (!reload_assets_.create(params)) {
            return false;
        }

        params.control_id = id_status_bar;
        params.text = L"";
        if (!status_bar_.create(params)) {
            return false;
        }

        // Native StatusBar keeps its Win32 chrome but adopts Segoe UI so the
        // status text matches the shared paint. lParam=TRUE forces an immediate
        // redraw with the new font.
        apply_native_fonts();

        return true;
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        SendMessageW(status_bar_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
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

        RECT client{};
        GetClientRect(hwnd(), &client);
        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        SendMessageW(status_bar_.hwnd(), WM_SIZE, 0, 0);

        RECT status_rect{};
        GetWindowRect(status_bar_.hwnd(), &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;
        const int outer = dpi_.logical_to_pixels(20);
        const int gap = dpi_.logical_to_pixels(12);
        const int button_width = dpi_.logical_to_pixels(160);
        const int button_height = dpi_.logical_to_pixels(34);

        MoveWindow(open_dialog_.hwnd(),
                   client.left + outer,
                   client.top + outer,
                   button_width,
                   button_height,
                   TRUE);
        MoveWindow(reload_assets_.hwnd(),
                   client.left + outer + button_width + gap,
                   client.top + outer,
                   button_width,
                   button_height,
                   TRUE);

        content_rect_ = client;
        content_rect_.top += button_height + outer + gap;
        content_rect_.left += outer;
        content_rect_.right -= outer;
        content_rect_.bottom -= status_height + outer;
    }

    void update_status(const wchar_t* text) noexcept {
        if (status_bar_.hwnd() != nullptr) {
            SendMessageW(status_bar_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text));
        }
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

    void paint_gallery(HDC target) {
        const nfui::ThemePalette& p = palette_;
        const int gap = dpi_.logical_to_pixels(16);
        const int card_radius = dpi_.logical_to_pixels(nfui::theme_metrics().corner_radius_card);
        const int small_radius = dpi_.logical_to_pixels(nfui::theme_metrics().corner_radius_control);

        RECT client{};
        GetClientRect(hwnd(), &client);
        nfui::fill_rect(target, client, p.background);

        RECT summary = make_rect(content_rect_.left,
                                 content_rect_.top,
                                 rect_width(content_rect_),
                                 dpi_.logical_to_pixels(104));
        RECT assets = make_rect(content_rect_.left,
                                summary.bottom + gap,
                                (rect_width(content_rect_) - gap) / 2,
                                rect_height(content_rect_) - rect_height(summary) - gap);
        RECT preview = make_rect(assets.right + gap,
                                 assets.top,
                                 rect_width(content_rect_) - rect_width(assets) - gap,
                                 rect_height(assets));

        nfui::fill_rounded_rect(target, summary, card_radius, p.surface, p.border);
        nfui::fill_rounded_rect(target, assets, card_radius, p.surface, p.border);
        nfui::fill_rounded_rect(target, preview, card_radius, p.surface_hover, p.border);

        const int dpi_value = dpi_.dpi();
        HFONT title_font = fonts_.serif(dpi_value, 16);
        HFONT section_font = fonts_.semibold(dpi_value, 11);
        HFONT body_font = fonts_.regular(dpi_value, 9);

        RECT text = summary;
        text.left += gap;
        text.top += gap;
        text.right -= gap;
        nfui::draw_text(target,
                        text,
                        L"ResourceGallery",
                        title_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        text.top += dpi_.logical_to_pixels(34);
        nfui::draw_text(target,
                        text,
                        L"Everything in this demo loads through an explicit resource module handle: strings, menus, dialogs, icons, bitmaps, and the toolbar resource marker.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT left_text = assets;
        left_text.left += gap;
        left_text.top += gap;
        left_text.right -= gap;
        nfui::draw_text(target,
                        left_text,
                        L"Asset checklist",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        left_text.top += dpi_.logical_to_pixels(32);

        const std::wstring summary_text =
            L"String: " + std::wstring(has_string_ ? L"loaded" : L"missing") + L"\n" +
            L"Menu: " + std::wstring(has_menu_ ? L"loaded" : L"missing") + L"\n" +
            L"Dialog: " + std::wstring(has_dialog_ ? L"loaded" : L"missing") + L"\n" +
            L"Icon: " + std::wstring(has_icon_ ? L"loaded" : L"missing") + L"\n" +
            L"Bitmap: " + std::wstring(has_bitmap_ ? L"loaded" : L"missing") + L"\n" +
            L"Toolbar marker: " + std::wstring(has_toolbar_ ? L"present" : L"missing") + L"\n" +
            L"Loaded title: " + title_;
        nfui::draw_text(target,
                        left_text,
                        summary_text,
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT preview_title = preview;
        preview_title.left += gap;
        preview_title.top += gap;
        preview_title.right -= gap;
        nfui::draw_text(target,
                        preview_title,
                        L"Preview",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT icon_rect = make_rect(preview.left + gap,
                                   preview.top + dpi_.logical_to_pixels(54),
                                   dpi_.logical_to_pixels(72),
                                   dpi_.logical_to_pixels(72));
        nfui::fill_rounded_rect(target, icon_rect, small_radius, p.surface, p.border);
        if (icon_ != nullptr) {
            DrawIconEx(target,
                       icon_rect.left + dpi_.logical_to_pixels(16),
                       icon_rect.top + dpi_.logical_to_pixels(16),
                       icon_,
                       dpi_.logical_to_pixels(40),
                       dpi_.logical_to_pixels(40),
                       0,
                       nullptr,
                       DI_NORMAL);
        }

        RECT bitmap_rect = make_rect(icon_rect.right + gap,
                                     icon_rect.top,
                                     rect_width(preview) - dpi_.logical_to_pixels(120),
                                     dpi_.logical_to_pixels(180));
        nfui::fill_rounded_rect(target, bitmap_rect, small_radius, p.surface, p.border);
        if (bitmap_ != nullptr) {
            BITMAP info{};
            GetObjectW(bitmap_, sizeof(info), &info);
            HDC memory_dc = CreateCompatibleDC(target);
            HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap_);
            StretchBlt(target,
                       bitmap_rect.left + dpi_.logical_to_pixels(16),
                       bitmap_rect.top + dpi_.logical_to_pixels(16),
                       rect_width(bitmap_rect) - dpi_.logical_to_pixels(32),
                       rect_height(bitmap_rect) - dpi_.logical_to_pixels(32),
                       memory_dc,
                       0,
                       0,
                       info.bmWidth,
                       info.bmHeight,
                       SRCCOPY);
            SelectObject(memory_dc, old_bitmap);
            DeleteDC(memory_dc);
        }

        RECT preview_copy = make_rect(preview.left + gap,
                                      bitmap_rect.bottom + gap,
                                      rect_width(preview) - gap * 2,
                                      rect_height(preview) - rect_height(bitmap_rect) - gap * 3);
        nfui::draw_text(target,
                        preview_copy,
                        L"Use the File menu to route the shared Exit command, the button to open the resource dialog, and Reload assets to confirm the gallery rebinds raw Win32 resource handles without hidden globals.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // FontCache owns the HFONT handles; they are released by fonts_' destructor.
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::Button open_dialog_;
    nfui::Button reload_assets_;
    nfui::StatusBar status_bar_;
    nfui::DpiScale dpi_{96};
    RECT content_rect_{};
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    ResourceGalleryWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}
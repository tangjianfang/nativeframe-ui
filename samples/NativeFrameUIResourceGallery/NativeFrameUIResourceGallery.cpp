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

void fill_rect_color(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void draw_round_panel(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
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
          resources_(instance) {
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
            layout_controls();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            paint_gallery(hdc);
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

        return true;
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

    void paint_gallery(HDC hdc) const {
        const nfui::ThemeTokens tokens = nfui::theme_tokens(nfui::ThemeMode::light);
        const COLORREF page = RGB(247, 249, 253);
        const COLORREF panel = RGB(255, 255, 255);
        const COLORREF panel_alt = RGB(239, 244, 251);
        const COLORREF border = RGB(214, 220, 230);
        const COLORREF muted = RGB(86, 98, 116);
        const int gap = dpi_.logical_to_pixels(16);
        const int radius = dpi_.logical_to_pixels(18);
        const int small_radius = dpi_.logical_to_pixels(12);

        RECT client{};
        GetClientRect(hwnd(), &client);
        fill_rect_color(hdc, client, page);

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

        draw_round_panel(hdc, summary, panel, border, radius);
        draw_round_panel(hdc, assets, panel, border, radius);
        draw_round_panel(hdc, preview, panel_alt, border, radius);

        HFONT title_font = create_font(dpi_, -24, FW_BOLD);
        HFONT section_font = create_font(dpi_, -17, FW_SEMIBOLD);
        HFONT body_font = create_font(dpi_, -14, FW_NORMAL);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(28, 34, 44));
        HGDIOBJ old_font = SelectObject(hdc, title_font);
        RECT text = summary;
        text.left += gap;
        text.top += gap;
        text.right -= gap;
        DrawTextW(hdc, L"ResourceGallery", -1, &text, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, body_font);
        text.top += dpi_.logical_to_pixels(34);
        SetTextColor(hdc, muted);
        DrawTextW(hdc,
                  L"Everything in this demo loads through an explicit resource module handle: strings, menus, dialogs, icons, bitmaps, and the toolbar resource marker.",
                  -1,
                  &text,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        SetTextColor(hdc, RGB(28, 34, 44));
        SelectObject(hdc, section_font);
        RECT left_text = assets;
        left_text.left += gap;
        left_text.top += gap;
        left_text.right -= gap;
        DrawTextW(hdc, L"Asset checklist", -1, &left_text, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, body_font);
        SetTextColor(hdc, muted);
        left_text.top += dpi_.logical_to_pixels(32);

        const std::wstring summary_text =
            L"String: " + std::wstring(has_string_ ? L"loaded" : L"missing") + L"\n" +
            L"Menu: " + std::wstring(has_menu_ ? L"loaded" : L"missing") + L"\n" +
            L"Dialog: " + std::wstring(has_dialog_ ? L"loaded" : L"missing") + L"\n" +
            L"Icon: " + std::wstring(has_icon_ ? L"loaded" : L"missing") + L"\n" +
            L"Bitmap: " + std::wstring(has_bitmap_ ? L"loaded" : L"missing") + L"\n" +
            L"Toolbar marker: " + std::wstring(has_toolbar_ ? L"present" : L"missing") + L"\n" +
            L"Loaded title: " + title_;
        DrawTextW(hdc, summary_text.c_str(), -1, &left_text, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT preview_title = preview;
        preview_title.left += gap;
        preview_title.top += gap;
        preview_title.right -= gap;
        SetTextColor(hdc, RGB(28, 34, 44));
        SelectObject(hdc, section_font);
        DrawTextW(hdc, L"Preview", -1, &preview_title, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT icon_rect = make_rect(preview.left + gap,
                                   preview.top + dpi_.logical_to_pixels(54),
                                   dpi_.logical_to_pixels(72),
                                   dpi_.logical_to_pixels(72));
        draw_round_panel(hdc, icon_rect, panel, border, small_radius);
        if (icon_ != nullptr) {
            DrawIconEx(hdc,
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
        draw_round_panel(hdc, bitmap_rect, panel, border, small_radius);
        if (bitmap_ != nullptr) {
            BITMAP info{};
            GetObjectW(bitmap_, sizeof(info), &info);
            HDC memory_dc = CreateCompatibleDC(hdc);
            HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap_);
            StretchBlt(hdc,
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
        SelectObject(hdc, body_font);
        SetTextColor(hdc, muted);
        DrawTextW(hdc,
                  L"Use the File menu to route the shared Exit command, the button to open the resource dialog, and Reload assets to confirm the gallery rebinds raw Win32 resource handles without hidden globals.",
                  -1,
                  &preview_copy,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        SelectObject(hdc, old_font);
        DeleteObject(body_font);
        DeleteObject(section_font);
        DeleteObject(title_font);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
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

#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <windowsx.h>

namespace {

[[nodiscard]] int rect_width(const RECT& rect) noexcept {
    return rect.right - rect.left;
}

[[nodiscard]] int rect_height(const RECT& rect) noexcept {
    return rect.bottom - rect.top;
}

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + std::max(width, 0);
    rect.bottom = top + std::max(height, 0);
    return rect;
}

[[nodiscard]] RECT inset_rect(const RECT& rect, int amount) noexcept {
    RECT inset = rect;
    inset.left += amount;
    inset.top += amount;
    inset.right -= amount;
    inset.bottom -= amount;
    return inset;
}

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

void draw_text_block(HDC hdc,
                     const RECT& rect,
                     std::wstring_view text,
                     COLORREF color,
                     HFONT font,
                     UINT format) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ old_font = SelectObject(hdc, font);
    RECT copy = rect;
    DrawTextW(hdc, text.data(), static_cast<int>(text.size()), &copy, format);
    SelectObject(hdc, old_font);
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

constexpr std::array<std::wstring_view, 4> navigation_items{
    L"Surface",
    L"Canvas",
    L"Tokens",
    L"Publish",
};

constexpr std::array<std::wstring_view, 4> status_lines{
    L"Surface review active.",
    L"Preview canvas active.",
    L"Theme token sheet active.",
    L"Publish checklist active.",
};

class DarkStudioWindow final : public nfui::Window {
public:
    explicit DarkStudioWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance) {
    }

    ~DarkStudioWindow() noexcept override {
        destroy_icons();
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIDarkStudioWindow",
            L"NativeFrame UI DarkStudio",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1320,
            840,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        if (!create_children()) {
            return false;
        }

        refresh_layout();
        update_status();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            refresh_layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            if (select_navigation({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
                update_status();
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
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
            refresh_layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            paint_shell(hdc);
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

private:
    [[nodiscard]] bool create_children() noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            101,
            L"",
            0,
            0,
            100,
            24,
        };
        if (!status_.create(params)) {
            return false;
        }
        return true;
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

    void refresh_layout() noexcept {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        GetClientRect(hwnd(), &client_rect_);
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);
    }

    [[nodiscard]] int status_height() const noexcept {
        if (status_.hwnd() == nullptr) {
            return 0;
        }

        RECT status_rect{};
        GetWindowRect(status_.hwnd(), &status_rect);
        return status_rect.bottom - status_rect.top;
    }

    [[nodiscard]] RECT content_rect() const noexcept {
        RECT content = client_rect_;
        content.bottom -= status_height();
        return content;
    }

    [[nodiscard]] RECT navigation_rect(std::size_t index) const noexcept {
        const RECT content = content_rect();
        const int outer = dpi_.logical_to_pixels(20);
        const int nav_width = dpi_.logical_to_pixels(210);
        const int nav_height = dpi_.logical_to_pixels(44);
        const int nav_gap = dpi_.logical_to_pixels(10);
        const int top = content.top + dpi_.logical_to_pixels(120) + static_cast<int>(index) * (nav_height + nav_gap);
        return make_rect(content.left + outer, top, nav_width - outer, nav_height);
    }

    [[nodiscard]] bool select_navigation(POINT point) noexcept {
        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT rect = navigation_rect(index);
            if (PtInRect(&rect, point) != FALSE && selected_navigation_ != static_cast<int>(index)) {
                selected_navigation_ = static_cast<int>(index);
                return true;
            }
        }
        return false;
    }

    void update_status() noexcept {
        if (status_.hwnd() != nullptr) {
            SendMessageW(status_.hwnd(),
                         SB_SETTEXTW,
                         0,
                         reinterpret_cast<LPARAM>(status_lines[static_cast<std::size_t>(selected_navigation_)].data()));
        }
    }

    void paint_shell(HDC hdc) const {
        const nfui::ThemeTokens tokens = nfui::theme_tokens(nfui::ThemeMode::dark);
        const RECT content = content_rect();
        const int outer = dpi_.logical_to_pixels(20);
        const int gap = dpi_.logical_to_pixels(16);
        const int rail_width = dpi_.logical_to_pixels(220);
        const int preview_width = std::max(rect_width(content) - rail_width - outer * 2 - gap, dpi_.logical_to_pixels(520));
        const int preview_height = dpi_.logical_to_pixels(360);
        const int radius = dpi_.logical_to_pixels(18);
        const int small_radius = dpi_.logical_to_pixels(12);

        const COLORREF background = blend(tokens.window_background, RGB(8, 12, 18), 32);
        const COLORREF rail_fill = blend(tokens.window_background, RGB(15, 20, 30), 42);
        const COLORREF chrome = blend(tokens.window_background, RGB(35, 40, 52), 44);
        const COLORREF surface = blend(tokens.window_background, RGB(44, 52, 66), 38);
        const COLORREF border = RGB(73, 81, 97);
        const COLORREF muted = RGB(165, 176, 194);

        fill_rect_color(hdc, content, background);

        RECT rail = make_rect(content.left + outer,
                              content.top + outer,
                              rail_width,
                              rect_height(content) - outer * 2);
        RECT header = make_rect(rail.right + gap,
                                content.top + outer,
                                preview_width,
                                dpi_.logical_to_pixels(88));
        RECT preview = make_rect(header.left,
                                 header.bottom + gap,
                                 preview_width,
                                 preview_height);
        RECT metric_one = make_rect(preview.left,
                                    preview.bottom + gap,
                                    (preview_width - gap) / 2,
                                    dpi_.logical_to_pixels(132));
        RECT metric_two = make_rect(metric_one.right + gap,
                                    metric_one.top,
                                    rect_width(metric_one),
                                    rect_height(metric_one));

        draw_round_panel(hdc, rail, rail_fill, border, radius);
        draw_round_panel(hdc, header, chrome, border, radius);
        draw_round_panel(hdc, preview, surface, border, radius);
        draw_round_panel(hdc, metric_one, chrome, border, radius);
        draw_round_panel(hdc, metric_two, chrome, border, radius);

        HFONT title_font = create_font(dpi_, -28, FW_BOLD);
        HFONT section_font = create_font(dpi_, -18, FW_SEMIBOLD);
        HFONT body_font = create_font(dpi_, -14, FW_NORMAL);
        HFONT metric_font = create_font(dpi_, -24, FW_BOLD);

        RECT rail_title = make_rect(rail.left + gap,
                                    rail.top + gap,
                                    rect_width(rail) - gap * 2,
                                    dpi_.logical_to_pixels(72));
        draw_text_block(hdc, rail_title, L"DarkStudio", tokens.window_text, title_font, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        rail_title.top += dpi_.logical_to_pixels(36);
        draw_text_block(hdc,
                        rail_title,
                        L"A focused dark-shell sample for preview-heavy desktop tools.",
                        muted,
                        body_font,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT item = navigation_rect(index);
            if (selected_navigation_ == static_cast<int>(index)) {
                draw_round_panel(hdc, item, blend(tokens.accent, background, 72), tokens.accent, small_radius);
            }
            RECT label = inset_rect(item, dpi_.logical_to_pixels(12));
            draw_text_block(hdc,
                            label,
                            navigation_items[index],
                            selected_navigation_ == static_cast<int>(index) ? tokens.window_text : muted,
                            section_font,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        RECT header_text = inset_rect(header, gap);
        draw_text_block(hdc,
                        header_text,
                        L"Preview-first shell",
                        tokens.window_text,
                        section_font,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        header_text.top += dpi_.logical_to_pixels(30);
        draw_text_block(hdc,
                        header_text,
                        L"Native status bar, dark chrome, and a central canvas keep this demo anchored in HWND-friendly patterns.",
                        muted,
                        body_font,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT preview_title = inset_rect(preview, gap);
        draw_text_block(hdc,
                        preview_title,
                        L"Preview canvas",
                        tokens.window_text,
                        section_font,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT canvas = make_rect(preview.left + gap,
                                preview.top + dpi_.logical_to_pixels(56),
                                rect_width(preview) - gap * 2,
                                preview_height - dpi_.logical_to_pixels(76));
        draw_round_panel(hdc, canvas, blend(tokens.accent, background, 88), blend(tokens.accent, border, 36), radius);

        RECT strip = make_rect(canvas.left + dpi_.logical_to_pixels(18),
                               canvas.top + dpi_.logical_to_pixels(18),
                               rect_width(canvas) - dpi_.logical_to_pixels(36),
                               dpi_.logical_to_pixels(46));
        draw_round_panel(hdc, strip, blend(tokens.window_background, RGB(58, 67, 82), 36), border, small_radius);
        RECT strip_text = inset_rect(strip, dpi_.logical_to_pixels(10));
        draw_text_block(hdc,
                        strip_text,
                        L"Live preview • navigation updates the shell without leaving native Win32 controls behind.",
                        tokens.window_text,
                        body_font,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT canvas_caption = make_rect(canvas.left + dpi_.logical_to_pixels(18),
                                        strip.bottom + dpi_.logical_to_pixels(18),
                                        rect_width(canvas) - dpi_.logical_to_pixels(36),
                                        dpi_.logical_to_pixels(100));
        draw_text_block(hdc,
                        canvas_caption,
                        L"DarkStudio is intentionally self-contained. It proves that product-level dark shells can live in a sample executable without forcing framework consumers to adopt new core rendering APIs.",
                        muted,
                        body_font,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT metric_text = inset_rect(metric_one, gap);
        draw_text_block(hdc, metric_text, L"Native shell", muted, body_font, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(28);
        draw_text_block(hdc, metric_text, L"100%", tokens.window_text, metric_font, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(42);
        draw_text_block(hdc, metric_text, L"Sample-local painting with a standard status bar.", muted, body_font, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        metric_text = inset_rect(metric_two, gap);
        draw_text_block(hdc, metric_text, L"DPI layout", muted, body_font, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(28);
        draw_text_block(hdc, metric_text, L"Per-monitor", tokens.window_text, metric_font, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(42);
        draw_text_block(hdc, metric_text, L"All shell measurements scale through nfui::DpiScale.", muted, body_font, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        DeleteObject(metric_font);
        DeleteObject(body_font);
        DeleteObject(section_font);
        DeleteObject(title_font);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::StatusBar status_;
    nfui::DpiScale dpi_{96};
    RECT client_rect_{};
    int selected_navigation_{};
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

    DarkStudioWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}

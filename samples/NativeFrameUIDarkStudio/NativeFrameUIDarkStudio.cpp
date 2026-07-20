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
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::dark)) {
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
        // StatusBar is native Win32; inject deps for consistency and apply
        // Segoe UI via WM_SETFONT so the chrome text matches the rest of the shell.
        status_.set_palette(&palette_);
        status_.set_font_cache(&fonts_);
        if (!status_.create(params)) {
            return false;
        }
        SendMessageW(status_.hwnd(),
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(dpi_.dpi(), 9)),
                     TRUE);
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

    void paint_shell(HDC hdc) {
        const nfui::ThemePalette& p = palette_;
        const RECT content = content_rect();
        const int outer = dpi_.logical_to_pixels(20);
        const int gap = dpi_.logical_to_pixels(16);
        const int rail_width = dpi_.logical_to_pixels(220);
        const int preview_width = std::max(rect_width(content) - rail_width - outer * 2 - gap, dpi_.logical_to_pixels(520));
        const int preview_height = dpi_.logical_to_pixels(360);
        const int card_radius = dpi_.logical_to_pixels(nfui::theme_metrics().corner_radius_card);
        const int pill_radius = dpi_.logical_to_pixels(nfui::theme_metrics().corner_radius_control);

        // Flicker-free offscreen buffer scoped to the content area (above the
        // native status bar). BeginPaint DC origin is the window client; the
        // MemoryDC BitBlts back at the rect's actual origin on destruction.
        nfui::MemoryDC mem(hdc, content);
        HDC target = mem.valid() ? mem.dc() : hdc;

        const int dpi_value = dpi_.dpi();
        const nfui::Color rail_fill = nfui::darken(p.background, 0.18f);
        const nfui::Color chrome = p.surface;
        const nfui::Color panel = p.surface;
        const nfui::Color surface_alt = p.surface_hover;
        const nfui::Color selection_fill = p.selection;
        const nfui::Color accent_border = p.accent;
        const nfui::Color canvas_fill = nfui::alpha_blend(p.accent, p.background, 0.88f);
        const nfui::Color canvas_border = p.accent;
        const nfui::Color muted = p.text_secondary;
        const nfui::Color body = p.text;

        nfui::fill_rect(target, content, p.background);

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

        nfui::fill_rounded_rect(target, rail, card_radius, rail_fill, p.border);
        nfui::fill_rounded_rect(target, header, card_radius, chrome, p.border);
        nfui::fill_rounded_rect(target, preview, card_radius, panel, p.border);
        nfui::fill_rounded_rect(target, metric_one, card_radius, chrome, p.border);
        nfui::fill_rounded_rect(target, metric_two, card_radius, chrome, p.border);

        HFONT title_font = fonts_.serif(dpi_value, 20);
        HFONT section_font = fonts_.semibold(dpi_value, 12);
        HFONT body_font = fonts_.regular(dpi_value, 9);
        HFONT metric_font = fonts_.mono(dpi_value, 16);

        RECT rail_title = make_rect(rail.left + gap,
                                    rail.top + gap,
                                    rect_width(rail) - gap * 2,
                                    dpi_.logical_to_pixels(72));
        nfui::draw_text(target,
                        rail_title,
                        L"DarkStudio",
                        title_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        rail_title.top += dpi_.logical_to_pixels(36);
        nfui::draw_text(target,
                        rail_title,
                        L"A focused dark-shell sample for preview-heavy desktop tools.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT item = navigation_rect(index);
            const bool selected = selected_navigation_ == static_cast<int>(index);
            if (selected) {
                nfui::fill_rounded_rect(target, item, pill_radius, selection_fill, accent_border);
            }
            RECT label = inset_rect(item, dpi_.logical_to_pixels(12));
            nfui::draw_text(target,
                            label,
                            navigation_items[index],
                            section_font,
                            selected ? p.text : muted,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        RECT header_text = inset_rect(header, gap);
        nfui::draw_text(target,
                        header_text,
                        L"Preview-first shell",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        header_text.top += dpi_.logical_to_pixels(30);
        nfui::draw_text(target,
                        header_text,
                        L"Native status bar, dark chrome, and a central canvas keep this demo anchored in HWND-friendly patterns.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT preview_title = inset_rect(preview, gap);
        nfui::draw_text(target,
                        preview_title,
                        L"Preview canvas",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT canvas = make_rect(preview.left + gap,
                                preview.top + dpi_.logical_to_pixels(56),
                                rect_width(preview) - gap * 2,
                                preview_height - dpi_.logical_to_pixels(76));
        nfui::fill_rounded_rect(target, canvas, card_radius, canvas_fill, canvas_border);

        RECT strip = make_rect(canvas.left + dpi_.logical_to_pixels(18),
                               canvas.top + dpi_.logical_to_pixels(18),
                               rect_width(canvas) - dpi_.logical_to_pixels(36),
                               dpi_.logical_to_pixels(46));
        nfui::fill_rounded_rect(target, strip, pill_radius, surface_alt, p.border);
        RECT strip_text = inset_rect(strip, dpi_.logical_to_pixels(10));
        nfui::draw_text(target,
                        strip_text,
                        L"Live preview • navigation updates the shell without leaving native Win32 controls behind.",
                        body_font,
                        body,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT canvas_caption = make_rect(canvas.left + dpi_.logical_to_pixels(18),
                                        strip.bottom + dpi_.logical_to_pixels(18),
                                        rect_width(canvas) - dpi_.logical_to_pixels(36),
                                        dpi_.logical_to_pixels(100));
        nfui::draw_text(target,
                        canvas_caption,
                        L"DarkStudio is intentionally self-contained. It proves that product-level dark shells can live in a sample executable without forcing framework consumers to adopt new core rendering APIs.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        RECT metric_text = inset_rect(metric_one, gap);
        nfui::draw_text(target,
                        metric_text,
                        L"Native shell",
                        body_font,
                        muted,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(28);
        nfui::draw_text(target,
                        metric_text,
                        L"100%",
                        metric_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(42);
        nfui::draw_text(target,
                        metric_text,
                        L"Sample-local painting with a standard status bar.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        metric_text = inset_rect(metric_two, gap);
        nfui::draw_text(target,
                        metric_text,
                        L"DPI layout",
                        body_font,
                        muted,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(28);
        nfui::draw_text(target,
                        metric_text,
                        L"Per-monitor",
                        metric_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        metric_text.top += dpi_.logical_to_pixels(42);
        nfui::draw_text(target,
                        metric_text,
                        L"All shell measurements scale through nfui::DpiScale.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // FontCache owns the HFONT handles; they are released by fonts_' destructor.
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
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
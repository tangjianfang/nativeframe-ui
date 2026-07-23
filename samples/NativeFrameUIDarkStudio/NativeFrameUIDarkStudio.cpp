#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <cstdio>
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

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
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
            apply_native_fonts();
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
        status_.inject_theme(&palette_, &fonts_);
        if (!status_.create(params)) {
            return false;
        }
        apply_native_fonts();
        return true;
    }

    void apply_native_fonts() noexcept {
        if (status_.hwnd() == nullptr || fonts() == nullptr) {
            return;
        }
        SendMessageW(status_.hwnd(),
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts()->regular(dpi_.dpi(), 9)),
                     TRUE);
    }

    [[nodiscard]] nfui::FontCache* fonts() noexcept {
        return &fonts_;
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
        // CP23: navigation items must span the full rail width (rail_width)
        // minus the symmetric outer padding on both sides. The prior
        // expression used `nav_width - outer` which assumed a 210 logical
        // rail and then shrank the inner content by `outer` on only one
        // side, leaving a 30 logical px strip on the right where the rail
        // fill showed but no nav row covered. The rail is `rail_width`
        // logical px wide and inset by `outer` on both sides, so the nav
        // row width is `rail_width - outer * 2`.
        const int rail_width = dpi_.logical_to_pixels(220);
        const int nav_height = dpi_.logical_to_pixels(44);
        const int nav_gap = dpi_.logical_to_pixels(10);
        const int top = content.top + dpi_.logical_to_pixels(120) + static_cast<int>(index) * (nav_height + nav_gap);
        return make_rect(content.left + outer, top, rail_width - outer * 2, nav_height);
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

    void paint_shell(HDC hdc) noexcept {
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

        HFONT title_font = fonts()->semibold(dpi_value, 20);
        HFONT section_font = fonts()->semibold(dpi_value, 12);
        HFONT label_font = fonts()->bold(dpi_value, 12);
        HFONT body_font = fonts()->regular(dpi_value, 9);
        HFONT value_font = fonts()->mono(dpi_value, 28);

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

        const int indicator = dpi_.logical_to_pixels(2);
        const int icon_pad = dpi_.logical_to_pixels(8);
        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT item = navigation_rect(index);
            const bool selected = selected_navigation_ == static_cast<int>(index);
            if (selected) {
                nfui::fill_rounded_rect(target, item, pill_radius, p.selection, p.border);
                RECT accent = make_rect(item.left + indicator,
                                        item.top + dpi_.logical_to_pixels(10),
                                        indicator,
                                        rect_height(item) - dpi_.logical_to_pixels(20));
                nfui::fill_rounded_rect(target, accent, indicator / 2, p.accent_hover, p.accent_hover);
            }
            RECT label = inset_rect(item, dpi_.logical_to_pixels(12));
            label.left += icon_pad;
            nfui::draw_text(target,
                            label,
                            navigation_items[index],
                            selected ? label_font : section_font,
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

        // Colour-token swatch grid: small dots with labels instead of a giant
        // accent slab, so the preview area stays informative and subtle.
        struct Swatch { const wchar_t* name; nfui::Color color; };
        const std::array<Swatch, 5> swatches{{
            {L"background", p.background},
            {L"surface", p.surface},
            {L"accent", p.accent},
            {L"success", p.success},
            {L"info", p.info},
        }};

        const int swatch = dpi_.logical_to_pixels(16);
        const int swatch_col = dpi_.logical_to_pixels(96);
        const int swatch_row = dpi_.logical_to_pixels(30);
        const int swatch_left = preview.left + gap;
        const int swatch_top = preview.top + dpi_.logical_to_pixels(56);
        constexpr int swatch_cols = 2;

        for (std::size_t i = 0; i < swatches.size(); ++i) {
            const int col = static_cast<int>(i) % swatch_cols;
            const int row = static_cast<int>(i) / swatch_cols;
            const int x = swatch_left + col * swatch_col;
            const int y = swatch_top + row * swatch_row;
            RECT dot = make_rect(x, y, swatch, swatch);
            nfui::fill_ellipse(target, dot, swatches[i].color);
            nfui::draw_ellipse(target, dot, p.border, 1);
            RECT label = make_rect(x + swatch + dpi_.logical_to_pixels(8),
                                   y,
                                   swatch_col - swatch - dpi_.logical_to_pixels(8),
                                   swatch);
            nfui::draw_text(target,
                            label,
                            swatches[i].name,
                            body_font,
                            body,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Subtle code-block hint reinforces the canvas metaphor without
        // competing with the token grid.
        RECT hint = make_rect(preview.left + gap,
                              preview.bottom - dpi_.logical_to_pixels(44),
                              rect_width(preview) - gap * 2,
                              dpi_.logical_to_pixels(24));
        nfui::draw_text(target,
                        hint,
                        L"// preview canvas",
                        fonts()->mono(dpi_value, 9),
                        muted,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Metric card 1: Native shell.
        RECT metric_text = inset_rect(metric_one, gap);
        RECT dot = make_rect(metric_text.left,
                             metric_text.top + dpi_.logical_to_pixels(6),
                             dpi_.logical_to_pixels(8),
                             dpi_.logical_to_pixels(8));
        nfui::fill_ellipse(target, dot, p.success);
        RECT label = make_rect(dot.right + dpi_.logical_to_pixels(8),
                               metric_text.top,
                               rect_width(metric_text) - dpi_.logical_to_pixels(16),
                               dpi_.logical_to_pixels(20));
        nfui::draw_text(target,
                        label,
                        L"NATIVE SHELL",
                        section_font,
                        muted,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        RECT value = make_rect(metric_text.left,
                               metric_text.top + dpi_.logical_to_pixels(26),
                               rect_width(metric_text),
                               dpi_.logical_to_pixels(40));
        nfui::draw_text(target,
                        value,
                        L"100%",
                        value_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        RECT helper = make_rect(metric_text.left,
                                value.bottom + dpi_.logical_to_pixels(6),
                                rect_width(metric_text),
                                dpi_.logical_to_pixels(40));
        nfui::draw_text(target,
                        helper,
                        L"Sample-local painting with a standard status bar.",
                        body_font,
                        muted,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // Metric card 2: Per-monitor DPI layout.
        metric_text = inset_rect(metric_two, gap);
        dot = make_rect(metric_text.left,
                        metric_text.top + dpi_.logical_to_pixels(6),
                        dpi_.logical_to_pixels(8),
                        dpi_.logical_to_pixels(8));
        nfui::fill_ellipse(target, dot, p.info);
        label = make_rect(dot.right + dpi_.logical_to_pixels(8),
                          metric_text.top,
                          rect_width(metric_text) - dpi_.logical_to_pixels(16),
                          dpi_.logical_to_pixels(20));
        nfui::draw_text(target,
                        label,
                        L"DPI LAYOUT",
                        section_font,
                        muted,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        value = make_rect(metric_text.left,
                          metric_text.top + dpi_.logical_to_pixels(26),
                          rect_width(metric_text),
                          dpi_.logical_to_pixels(40));
        wchar_t dpi_str[32]{};
        swprintf_s(dpi_str, L"%d DPI", dpi_value);
        nfui::draw_text(target,
                        value,
                        dpi_str,
                        value_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        helper = make_rect(metric_text.left,
                           value.bottom + dpi_.logical_to_pixels(6),
                           rect_width(metric_text),
                           dpi_.logical_to_pixels(40));
        nfui::draw_text(target,
                        helper,
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
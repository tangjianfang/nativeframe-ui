#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
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

[[nodiscard]] int measure_text_width(HDC dc, HFONT font, std::wstring_view text) noexcept {
    if (dc == nullptr || font == nullptr || text.empty()) {
        return 0;
    }
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
    if (old != nullptr && old != HGDI_ERROR) {
        SelectObject(dc, old);
    }
    return size.cx;
}

constexpr std::array<std::wstring_view, 4> navigation_items{
    L"Surface",
    L"Canvas",
    L"Tokens",
    L"Publish",
};

enum class NavIcon {
    surface,
    canvas,
    tokens,
    publish,
};

[[nodiscard]] POINT map_icon_point(const RECT& bounds, int gx, int gy) noexcept {
    const int square = (std::min)(rect_width(bounds), rect_height(bounds));
    const int offset_x = bounds.left + (rect_width(bounds) - square) / 2;
    const int offset_y = bounds.top + (rect_height(bounds) - square) / 2;
    POINT pt{};
    pt.x = offset_x + gx * square / 16;
    pt.y = offset_y + gy * square / 16;
    return pt;
}

void draw_icon_polyline(HDC dc, const RECT& bounds,
                        const POINT* grid_points, int count,
                        nfui::Color color, int stroke_width) noexcept {
    if (dc == nullptr || grid_points == nullptr || count < 2) {
        return;
    }
    std::vector<POINT> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        points.push_back(map_icon_point(bounds, grid_points[i].x, grid_points[i].y));
    }
    nfui::draw_polyline(dc, points.data(), count, color, stroke_width);
}

void draw_nav_icon(HDC dc, const RECT& bounds, NavIcon kind,
                   nfui::Color color, int stroke_width) noexcept {
    switch (kind) {
    case NavIcon::surface: {
        // Three stacked diamond / layer outlines.
        constexpr POINT layer[] = {{8, 2}, {13, 5}, {8, 8}, {3, 5}, {8, 2}};
        constexpr POINT middle[] = {{8, 6}, {13, 9}, {8, 12}, {3, 9}, {8, 6}};
        constexpr POINT bottom[] = {{8, 10}, {13, 13}, {8, 16}, {3, 13}, {8, 10}};
        draw_icon_polyline(dc, bounds, layer, static_cast<int>(std::size(layer)), color, stroke_width);
        draw_icon_polyline(dc, bounds, middle, static_cast<int>(std::size(middle)), color, stroke_width);
        draw_icon_polyline(dc, bounds, bottom, static_cast<int>(std::size(bottom)), color, stroke_width);
        break;
    }
    case NavIcon::canvas: {
        // Paint brush: diamond bristles + diagonal handle.
        constexpr POINT bristles[] = {{5, 3}, {8, 6}, {5, 9}, {2, 6}, {5, 3}};
        constexpr POINT handle[] = {{6, 7}, {14, 14}};
        draw_icon_polyline(dc, bounds, bristles, static_cast<int>(std::size(bristles)), color, stroke_width);
        draw_icon_polyline(dc, bounds, handle, static_cast<int>(std::size(handle)), color, stroke_width);
        break;
    }
    case NavIcon::tokens: {
        // Code-ish "</>" glyph.
        constexpr POINT left_angle[] = {{4, 4}, {1, 8}, {4, 12}};
        constexpr POINT slash[] = {{7, 2}, {10, 14}};
        constexpr POINT right_angle[] = {{12, 4}, {15, 8}, {12, 12}};
        draw_icon_polyline(dc, bounds, left_angle, static_cast<int>(std::size(left_angle)), color, stroke_width);
        draw_icon_polyline(dc, bounds, slash, static_cast<int>(std::size(slash)), color, stroke_width);
        draw_icon_polyline(dc, bounds, right_angle, static_cast<int>(std::size(right_angle)), color, stroke_width);
        break;
    }
    case NavIcon::publish: {
        // Upload arrow rising from a tray.
        constexpr POINT shaft[] = {{8, 11}, {8, 2}};
        constexpr POINT left_head[] = {{8, 2}, {4, 6}};
        constexpr POINT right_head[] = {{8, 2}, {12, 6}};
        constexpr POINT tray_bottom[] = {{2, 12}, {14, 12}};
        constexpr POINT tray_left[] = {{2, 12}, {2, 10}};
        constexpr POINT tray_right[] = {{14, 12}, {14, 10}};
        draw_icon_polyline(dc, bounds, shaft, static_cast<int>(std::size(shaft)), color, stroke_width);
        draw_icon_polyline(dc, bounds, left_head, static_cast<int>(std::size(left_head)), color, stroke_width);
        draw_icon_polyline(dc, bounds, right_head, static_cast<int>(std::size(right_head)), color, stroke_width);
        draw_icon_polyline(dc, bounds, tray_bottom, static_cast<int>(std::size(tray_bottom)), color, stroke_width);
        draw_icon_polyline(dc, bounds, tray_left, static_cast<int>(std::size(tray_left)), color, stroke_width);
        draw_icon_polyline(dc, bounds, tray_right, static_cast<int>(std::size(tray_right)), color, stroke_width);
        break;
    }
    default:
        break;
    }
}

struct CodeToken {
    std::wstring_view text;
    nfui::Color color;
};

// CP32: thin wrapper around nfui::StatusBar so the sample's owner-draw dot
// subclass can reach the protected chrome paint helper without modifying
// framework headers.
class DarkStudioStatusBar final : public nfui::StatusBar {
public:
    void paint_chrome_public(HDC dc) noexcept {
        paint_chrome(dc);
    }
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
        if (brand_font_ != nullptr) {
            DeleteObject(brand_font_);
            brand_font_ = nullptr;
        }
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
        apply_brand_font(dpi_.dpi());
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
            apply_brand_font(dpi_.dpi());
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
    static LRESULT CALLBACK StatusBarDotProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam,
                                              UINT_PTR subclass_id,
                                              DWORD_PTR ref_data) noexcept {
        auto* self = reinterpret_cast<DarkStudioWindow*>(ref_data);
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            if (dc != nullptr && self != nullptr) {
                // StatusBar already has an internal chrome subclass that paints
                // itself on WM_PAINT. We only draw the ready-dot overlay on top.
                self->paint_status_overlay(dc);
                EndPaint(hwnd, &paint);
            }
            return 0;
        }
        if (message == WM_ERASEBKGND) {
            return 1;
        }
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(hwnd, &DarkStudioWindow::StatusBarDotProc, subclass_id);
        }
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

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
        status_.inject_theme(&palette_, &fonts_);
        if (!status_.create(params)) {
            return false;
        }
        if (SetWindowSubclass(status_.hwnd(),
                              &DarkStudioWindow::StatusBarDotProc,
                              1,
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        apply_native_fonts();
        return true;
    }

    void apply_native_fonts() noexcept {
        if (status_.hwnd() == nullptr) {
            return;
        }
        SendMessageW(status_.hwnd(),
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.semibold(dpi_.dpi(), nfui::font_pt::ui)),
                     TRUE);
    }

    void apply_brand_font(int dpi) noexcept {
        if (brand_font_ != nullptr && brand_font_dpi_ == dpi) {
            return;
        }
        if (brand_font_ != nullptr) {
            DeleteObject(brand_font_);
            brand_font_ = nullptr;
        }
        const int px = nfui::font_pixel_height(nfui::font_pt::xl, dpi);
        if (px > 0) {
            brand_font_ = CreateFontW(-px,
                                      0,
                                      0,
                                      0,
                                      FW_NORMAL,
                                      TRUE,
                                      FALSE,
                                      FALSE,
                                      DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_ROMAN,
                                      L"Georgia");
        }
        brand_font_dpi_ = dpi;
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
        const int outer = dip(16);
        const int rail_width = dip(240);
        const int nav_height = dip(40);
        const int nav_gap = dip(8);
        const int brand_height = dip(136);
        const int top = content.top + outer + brand_height + static_cast<int>(index) * (nav_height + nav_gap);
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
            SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L""));
        }
    }

    void paint_status_overlay(HDC dc) {
        if (dc == nullptr || status_.hwnd() == nullptr) {
            return;
        }
        const nfui::ThemePalette& p = palette_;
        const nfui::DpiScale status_dpi{nfui::dpi_of(status_.hwnd())};
        RECT bounds{};
        GetClientRect(status_.hwnd(), &bounds);

        const int grip = status_dpi.logical_to_pixels(16);
        const int pad_left = status_dpi.logical_to_pixels(12);
        const int dot_size = (std::max)(status_dpi.logical_to_pixels(8), 1);
        const int text_gap = status_dpi.logical_to_pixels(8);
        const int center_y = (bounds.top + bounds.bottom) / 2;

        RECT dot = make_rect(bounds.left + pad_left,
                             center_y - dot_size / 2,
                             dot_size,
                             dot_size);
        nfui::fill_ellipse(dc, dot, p.success);

        RECT text_rect = bounds;
        text_rect.left = dot.right + text_gap;
        text_rect.right -= grip;
        HFONT status_font = fonts_.semibold(status_dpi.dpi(), nfui::font_pt::sm);
        nfui::draw_text(dc,
                        text_rect,
                        L"Ready",
                        status_font,
                        p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_metric_card(HDC dc,
                           const RECT& card,
                           std::wstring_view label,
                           std::wstring_view value,
                           std::wstring_view description) {
        const nfui::ThemePalette& p = palette_;
        const int gap = dip(16);
        const int line_height = nfui::font_pixel_height(nfui::font_pt::lg, dpi_.dpi());

        RECT text = inset_rect(card, gap);

        HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        HFONT value_font = fonts_.bold(dpi_.dpi(), nfui::font_pt::lg);
        HFONT desc_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);

        const int label_width = measure_text_width(dc, label_font, label);
        const int value_width = measure_text_width(dc, value_font, value);

        RECT label_rect = text;
        label_rect.right = label_rect.left + label_width;
        nfui::draw_text(dc,
                        label_rect,
                        label,
                        label_font,
                        p.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT value_rect = text;
        value_rect.left = label_rect.right;
        value_rect.right = value_rect.left + value_width;
        nfui::draw_text(dc,
                        value_rect,
                        value,
                        value_font,
                        p.accent,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        text.top += line_height + dip(8);
        nfui::draw_text(dc,
                        text,
                        description,
                        desc_font,
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    void draw_code_block(HDC dc, const RECT& bounds) {
        const nfui::ThemePalette& p = palette_;
        const int dpi_value = dpi_.dpi();
        const int line_height = nfui::font_pixel_height(nfui::font_pt::xs, dpi_value) + dip(2);
        const int gutter = dip(28);
        const int pad = dip(8);

        static const std::vector<std::vector<CodeToken>> code_lines = {
            {{L"// NativeFrame UI DarkStudio Sample", p.text_secondary}},
            {{L"import ", p.accent}, {L"{ fui }", p.text}, {L" from ", p.accent}, {L"'native-frame-ui';", p.success}},
            {},
            {{L"function ", p.accent}, {L"renderPreview", p.text}, {L"() {", p.text_secondary}},
            {{L"    ", p.text_secondary}, {L"const ", p.accent}, {L"scale", p.text}, {L" = ", p.text_secondary}, {L"fui", p.accent}, {L".DpiScale;", p.text_secondary}},
            {{L"    ", p.text_secondary}, {L"console.log(", p.text}, {L"'Rendering preview at scale:', ", p.success}, {L"scale", p.text}, {L");", p.text_secondary}},
            {{L"    ", p.text_secondary}, {L"// ... more code ...", p.text_secondary}},
            {{L"}", p.text_secondary}},
        };

        HFONT code_font = fonts_.mono(dpi_value, nfui::font_pt::xs);

        int save = SaveDC(dc);
        IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);

        int y = bounds.top + pad;
        int line_index = 1;
        for (const auto& tokens : code_lines) {
            // Line number gutter.
            RECT num_rect = make_rect(bounds.left + pad, y, gutter - pad, line_height);
            std::wstring number = std::to_wstring(line_index);
            nfui::draw_text(dc,
                            num_rect,
                            number,
                            code_font,
                            p.text_secondary,
                            DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            int x = bounds.left + gutter;
            for (const auto& token : tokens) {
                const int token_width = measure_text_width(dc, code_font, token.text);
                RECT token_rect = make_rect(x, y, token_width, line_height);
                nfui::draw_text(dc,
                                token_rect,
                                token.text,
                                code_font,
                                token.color,
                                DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
                x += token_width;
            }

            y += line_height;
            ++line_index;
        }

        RestoreDC(dc, save);
    }

    void paint_shell(HDC hdc) {
        const nfui::ThemePalette& p = palette_;
        const RECT content = content_rect();
        const int outer = dip(16);
        const int gap = dip(16);
        const int rail_width = dip(240);
        const int preview_width = (std::max)(rect_width(content) - rail_width - outer * 2 - gap, dip(520));
        const int preview_height = dip(384);
        const int card_height = dip(128);
        const int card_radius = dip(nfui::theme_metrics().corner_radius_card);
        const int pill_radius = dip(nfui::theme_metrics().corner_radius_control);
        const int stroke = (std::max)(dpi_.logical_to_pixels(1), 1);

        nfui::MemoryDC mem(hdc, content);
        HDC target = mem.valid() ? mem.dc() : hdc;

        const int dpi_value = dpi_.dpi();
        const nfui::Color rail_fill = nfui::darken(p.background, 0.18f);
        const nfui::Color canvas_fill = p.background;

        nfui::fill_rect(target, content, p.background);

        RECT rail = make_rect(content.left + outer,
                              content.top + outer,
                              rail_width,
                              rect_height(content) - outer * 2);
        RECT header = make_rect(rail.right + gap,
                                content.top + outer,
                                preview_width,
                                dip(120));
        RECT preview = make_rect(header.left,
                                 header.bottom + gap,
                                 preview_width,
                                 preview_height);
        RECT metric_one = make_rect(preview.left,
                                    preview.bottom + gap,
                                    (preview_width - gap) / 2,
                                    card_height);
        RECT metric_two = make_rect(metric_one.right + gap,
                                    metric_one.top,
                                    rect_width(metric_one),
                                    card_height);

        nfui::fill_rounded_rect(target, rail, card_radius, rail_fill, p.border);
        nfui::fill_rounded_rect(target, header, card_radius, p.surface, p.border);
        nfui::fill_rounded_rect(target, preview, card_radius, p.surface, p.border);
        nfui::fill_rounded_rect(target, metric_one, card_radius, p.surface, p.border);
        nfui::fill_rounded_rect(target, metric_two, card_radius, p.surface, p.border);

        HFONT brand_font = brand_font_;
        HFONT body_font = fonts_.regular(dpi_value, nfui::font_pt::sm);
        HFONT section_font = fonts_.semibold(dpi_value, nfui::font_pt::lg);

        // Brand block.
        RECT brand = make_rect(rail.left + gap,
                               rail.top + gap,
                               rect_width(rail) - gap * 2,
                               dip(44));
        nfui::draw_text(target,
                        brand,
                        L"DarkStudio",
                        brand_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        brand.top += dip(44);
        brand.bottom = rail.top + dip(120);
        nfui::draw_text(target,
                        brand,
                        L"A focused dark-shell sample for preview-heavy desktop tools.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        // Navigation rows.
        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT item = navigation_rect(index);
            const bool selected = selected_navigation_ == static_cast<int>(index);
            if (selected) {
                nfui::fill_rounded_rect(target, item, pill_radius, p.selection, p.selection);
                RECT accent_bar = make_rect(item.left + dip(4),
                                            item.top + dip(10),
                                            (std::max)(dip(3), 1),
                                            rect_height(item) - dip(20));
                nfui::fill_rect(target, accent_bar, p.accent);
            }

            RECT icon_bounds = make_rect(item.left + dip(12),
                                         item.top + (rect_height(item) - dip(20)) / 2,
                                         dip(20),
                                         dip(20));
            draw_nav_icon(target,
                          icon_bounds,
                          static_cast<NavIcon>(index),
                          selected ? p.text : p.text_secondary,
                          stroke);

            RECT label = item;
            label.left = icon_bounds.right + dip(12);
            label.right -= dip(12);
            nfui::draw_text(target,
                            label,
                            navigation_items[index],
                            section_font,
                            selected ? p.text : p.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Header card.
        RECT header_text = inset_rect(header, gap);
        nfui::draw_text(target,
                        header_text,
                        L"Preview-first shell",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        header_text.top += dip(32);
        nfui::draw_text(target,
                        header_text,
                        L"A minimal shell designed to prioritize the content preview experience with non-intrusive UI controls and maximum screen real estate for canvas elements.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        // Preview canvas.
        RECT preview_title = inset_rect(preview, gap);
        nfui::draw_text(target,
                        preview_title,
                        L"Preview canvas",
                        section_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT code_panel = make_rect(preview.left + gap,
                                    preview.top + dip(48),
                                    rect_width(preview) - gap * 2,
                                    preview.bottom - preview.top - dip(64));
        nfui::fill_rounded_rect(target, code_panel, card_radius, canvas_fill, p.border);
        draw_code_block(target, inset_rect(code_panel, dip(8)));

        // Stat cards.
        paint_metric_card(target,
                          metric_one,
                          L"Native shell - ",
                          L"100%",
                          L"Sample-local painting with a standard status bar.");
        paint_metric_card(target,
                          metric_two,
                          L"DPI layout - ",
                          L"Per-monitor",
                          L"All shell measurements scale through nfui::DpiScale.");
    }

    [[nodiscard]] int dip(int logical) const noexcept {
        return dpi_.logical_to_pixels(logical);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    DarkStudioStatusBar status_;
    nfui::DpiScale dpi_{96};
    RECT client_rect_{};
    int selected_navigation_{};
    HICON large_icon_{};
    HICON small_icon_{};
    HFONT brand_font_{};
    int brand_font_dpi_{};
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

#include "ShowcaseView.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

struct ShowcasePalette {
    COLORREF window{};
    COLORREF sidebar{};
    COLORREF command_bar{};
    COLORREF surface{};
    COLORREF surface_alt{};
    COLORREF border{};
    COLORREF accent{};
    COLORREF accent_soft{};
    COLORREF text{};
    COLORREF muted_text{};
    COLORREF success{};
    COLORREF warning{};
};

struct ShowcaseLayout {
    RECT sidebar{};
    RECT command_bar{};
    RECT workspace{};
    RECT inspector{};
    RECT theme_toggle{};
    RECT divider_left{};
    RECT divider_right{};
    std::array<RECT, 4> navigation{};
    std::array<RECT, 3> cards{};
    std::array<RECT, 3> inspector_rows{};
};

constexpr std::array<std::wstring_view, 4> navigation_labels{
    L"Overview",
    L"Pipelines",
    L"Reviews",
    L"Resources",
};

constexpr std::array<std::wstring_view, 3> card_titles{
    L"Adoption",
    L"Build Health",
    L"Resource Flow",
};

constexpr std::array<std::wstring_view, 3> card_values{
    L"+18%",
    L"98.6%",
    L"24 assets",
};

constexpr std::array<std::wstring_view, 3> card_badges{
    L"Stable",
    L"Release Ready",
    L"Explicit",
};

constexpr std::array<std::wstring_view, 3> inspector_labels{
    L"Framework boundary",
    L"DPI path",
    L"Resource story",
};

constexpr std::array<std::wstring_view, 3> inspector_values{
    L"Showcase painting stays sample-local and never widens the stable core API.",
    L"Logical metrics scale through nfui::DpiScale and respond to WM_DPICHANGED.",
    L"Every sample still links explicit framework resources through nfui_add_resources.",
};

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
    int percent_from = 100 - percent_to;
    return RGB(
        (GetRValue(from) * percent_from + GetRValue(to) * percent_to) / 100,
        (GetGValue(from) * percent_from + GetGValue(to) * percent_to) / 100,
        (GetBValue(from) * percent_from + GetBValue(to) * percent_to) / 100);
}

[[nodiscard]] ShowcasePalette palette_for(nfui::ThemeMode mode) noexcept {
    const nfui::ThemeTokens tokens = nfui::theme_tokens(mode);
    const bool dark = mode == nfui::ThemeMode::dark;

    ShowcasePalette palette{};
    palette.window = tokens.window_background;
    palette.sidebar = dark ? blend(tokens.window_background, RGB(0, 0, 0), 22)
                           : blend(tokens.window_background, RGB(238, 242, 249), 92);
    palette.command_bar = dark ? blend(tokens.window_background, RGB(62, 70, 90), 24)
                               : blend(tokens.window_background, RGB(246, 249, 255), 96);
    palette.surface = dark ? blend(tokens.window_background, RGB(47, 54, 66), 34)
                           : RGB(255, 255, 255);
    palette.surface_alt = dark ? blend(tokens.window_background, RGB(29, 34, 41), 16)
                               : blend(tokens.window_background, RGB(234, 240, 248), 82);
    palette.border = dark ? RGB(73, 80, 95) : RGB(214, 221, 232);
    palette.accent = tokens.accent;
    palette.accent_soft = dark ? blend(tokens.accent, tokens.window_background, 70)
                               : blend(tokens.accent, RGB(255, 255, 255), 78);
    palette.text = tokens.window_text;
    palette.muted_text = dark ? RGB(176, 184, 201) : RGB(95, 106, 126);
    palette.success = dark ? RGB(70, 201, 144) : RGB(24, 148, 103);
    palette.warning = dark ? RGB(255, 189, 82) : RGB(189, 123, 24);
    return palette;
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
    RECT text_rect = rect;
    DrawTextW(hdc, text.data(), static_cast<int>(text.size()), &text_rect, format);
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

[[nodiscard]] ShowcaseLayout build_layout(const RECT& client_rect, const nfui::DpiScale& dpi) noexcept {
    ShowcaseLayout layout{};
    const int outer = dpi.logical_to_pixels(20);
    const int gap = dpi.logical_to_pixels(16);
    const int sidebar_width = dpi.logical_to_pixels(220);
    const int inspector_width = dpi.logical_to_pixels(300);
    const int command_height = dpi.logical_to_pixels(88);
    const int nav_height = dpi.logical_to_pixels(42);
    const int nav_gap = dpi.logical_to_pixels(10);
    const int card_height = dpi.logical_to_pixels(150);
    const int inspector_row_height = dpi.logical_to_pixels(102);

    const int client_width = rect_width(client_rect);
    const int client_height = rect_height(client_rect);
    const int content_top = client_rect.top + outer;
    const int content_bottom = client_rect.bottom - outer;

    layout.sidebar = make_rect(client_rect.left + outer,
                               content_top,
                               sidebar_width,
                               std::max(client_height - outer * 2, 0));

    const int workspace_left = layout.sidebar.right + gap;
    const int workspace_width = std::max(client_width - sidebar_width - inspector_width - outer * 2 - gap * 2, dpi.logical_to_pixels(480));
    layout.command_bar = make_rect(workspace_left, content_top, workspace_width, command_height);
    layout.workspace = make_rect(workspace_left,
                                 layout.command_bar.bottom + gap,
                                 workspace_width,
                                 std::max(static_cast<int>(content_bottom - layout.command_bar.bottom - gap), 0));
    layout.inspector = make_rect(layout.workspace.right + gap,
                                 content_top,
                                 inspector_width,
                                 std::max(client_height - outer * 2, 0));

    layout.divider_left = make_rect(layout.sidebar.right + gap / 2, content_top, 1, layout.sidebar.bottom - content_top);
    layout.divider_right = make_rect(layout.workspace.right + gap / 2, content_top, 1, layout.sidebar.bottom - content_top);

    const int nav_left = layout.sidebar.left + gap;
    const int nav_width = sidebar_width - gap * 2;
    const int nav_top_start = layout.sidebar.top + dpi.logical_to_pixels(100);
    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const int y = nav_top_start + static_cast<int>(index) * (nav_height + nav_gap);
        layout.navigation[index] = make_rect(nav_left, y, nav_width, nav_height);
    }

    const int toggle_width = dpi.logical_to_pixels(170);
    const int toggle_height = dpi.logical_to_pixels(42);
    layout.theme_toggle = make_rect(layout.command_bar.right - toggle_width - gap,
                                    layout.command_bar.top + (command_height - toggle_height) / 2,
                                    toggle_width,
                                    toggle_height);

    const int card_gap = gap;
    const int total_card_gap = card_gap * 2;
    const int card_width = std::max((workspace_width - total_card_gap) / 3, dpi.logical_to_pixels(160));
    const int cards_top = layout.workspace.top;
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const int x = layout.workspace.left + static_cast<int>(index) * (card_width + card_gap);
        layout.cards[index] = make_rect(x, cards_top, card_width, card_height);
    }

    const int inspector_row_top = layout.inspector.top + dpi.logical_to_pixels(118);
    const int inspector_row_width = inspector_width - gap * 2;
    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        const int y = inspector_row_top + static_cast<int>(index) * (inspector_row_height + gap);
        layout.inspector_rows[index] = make_rect(layout.inspector.left + gap,
                                                 y,
                                                 inspector_row_width,
                                                 inspector_row_height);
    }

    return layout;
}

} // namespace

void ShowcaseView::set_theme_mode(nfui::ThemeMode mode) noexcept {
    if (mode == nfui::ThemeMode::high_contrast || mode == nfui::ThemeMode::system) {
        theme_mode_ = nfui::ThemeMode::light;
        return;
    }
    theme_mode_ = mode;
}

nfui::ThemeMode ShowcaseView::theme_mode() const noexcept {
    return theme_mode_;
}

void ShowcaseView::toggle_theme() noexcept {
    theme_mode_ = theme_mode_ == nfui::ThemeMode::dark ? nfui::ThemeMode::light : nfui::ThemeMode::dark;
}

void ShowcaseView::set_dpi(int dpi) noexcept {
    dpi_scale_ = nfui::DpiScale(dpi);
}

int ShowcaseView::dpi() const noexcept {
    return dpi_scale_.dpi();
}

void ShowcaseView::set_client_rect(const RECT& rect) noexcept {
    client_rect_ = rect;
}

int ShowcaseView::hovered_card() const noexcept {
    return hovered_card_;
}

int ShowcaseView::selected_navigation() const noexcept {
    return selected_navigation_;
}

bool ShowcaseView::on_mouse_move(POINT point) noexcept {
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    int hovered = -1;
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        if (PtInRect(&layout.cards[index], point) != FALSE) {
            hovered = static_cast<int>(index);
            break;
        }
    }

    if (hovered == hovered_card_) {
        return false;
    }

    hovered_card_ = hovered;
    return true;
}

bool ShowcaseView::on_left_button_down(POINT point) noexcept {
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    if (PtInRect(&layout.theme_toggle, point) != FALSE) {
        toggle_theme();
        return true;
    }

    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        if (PtInRect(&layout.navigation[index], point) != FALSE) {
            if (selected_navigation_ != static_cast<int>(index)) {
                selected_navigation_ = static_cast<int>(index);
                return true;
            }
            return false;
        }
    }

    return on_mouse_move(point);
}

bool ShowcaseView::clear_hover() noexcept {
    if (hovered_card_ < 0) {
        return false;
    }
    hovered_card_ = -1;
    return true;
}

void ShowcaseView::paint(HDC hdc) const {
    const ShowcasePalette palette = palette_for(theme_mode_);
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    const int radius = dpi_scale_.logical_to_pixels(18);
    const int small_radius = dpi_scale_.logical_to_pixels(12);
    const int gap = dpi_scale_.logical_to_pixels(16);

    fill_rect_color(hdc, client_rect_, palette.window);
    fill_rect_color(hdc, layout.sidebar, palette.sidebar);
    draw_round_panel(hdc, layout.command_bar, palette.command_bar, palette.border, radius);
    draw_round_panel(hdc, layout.inspector, palette.surface_alt, palette.border, radius);
    fill_rect_color(hdc, layout.divider_left, palette.border);
    fill_rect_color(hdc, layout.divider_right, palette.border);

    HFONT title_font = create_font(dpi_scale_, -28, FW_BOLD);
    HFONT section_font = create_font(dpi_scale_, -18, FW_SEMIBOLD);
    HFONT body_font = create_font(dpi_scale_, -14, FW_NORMAL);
    HFONT metric_font = create_font(dpi_scale_, -26, FW_BOLD);
    HFONT badge_font = create_font(dpi_scale_, -13, FW_SEMIBOLD);

    const RECT brand_rect = make_rect(layout.sidebar.left + gap,
                                      layout.sidebar.top + gap,
                                      rect_width(layout.sidebar) - gap * 2,
                                      dpi_scale_.logical_to_pixels(64));
    draw_text_block(hdc,
                    brand_rect,
                    L"NativeFrame UI",
                    palette.text,
                    title_font,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    RECT tagline_rect = brand_rect;
    tagline_rect.top += dpi_scale_.logical_to_pixels(38);
    draw_text_block(hdc,
                    tagline_rect,
                    L"Modern Win32 showcase with stable HWND-centered boundaries.",
                    palette.muted_text,
                    body_font,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const bool selected = selected_navigation_ == static_cast<int>(index);
        const COLORREF fill = selected ? palette.accent_soft : palette.sidebar;
        const COLORREF border = selected ? palette.accent : palette.sidebar;
        if (selected) {
            draw_round_panel(hdc, layout.navigation[index], fill, border, small_radius);
        }

        RECT label_rect = inset_rect(layout.navigation[index], dpi_scale_.logical_to_pixels(12));
        draw_text_block(hdc,
                        label_rect,
                        navigation_labels[index],
                        selected ? palette.text : palette.muted_text,
                        section_font,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT command_title = inset_rect(layout.command_bar, gap);
    draw_text_block(hdc,
                    command_title,
                    L"Product Growth Showcase",
                    palette.text,
                    section_font,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    command_title.top += dpi_scale_.logical_to_pixels(34);
    draw_text_block(hdc,
                    command_title,
                    L"Focused on evaluation-ready light and dark shells, explicit resources, and DPI-aware composition.",
                    palette.muted_text,
                    body_font,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    draw_round_panel(hdc, layout.theme_toggle, palette.surface, palette.border, small_radius);
    RECT toggle_label = inset_rect(layout.theme_toggle, dpi_scale_.logical_to_pixels(10));
    const std::wstring_view toggle_text =
        theme_mode_ == nfui::ThemeMode::dark ? L"Switch to light" : L"Switch to dark";
    draw_text_block(hdc,
                    toggle_label,
                    toggle_text,
                    palette.text,
                    badge_font,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const bool hovered = hovered_card_ == static_cast<int>(index);
        const COLORREF fill = hovered ? blend(palette.surface, palette.accent_soft, 22) : palette.surface;
        const COLORREF border = hovered ? palette.accent : palette.border;
        draw_round_panel(hdc, layout.cards[index], fill, border, radius);

        RECT card_text = inset_rect(layout.cards[index], gap);
        draw_text_block(hdc,
                        card_text,
                        card_titles[index],
                        palette.muted_text,
                        body_font,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        card_text.top += dpi_scale_.logical_to_pixels(28);
        draw_text_block(hdc,
                        card_text,
                        card_values[index],
                        palette.text,
                        metric_font,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT badge_rect = make_rect(layout.cards[index].left + gap,
                                    layout.cards[index].bottom - dpi_scale_.logical_to_pixels(54),
                                    dpi_scale_.logical_to_pixels(130),
                                    dpi_scale_.logical_to_pixels(30));
        const COLORREF badge_fill = index == 1 ? blend(palette.success, palette.window, 72)
                                               : (index == 2 ? blend(palette.warning, palette.window, 78)
                                                             : blend(palette.accent, palette.window, 82));
        const COLORREF badge_border = index == 1 ? palette.success : (index == 2 ? palette.warning : palette.accent);
        draw_round_panel(hdc, badge_rect, badge_fill, badge_border, small_radius);
        RECT badge_text = inset_rect(badge_rect, dpi_scale_.logical_to_pixels(8));
        draw_text_block(hdc,
                        badge_text,
                        card_badges[index],
                        palette.text,
                        badge_font,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT workspace_note = make_rect(layout.workspace.left,
                                    layout.cards[0].bottom + gap,
                                    rect_width(layout.workspace),
                                    dpi_scale_.logical_to_pixels(110));
    draw_round_panel(hdc, workspace_note, palette.surface_alt, palette.border, radius);
    RECT note_text = inset_rect(workspace_note, gap);
    draw_text_block(hdc,
                    note_text,
                    L"Showcase-only visuals: card styling, badges, command-bar treatment, and inspector composition live entirely in samples/NativeFrameUIShowcase. Stable framework guarantees remain HWND access, DPI helpers, resources, controls, and command routing.",
                    palette.text,
                    body_font,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    RECT inspector_header = make_rect(layout.inspector.left + gap,
                                      layout.inspector.top + gap,
                                      rect_width(layout.inspector) - gap * 2,
                                      dpi_scale_.logical_to_pixels(68));
    draw_text_block(hdc,
                    inspector_header,
                    L"Inspector",
                    palette.text,
                    section_font,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    inspector_header.top += dpi_scale_.logical_to_pixels(30);
    draw_text_block(hdc,
                    inspector_header,
                    L"Readable implementation notes for reviewers and release screenshots.",
                    palette.muted_text,
                    body_font,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        draw_round_panel(hdc, layout.inspector_rows[index], palette.surface, palette.border, small_radius);
        RECT row_text = inset_rect(layout.inspector_rows[index], dpi_scale_.logical_to_pixels(12));
        draw_text_block(hdc,
                        row_text,
                        inspector_labels[index],
                        palette.muted_text,
                        badge_font,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        row_text.top += dpi_scale_.logical_to_pixels(24);
        draw_text_block(hdc,
                        row_text,
                        inspector_values[index],
                        palette.text,
                        body_font,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }

    DeleteObject(badge_font);
    DeleteObject(metric_font);
    DeleteObject(body_font);
    DeleteObject(section_font);
    DeleteObject(title_font);
}

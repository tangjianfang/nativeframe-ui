#include "ShowcaseView.hpp"

#include <nfui/Paint.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace {

enum class FontWeight { regular, semibold };

struct ShowcasePalette {
    nfui::Color window;
    nfui::Color sidebar;
    nfui::Color command_bar;
    nfui::Color surface;
    nfui::Color surface_alt;
    nfui::Color border;
    nfui::Color accent;
    nfui::Color accent_soft;
    nfui::Color text;
    nfui::Color muted_text;
    nfui::Color success;
    nfui::Color warning;
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

[[nodiscard]] HFONT fetch_font(nfui::FontCache& fonts,
                               int dpi,
                               FontWeight weight,
                               int point_size) noexcept {
    return weight == FontWeight::semibold ? fonts.semibold(dpi, point_size)
                                          : fonts.regular(dpi, point_size);
}

[[nodiscard]] ShowcasePalette palette_for(nfui::ThemeMode mode) noexcept {
    const nfui::ThemePalette p = nfui::theme_palette(mode);
    const bool dark = mode == nfui::ThemeMode::dark;

    ShowcasePalette palette{};
    palette.window = p.background;
    palette.sidebar = dark ? nfui::darken(p.background, 0.22f)
                           : nfui::alpha_blend(p.border, p.surface, 0.35f);
    palette.command_bar = dark ? nfui::darken(p.background, 0.24f)
                               : nfui::alpha_blend(p.border, p.background, 0.06f);
    palette.surface = p.surface;
    palette.surface_alt = dark ? nfui::darken(p.background, 0.16f)
                               : nfui::alpha_blend(p.border, p.background, 0.10f);
    palette.border = p.border;
    palette.accent = p.accent;
    palette.accent_soft = nfui::alpha_blend(p.background, p.accent, dark ? 0.70f : 0.78f);
    palette.text = p.text;
    palette.muted_text = p.text_secondary;
    palette.success = p.success;
    palette.warning = p.warning;
    return palette;
}

void fill_rect_color(HDC hdc, const RECT& rect, nfui::Color color) noexcept {
    if (HBRUSH brush = CreateSolidBrush(color.rgb)) {
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }
}

void draw_text_block(HDC hdc,
                     const RECT& rect,
                     std::wstring_view text,
                     nfui::FontCache& fonts,
                     int dpi,
                     FontWeight weight,
                     int point_size,
                     nfui::Color color,
                     UINT format) noexcept {
    const HFONT font = fetch_font(fonts, dpi, weight, point_size);
    nfui::draw_text(hdc, rect, text, font, color, format);
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

void ShowcaseView::paint(HDC hdc, nfui::FontCache& fonts) const noexcept {
    const ShowcasePalette palette = palette_for(theme_mode_);
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    const int radius = dpi_scale_.logical_to_pixels(9);
    const int small_radius = dpi_scale_.logical_to_pixels(6);
    const int gap = dpi_scale_.logical_to_pixels(16);

    fill_rect_color(hdc, client_rect_, palette.window);
    fill_rect_color(hdc, layout.sidebar, palette.sidebar);
    nfui::fill_rounded_rect(hdc, layout.command_bar, radius, palette.command_bar, palette.border);
    nfui::fill_rounded_rect(hdc, layout.inspector, radius, palette.surface_alt, palette.border);
    fill_rect_color(hdc, layout.divider_left, palette.border);
    fill_rect_color(hdc, layout.divider_right, palette.border);

    const RECT brand_rect = make_rect(layout.sidebar.left + gap,
                                      layout.sidebar.top + gap,
                                      rect_width(layout.sidebar) - gap * 2,
                                      dpi_scale_.logical_to_pixels(64));
    draw_text_block(hdc,
                    brand_rect,
                    L"NativeFrame UI",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    21,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    RECT tagline_rect = brand_rect;
    tagline_rect.top += dpi_scale_.logical_to_pixels(38);
    draw_text_block(hdc,
                    tagline_rect,
                    L"Modern Win32 showcase with stable HWND-centered boundaries.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    11,
                    palette.muted_text,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const bool selected = selected_navigation_ == static_cast<int>(index);
        const nfui::Color fill = selected ? palette.accent_soft : palette.sidebar;
        const nfui::Color border = selected ? palette.accent : palette.sidebar;
        if (selected) {
            nfui::fill_rounded_rect(hdc, layout.navigation[index], small_radius, fill, border);
        }

        RECT label_rect = inset_rect(layout.navigation[index], dpi_scale_.logical_to_pixels(12));
        draw_text_block(hdc,
                        label_rect,
                        navigation_labels[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        14,
                        selected ? palette.text : palette.muted_text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT command_title = inset_rect(layout.command_bar, gap);
    draw_text_block(hdc,
                    command_title,
                    L"Product Growth Showcase",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    14,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    command_title.top += dpi_scale_.logical_to_pixels(34);
    draw_text_block(hdc,
                    command_title,
                    L"Focused on evaluation-ready light and dark shells, explicit resources, and DPI-aware composition.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    11,
                    palette.muted_text,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    nfui::fill_rounded_rect(hdc, layout.theme_toggle, small_radius, palette.surface, palette.border);
    RECT toggle_label = inset_rect(layout.theme_toggle, dpi_scale_.logical_to_pixels(10));
    const std::wstring_view toggle_text =
        theme_mode_ == nfui::ThemeMode::dark ? L"Switch to light" : L"Switch to dark";
    draw_text_block(hdc,
                    toggle_label,
                    toggle_text,
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    10,
                    palette.text,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const bool hovered = hovered_card_ == static_cast<int>(index);
        const nfui::Color fill = hovered
                                     ? nfui::alpha_blend(palette.accent, palette.surface, 0.22f)
                                     : palette.surface;
        const nfui::Color border = hovered ? palette.accent : palette.border;
        nfui::fill_rounded_rect(hdc, layout.cards[index], radius, fill, border);

        RECT card_text = inset_rect(layout.cards[index], gap);
        draw_text_block(hdc,
                        card_text,
                        card_titles[index],
                        fonts,
                        dpi(),
                        FontWeight::regular,
                        11,
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        card_text.top += dpi_scale_.logical_to_pixels(28);
        draw_text_block(hdc,
                        card_text,
                        card_values[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        20,
                        palette.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT badge_rect = make_rect(layout.cards[index].left + gap,
                                    layout.cards[index].bottom - dpi_scale_.logical_to_pixels(54),
                                    dpi_scale_.logical_to_pixels(130),
                                    dpi_scale_.logical_to_pixels(30));
        const nfui::Color badge_fill = index == 1
                                           ? nfui::alpha_blend(palette.window, palette.success, 0.72f)
                                           : (index == 2
                                                  ? nfui::alpha_blend(palette.window, palette.warning, 0.78f)
                                                  : nfui::alpha_blend(palette.window, palette.accent, 0.82f));
        const nfui::Color badge_border = index == 1 ? palette.success : (index == 2 ? palette.warning : palette.accent);
        nfui::fill_rounded_rect(hdc, badge_rect, small_radius, badge_fill, badge_border);
        RECT badge_text = inset_rect(badge_rect, dpi_scale_.logical_to_pixels(8));
        draw_text_block(hdc,
                        badge_text,
                        card_badges[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        10,
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT workspace_note = make_rect(layout.workspace.left,
                                    layout.cards[0].bottom + gap,
                                    rect_width(layout.workspace),
                                    dpi_scale_.logical_to_pixels(110));
    nfui::fill_rounded_rect(hdc, workspace_note, radius, palette.surface_alt, palette.border);
    RECT note_text = inset_rect(workspace_note, gap);
    draw_text_block(hdc,
                    note_text,
                    L"Showcase-only visuals: card styling, badges, command-bar treatment, and inspector composition live entirely in samples/NativeFrameUIShowcase. Stable framework guarantees remain HWND access, DPI helpers, resources, controls, and command routing.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    11,
                    palette.text,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    RECT inspector_header = make_rect(layout.inspector.left + gap,
                                      layout.inspector.top + gap,
                                      rect_width(layout.inspector) - gap * 2,
                                      dpi_scale_.logical_to_pixels(68));
    draw_text_block(hdc,
                    inspector_header,
                    L"Inspector",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    14,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    inspector_header.top += dpi_scale_.logical_to_pixels(30);
    draw_text_block(hdc,
                    inspector_header,
                    L"Readable implementation notes for reviewers and release screenshots.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    11,
                    palette.muted_text,
                    DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        nfui::fill_rounded_rect(hdc, layout.inspector_rows[index], small_radius, palette.surface, palette.border);
        RECT row_text = inset_rect(layout.inspector_rows[index], dpi_scale_.logical_to_pixels(12));
        draw_text_block(hdc,
                        row_text,
                        inspector_labels[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        10,
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        row_text.top += dpi_scale_.logical_to_pixels(24);
        draw_text_block(hdc,
                        row_text,
                        inspector_values[index],
                        fonts,
                        dpi(),
                        FontWeight::regular,
                        11,
                        palette.text,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }
}

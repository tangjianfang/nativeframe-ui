#include "ShowcaseView.hpp"

#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/VectorIcon.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace {

enum class FontWeight { regular, semibold, bold };

struct ShowcasePalette {
    nfui::Color window;
    nfui::Color sidebar;
    nfui::Color command_bar;
    nfui::Color surface;
    nfui::Color surface_alt;
    nfui::Color surface_tint;   // CP32: selected nav row fill — distinct from sidebar base
    nfui::Color border;
    nfui::Color accent;
    nfui::Color accent_soft;
    nfui::Color accent_text;
    nfui::Color text;
    nfui::Color muted_text;
    nfui::Color success;
    nfui::Color warning;
    nfui::Color info;
    nfui::Color shadow;
};

struct ShowcaseLayout {
    RECT sidebar{};
    RECT command_bar{};
    RECT workspace{};
    RECT inspector{};
    RECT theme_toggle{};
    RECT divider_left{};
    RECT divider_right{};
    RECT brand_title{};
    RECT brand_tagline{};
    RECT version_badge{};
    RECT avatar{};
    RECT avatar_dot{};
    std::array<RECT, 4> navigation{};
    std::array<RECT, 3> cards{};
    std::array<RECT, 3> inspector_rows{};
    RECT workspace_note{};
};

constexpr std::array<std::wstring_view, 4> navigation_labels{
    L"Overview",
    L"Pipelines",
    L"Reviews",
    L"Resources",
};

// CP32: KPI card content. Label is uppercase copy that goes above the big
// value; the badge below sits in a coloured pill.
constexpr std::array<std::wstring_view, 3> card_titles{
    L"ADOPTION",
    L"BUILD HEALTH",
    L"RESOURCE FLOW",
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

// CP32: inspector pill-tag strings — replace the previous colour-bullets with
// small rounded pills that mirror the version badge style.
constexpr std::array<std::wstring_view, 3> inspector_tags{
    L"12px",
    L"mono",
    L"dpi-aware",
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

[[nodiscard]] RECT inset_rect(const RECT& rect, int left, int top, int right, int bottom) noexcept {
    RECT inset = rect;
    inset.left += left;
    inset.top += top;
    inset.right -= right;
    inset.bottom -= bottom;
    return inset;
}

[[nodiscard]] HFONT fetch_font(nfui::FontCache& fonts,
                               int dpi,
                               FontWeight weight,
                               int point_size) noexcept {
    switch (weight) {
    case FontWeight::semibold: return fonts.semibold(dpi, point_size);
    case FontWeight::bold:     return fonts.bold(dpi, point_size);
    case FontWeight::regular:
    default:                   return fonts.regular(dpi, point_size);
    }
}

[[nodiscard]] ShowcasePalette palette_for(nfui::ThemeMode mode) noexcept {
    const nfui::ThemePalette p = nfui::theme_palette(mode);
    const bool dark = mode == nfui::ThemeMode::dark;

    ShowcasePalette palette{};
    palette.window = p.background;
    // CP32: sidebar gets a subtle tint away from the window base so the
    // selected row's surface fill can read as "lighter" against the rail.
    palette.sidebar = dark ? nfui::darken(p.background, 0.04f)
                           : nfui::alpha_blend(p.border, p.background, 0.30f);
    palette.command_bar = dark ? p.surface
                               : p.surface;
    palette.surface = p.surface;
    // CP32: surface_alt lifts slightly more than surface so workspace panels
    // ("Showcase-only visuals") stay distinct from KPI cards.
    palette.surface_alt = dark ? nfui::lighten(p.surface, 0.04f)
                               : nfui::alpha_blend(p.border, p.surface, 0.10f);
    // CP32: selected-nav fill — lighter than sidebar base in dark mode, light
    // surface in light mode. Reads as the row that owns focus.
    palette.surface_tint = dark ? p.surface_hover
                                : nfui::alpha_blend(p.surface, p.background, 0.55f);
    palette.border = p.border;
    palette.accent = p.accent;
    palette.accent_soft = nfui::alpha_blend(p.background, p.accent, dark ? 0.70f : 0.80f);
    palette.accent_text = p.accent_text;
    palette.text = p.text;
    palette.muted_text = p.text_secondary;
    palette.success = p.success;
    palette.warning = p.warning;
    palette.info = p.info;
    palette.shadow  = p.shadow;
    return palette;
}

void fill_rect_color(HDC hdc, const RECT& rect, nfui::Color color) noexcept {
    if (HBRUSH brush = CreateSolidBrush(color.rgb)) {
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }
}

// CP32: draws the nav-row glyph. The actual IconKind enum is a small subset of
// the named icons suggested by the redesign doc, so each row draws a small
// geometric glyph tuned to the section — a trend for Overview, a graph for
// Pipelines, a checklist for Reviews, and a folder for Resources. Strokes are
// caller-resolution colour (muted / accent).
void draw_nav_glyph(HDC hdc, std::size_t index, const RECT& cell, nfui::Color color, int stroke_px) noexcept {
    const int left = cell.left;
    const int top = cell.top;
    const int w = rect_width(cell);
    const int h = rect_height(cell);

    auto stroke = [&](POINT a, POINT b) noexcept {
        nfui::draw_line(hdc, a, b, color, stroke_px);
    };

    switch (index) {
    case 0: {
        // Overview: mini trend chart — 3 vertical bars + diagonal trendline.
        const int base = top + h - 1;
        const int bar_w = std::max(2, w / 8);
        const int gap = std::max(2, w / 6);
        const int heights[3] = { h / 3, h - h / 3 - 2, h / 2 };
        for (int i = 0; i < 3; ++i) {
            const int bx = left + (w / 4 - bar_w) + i * gap;
            const RECT bar = make_rect(bx, base - heights[i], bar_w, heights[i]);
            fill_rect_color(hdc, bar, color);
        }
        // Trendline from low-left to high-right.
        const POINT a{ left + 1, base - h / 3 };
        const POINT b{ left + w - 1, top + h / 4 };
        nfui::draw_polyline(hdc, std::array<POINT, 2>{a, b}.data(), 2, color, stroke_px);
        break;
    }
    case 1: {
        // Pipelines: 3 nodes connected by lines (graph/dag).
        const int cx0 = left + w / 4;
        const int cy0 = top + h / 2;
        const int cx1 = left + w / 2;
        const int cy1 = top + h / 4;
        const int cx2 = left + (3 * w) / 4;
        const int cy2 = top + (3 * h) / 4;
        stroke({cx0, cy0}, {cx1, cy1});
        stroke({cx1, cy1}, {cx2, cy2});
        stroke({cx0, cy0}, {cx2, cy2});
        const int dot_r = std::max(2, w / 8);
        nfui::fill_ellipse(hdc, make_rect(cx0 - dot_r, cy0 - dot_r, dot_r * 2, dot_r * 2), color);
        nfui::fill_ellipse(hdc, make_rect(cx1 - dot_r, cy1 - dot_r, dot_r * 2, dot_r * 2), color);
        nfui::fill_ellipse(hdc, make_rect(cx2 - dot_r, cy2 - dot_r, dot_r * 2, dot_r * 2), color);
        break;
    }
    case 2: {
        // Reviews: checklist — 2 small check marks + 3 horizontal lines.
        const int row_h = std::max(3, h / 5);
        const int left_pad = std::max(2, w / 12);
        const int check_w = std::max(3, w / 6);
        for (int i = 0; i < 3; ++i) {
            const int y = top + i * (row_h + 2) + row_h / 2;
            // checkbox square outline
            const RECT box = make_rect(left + left_pad, y - row_h / 2, row_h, row_h);
            nfui::draw_ellipse(hdc, box, color, 1);
            // horizontal text line beside the box
            const int line_x = box.right + 2;
            const int line_w = std::max(4, (w - left_pad - check_w) - (line_x - left));
            if (line_w > 0) {
                fill_rect_color(hdc, make_rect(line_x, y - 1, line_w, 2), color);
            }
        }
        break;
    }
    case 3:
    default: {
        // Resources: folder glyph — tab on top-left + body.
        const int tab_h = std::max(2, h / 5);
        const int tab_w = w / 3;
        const RECT tab = make_rect(left, top + 1, tab_w, tab_h);
        fill_rect_color(hdc, tab, color);
        const RECT body = make_rect(left, top + tab_h - 1, w, h - tab_h + 1);
        // Hollow body: outline only — draw an edge that matches the tab.
        nfui::draw_polyline(hdc,
                            std::array<POINT, 5>{
                                POINT{left, top + tab_h - 1},
                                POINT{left, top + h - 1},
                                POINT{left + w, top + h - 1},
                                POINT{left + w, top + tab_h - 1},
                                POINT{left + tab_w, top + tab_h - 1}}.data(),
                            5, color, stroke_px);
        break;
    }
    }
}

[[nodiscard]] ShowcaseLayout build_layout(const RECT& client_rect, const nfui::DpiScale& dpi) noexcept {
    ShowcaseLayout layout{};
    const int outer = dpi.logical_to_pixels(24);
    const int gap = dpi.logical_to_pixels(16);
    const int sidebar_width = dpi.logical_to_pixels(240);
    const int inspector_width = dpi.logical_to_pixels(320);
    const int command_height = dpi.logical_to_pixels(116);
    const int nav_height = dpi.logical_to_pixels(40);
    const int nav_gap = dpi.logical_to_pixels(4);
    const int card_height = dpi.logical_to_pixels(180);
    const int inspector_row_height = dpi.logical_to_pixels(120);
    const int note_height = dpi.logical_to_pixels(140);

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

    // Brand area occupies the top of the sidebar.
    const int brand_pad = dpi.logical_to_pixels(8);
    layout.brand_title = make_rect(layout.sidebar.left + gap + brand_pad,
                                   layout.sidebar.top + dpi.logical_to_pixels(20),
                                   rect_width(layout.sidebar) - (gap + brand_pad) * 2,
                                   dpi.logical_to_pixels(36));
    layout.brand_tagline = make_rect(layout.sidebar.left + gap + brand_pad,
                                     layout.brand_title.bottom + dpi.logical_to_pixels(2),
                                     rect_width(layout.sidebar) - (gap + brand_pad) * 2,
                                     dpi.logical_to_pixels(40));

    // Nav rows begin below the brand block (after tagline + breathing room).
    const int nav_left = layout.sidebar.left + dpi.logical_to_pixels(12);
    const int nav_width = sidebar_width - dpi.logical_to_pixels(24);
    const int nav_top_start = layout.brand_tagline.bottom + dpi.logical_to_pixels(40);
    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const int y = nav_top_start + static_cast<int>(index) * (nav_height + nav_gap);
        layout.navigation[index] = make_rect(nav_left, y, nav_width, nav_height);
    }

    // Theme toggle — pinned top-right of the command bar with a more prominent
    // footprint than the previous build.
    const int toggle_width = dpi.logical_to_pixels(160);
    const int toggle_height = dpi.logical_to_pixels(40);
    const int toggle_pad = dpi.logical_to_pixels(16);
    layout.theme_toggle = make_rect(layout.command_bar.right - toggle_width - toggle_pad,
                                    layout.command_bar.top + toggle_pad,
                                    toggle_width,
                                    toggle_height);

    // KPI cards row.
    const int card_gap = gap;
    const int total_card_gap = card_gap * 2;
    const int card_width = std::max((workspace_width - total_card_gap) / 3, dpi.logical_to_pixels(160));
    const int cards_top = layout.workspace.top;
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const int x = layout.workspace.left + static_cast<int>(index) * (card_width + card_gap);
        layout.cards[index] = make_rect(x, cards_top, card_width, card_height);
    }

    // Inspector rows — title (with tag pill) on top, body paragraph below.
    const int inspector_row_top = layout.inspector.top + dpi.logical_to_pixels(118);
    const int inspector_row_width = inspector_width - gap * 2;
    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        const int y = inspector_row_top + static_cast<int>(index) * (inspector_row_height + gap);
        layout.inspector_rows[index] = make_rect(layout.inspector.left + gap,
                                                 y,
                                                 inspector_row_width,
                                                 inspector_row_height);
    }

    layout.workspace_note = make_rect(layout.workspace.left,
                                      layout.cards[0].bottom + gap,
                                      workspace_width,
                                      note_height);

    // CP32: bottom of the sidebar hosts a small version pill (left) + an
    // avatar with status dot (right).
    const int bottom_band_h = dpi.logical_to_pixels(56);
    const int version_w = dpi.logical_to_pixels(72);
    const int version_h = dpi.logical_to_pixels(28);
    const int avatar_d = dpi.logical_to_pixels(36);
    const int side_pad = dpi.logical_to_pixels(20);
    const int bottom_y = layout.sidebar.bottom - dpi.logical_to_pixels(20) - bottom_band_h;
    layout.version_badge = make_rect(layout.sidebar.left + side_pad,
                                     bottom_y + (bottom_band_h - version_h) / 2,
                                     version_w,
                                     version_h);
    layout.avatar = make_rect(layout.sidebar.right - side_pad - avatar_d,
                             bottom_y + (bottom_band_h - avatar_d) / 2,
                             avatar_d,
                             avatar_d);
    const int dot_size = dpi.logical_to_pixels(11);
    layout.avatar_dot = make_rect(layout.avatar.right - dot_size + dpi.logical_to_pixels(2),
                                  layout.avatar.bottom - dot_size + dpi.logical_to_pixels(2),
                                  dot_size,
                                  dot_size);

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
        // CP22: clicking the toggle also moves focus there, so a keyboard
        // user can keep the same affordance on-screen when they switch to
        // the keyboard. Toggle always paints differently (theme change), so
        // always returns true.
        focus_index_ = 0;
        toggle_theme();
        return true;
    }

    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        if (PtInRect(&layout.navigation[index], point) != FALSE) {
            // CP22: capture the prior focus before the assignment so we
            // can return true when EITHER the focus OR the selected nav
            // moved. Without this, clicking an already-selected nav row
            // (the default state on first launch) moved focus from -1
            // → index+1 but the function returned false, so the focus
            // ring never redrew. (Found by the CP22 adversarial review.)
            const int prior_focus = focus_index_;
            const int new_focus   = static_cast<int>(index) + 1;
            const bool focus_changed   = prior_focus != new_focus;
            const bool selection_changed = selected_navigation_ != static_cast<int>(index);
            focus_index_ = new_focus;
            if (selection_changed) {
                selected_navigation_ = static_cast<int>(index);
            }
            return focus_changed || selection_changed;
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

void ShowcaseView::set_focus_index(int index) noexcept {
    // 0..4 are valid; -1 clears focus.
    if (index >= -1 && index <= 4) {
        focus_index_ = index;
    }
}

void ShowcaseView::cycle_focus(bool reverse) noexcept {
    constexpr int kCount = 5;  // 0 = toggle, 1..4 = nav[0..3]
    if (focus_index_ < 0) {
        focus_index_ = reverse ? (kCount - 1) : 0;
        return;
    }
    if (reverse) {
        focus_index_ = (focus_index_ - 1 + kCount) % kCount;
    } else {
        focus_index_ = (focus_index_ + 1) % kCount;
    }
}

bool ShowcaseView::activate_focused() noexcept {
    if (focus_index_ == 0) {
        toggle_theme();
        return true;
    }
    if (focus_index_ >= 1 && focus_index_ <= 4) {
        const int nav_index = focus_index_ - 1;
        if (selected_navigation_ != nav_index) {
            selected_navigation_ = nav_index;
        }
        return true;
    }
    return false;
}

RECT ShowcaseView::focused_rect() const noexcept {
    RECT r{};
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    if (focus_index_ == 0) {
        r = layout.theme_toggle;
    } else if (focus_index_ >= 1 && focus_index_ <= 4) {
        r = layout.navigation[focus_index_ - 1];
    }
    return r;
}

void ShowcaseView::paint(HDC hdc, nfui::FontCache& fonts) const noexcept {
    const ShowcasePalette palette = palette_for(theme_mode_);
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    const bool dark = theme_mode_ == nfui::ThemeMode::dark;
    const int gap = dpi_scale_.logical_to_pixels(16);
    const int toggle_pad = dpi_scale_.logical_to_pixels(16);
    const int indicator_inset = dpi_scale_.logical_to_pixels(4);
    const int radius = dpi_scale_.logical_to_pixels(10);
    const int small_radius = dpi_scale_.logical_to_pixels(8);
    const int pill_radius = dpi_scale_.logical_to_pixels(999);

    // Backdrop + sidebar.
    fill_rect_color(hdc, client_rect_, palette.window);
    fill_rect_color(hdc, layout.sidebar, palette.sidebar);

    // Command bar — flat surface card (no border, the surface lift does the work).
    nfui::fill_rounded_rect(hdc, layout.command_bar, radius, palette.command_bar, palette.command_bar);

    // Inspector rail.
    nfui::fill_rounded_rect(hdc, layout.inspector, radius, palette.surface_alt, palette.surface_alt);

    // Brand block at top of the sidebar.
    nfui::draw_text(hdc,
                    layout.brand_title,
                    L"NativeFrame UI",
                    fetch_font(fonts, dpi(), FontWeight::bold, nfui::font_pt::xl),
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    nfui::draw_text(hdc,
                    layout.brand_tagline,
                    L"Modern Win32 showcase",
                    fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::sm),
                    palette.muted_text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // CP32: nav rows — glyph cell on the left + label. Selected row gets a
    // 2-px coral bar at the left edge AND a tinted fill so the row clearly
    // owns focus. The bar is inset 4 logical px top/bottom, not 3 px.
    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const bool selected = selected_navigation_ == static_cast<int>(index);
        if (selected) {
            nfui::fill_rounded_rect(hdc, layout.navigation[index], small_radius,
                                    palette.surface_tint, palette.surface_tint);
            const int indicator_width = dpi_scale_.logical_to_pixels(2);
            RECT indicator = make_rect(layout.navigation[index].left,
                                       layout.navigation[index].top + indicator_inset,
                                       indicator_width,
                                       rect_height(layout.navigation[index]) - indicator_inset * 2);
            fill_rect_color(hdc, indicator, palette.accent);
        }

        const int glyph_pad = dpi_scale_.logical_to_pixels(12);
        const int glyph_box = rect_height(layout.navigation[index]) - dpi_scale_.logical_to_pixels(10);
        RECT glyph = make_rect(layout.navigation[index].left + glyph_pad,
                               layout.navigation[index].top + (rect_height(layout.navigation[index]) - glyph_box) / 2,
                               glyph_box,
                               glyph_box);
        const nfui::Color glyph_color = selected ? palette.accent : palette.muted_text;
        draw_nav_glyph(hdc, index, glyph, glyph_color, std::max(1, dpi_scale_.logical_to_pixels(1)));

        RECT label_rect = make_rect(glyph.right + dpi_scale_.logical_to_pixels(10),
                                    layout.navigation[index].top,
                                    layout.navigation[index].right - (glyph.right + dpi_scale_.logical_to_pixels(10)),
                                    rect_height(layout.navigation[index]));
        nfui::draw_text(hdc,
                        label_rect,
                        navigation_labels[index],
                        fetch_font(fonts, dpi(), selected ? FontWeight::semibold : FontWeight::regular, nfui::font_pt::base),
                        selected ? palette.text : palette.muted_text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // CP32: bottom of the sidebar — version pill (coral soft fill + coral text)
    // and avatar (coral circle with JD + green status dot).
    nfui::fill_rounded_rect(hdc, layout.version_badge, pill_radius, palette.accent_soft, palette.accent_soft);
    {
        RECT badge_text = inset_rect(layout.version_badge, dpi_scale_.logical_to_pixels(8));
        nfui::draw_text(hdc,
                        badge_text,
                        L"v1.4.0",
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::sm),
                        palette.accent,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    {
        // Avatar: in dark mode the circle is filled coral with white initials
        // so the right-rail reads as branded. In light mode the circle gets a
        // surface fill with a coral ring so it doesn't compete with the nav
        // and stays tasteful against the cream background.
        const nfui::Color avatar_fill = dark ? palette.accent : palette.surface;
        const nfui::Color avatar_border = dark ? palette.accent : palette.accent;
        const nfui::Color initials_color = dark ? palette.accent_text : palette.accent;
        nfui::fill_ellipse(hdc, layout.avatar, avatar_fill);
        nfui::draw_ellipse(hdc, layout.avatar, avatar_border, std::max(1, dpi_scale_.logical_to_pixels(1)));
        nfui::draw_text(hdc,
                        layout.avatar,
                        L"JD",
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::base),
                        initials_color,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Status dot at bottom-right (5-px green disc). A thin outer halo
        // matches the sidebar paint so the dot reads against the avatar.
        const int dot_inset = dpi_scale_.logical_to_pixels(2);
        RECT halo = inset_rect(layout.avatar_dot, -dot_inset, -dot_inset, -dot_inset, -dot_inset);
        nfui::fill_ellipse(hdc, halo, palette.sidebar);
        nfui::fill_ellipse(hdc, layout.avatar_dot, palette.success);
    }

    // Workspace — hero card "Product Growth Showcase" + description; the
    // description text reads as body copy, not as muted subtitle.
    {
        RECT hero_title = make_rect(layout.command_bar.left + gap,
                                    layout.command_bar.top + gap,
                                    layout.command_bar.right - layout.command_bar.left - toggle_pad - dpi_scale_.logical_to_pixels(180) - gap * 2,
                                    dpi_scale_.logical_to_pixels(32));
        nfui::draw_text(hdc,
                        hero_title,
                        L"Product Growth Showcase",
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::md),
                        palette.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        RECT hero_body = make_rect(layout.command_bar.left + gap,
                                    hero_title.bottom + dpi_scale_.logical_to_pixels(6),
                                    rect_width(hero_title),
                                    dpi_scale_.logical_to_pixels(56));
        nfui::draw_text(hdc,
                        hero_body,
                        L"Focused on evaluation-ready light and dark shells, explicit resources, and DPI-aware composition.",
                        fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::sm),
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    // Theme toggle — more prominent: the toggle paints with a coral border in
    // dark mode (brand emphasis) and a neutral border in light mode. The label
    // is the canonical action copy and uses base-text colour.
    {
        const nfui::Color toggle_border = dark ? palette.accent : palette.border;
        nfui::fill_rounded_rect(hdc, layout.theme_toggle, small_radius, palette.surface, toggle_border);
        const std::wstring_view toggle_text =
            theme_mode_ == nfui::ThemeMode::dark ? L"Switch to light" : L"Switch to dark";
        RECT toggle_label = inset_rect(layout.theme_toggle, dpi_scale_.logical_to_pixels(10));
        nfui::draw_text(hdc,
                        toggle_label,
                        toggle_text,
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::base),
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // KPI cards — small uppercase label, big value, pill badge centred below.
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const bool hovered = hovered_card_ == static_cast<int>(index);
        const nfui::Color fill = hovered
                                     ? nfui::alpha_blend(palette.accent, palette.surface, 0.12f)
                                     : palette.surface;
        const nfui::Color border = hovered ? palette.accent : palette.border;
        const int card_elevation = 1;
        nfui::paint_drop_shadow(hdc, layout.cards[index], radius,
                                card_elevation, palette.shadow);
        nfui::fill_rounded_rect(hdc, layout.cards[index], radius, fill, border);

        // Top: small uppercase label.
        const int label_pad = dpi_scale_.logical_to_pixels(16);
        RECT card_label = make_rect(layout.cards[index].left + label_pad,
                                    layout.cards[index].top + label_pad,
                                    rect_width(layout.cards[index]) - label_pad * 2,
                                    dpi_scale_.logical_to_pixels(20));
        nfui::draw_text(hdc,
                        card_label,
                        card_titles[index],
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::sm),
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        // Middle: large bold value. Anchored to the card center vertically so
        // the title and pill stay balanced.
        RECT card_value = make_rect(layout.cards[index].left + label_pad,
                                    layout.cards[index].top + dpi_scale_.logical_to_pixels(46),
                                    rect_width(layout.cards[index]) - label_pad * 2,
                                    dpi_scale_.logical_to_pixels(48));
        nfui::draw_text(hdc,
                        card_value,
                        card_values[index],
                        fetch_font(fonts, dpi(), FontWeight::bold, nfui::font_pt::lg),
                        palette.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Bottom: a coloured pill whose size hugs its label. Each row uses
        // the semantic colour (Stable / green, Release Ready / blue, Explicit /
        // coral) per the design spec.
        nfui::Color semantic{};
        nfui::Color pill_fill{};
        nfui::Color pill_border{};
        nfui::Color pill_text{};
        switch (index) {
        case 0: // Stable — success/green.
            semantic    = palette.success;
            pill_fill   = nfui::alpha_blend(palette.success, palette.surface, 0.18f);
            pill_border = nfui::alpha_blend(palette.success, palette.surface, 0.55f);
            pill_text   = dark ? nfui::lighten(palette.success, 0.10f) : palette.success;
            break;
        case 1: // Release Ready — success/green (matches the Stable pill so
                // both read as "ready" — Explicit is the only coral pill).
            semantic    = palette.success;
            pill_fill   = nfui::alpha_blend(palette.success, palette.surface, 0.18f);
            pill_border = nfui::alpha_blend(palette.success, palette.surface, 0.55f);
            pill_text   = dark ? nfui::lighten(palette.success, 0.10f) : palette.success;
            break;
        case 2: // Explicit — accent/coral.
            semantic    = palette.accent;
            pill_fill   = nfui::alpha_blend(palette.accent, palette.surface, 0.18f);
            pill_border = nfui::alpha_blend(palette.accent, palette.surface, 0.55f);
            pill_text   = palette.accent;
            break;
        default:
            semantic    = palette.accent;
            pill_fill   = palette.accent_soft;
            pill_border = palette.accent_soft;
            pill_text   = palette.accent;
            break;
        }
        // Compute a pill that hugs the label width. Pill width = text width
        // + 2*padding; height = 24 logical px.
        HDC measure_hdc = CreateCompatibleDC(hdc);
        HFONT measure_font = fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::xs);
        HFONT old_font = static_cast<HFONT>(SelectObject(measure_hdc, measure_font));
        SIZE text_size{};
        GetTextExtentPoint32W(measure_hdc, card_badges[index].data(),
                              static_cast<int>(card_badges[index].size()), &text_size);
        SelectObject(measure_hdc, old_font);
        DeleteDC(measure_hdc);
        const int pill_h = dpi_scale_.logical_to_pixels(24);
        const int pill_w = text_size.cx + dpi_scale_.logical_to_pixels(20);
        const int pill_pad_x = dpi_scale_.logical_to_pixels(16);
        const int pill_y = layout.cards[index].bottom - dpi_scale_.logical_to_pixels(20) - pill_h;
        RECT pill_rect = make_rect(layout.cards[index].left + pill_pad_x, pill_y, pill_w, pill_h);
        nfui::fill_rounded_rect(hdc, pill_rect, pill_radius, pill_fill, pill_border);
        RECT pill_text_rect = inset_rect(pill_rect, dpi_scale_.logical_to_pixels(10), 0,
                                        dpi_scale_.logical_to_pixels(10), 0);
        nfui::draw_text(hdc,
                        pill_text_rect,
                        card_badges[index],
                        measure_font,
                        pill_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        (void)semantic; // silence unused-but-reserved warning on some toolchains.
    }

    // Workspace note card — "Showcase-only visuals" copy in a flat panel.
    nfui::fill_rounded_rect(hdc, layout.workspace_note, radius, palette.surface_alt, palette.surface_alt);
    {
        const int note_pad = gap;
        RECT note_title = make_rect(layout.workspace_note.left + note_pad,
                                    layout.workspace_note.top + note_pad,
                                    rect_width(layout.workspace_note) - note_pad * 2,
                                    dpi_scale_.logical_to_pixels(28));
        nfui::draw_text(hdc,
                        note_title,
                        L"Showcase-only visuals",
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::md),
                        palette.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT note_body = make_rect(layout.workspace_note.left + note_pad,
                                    note_title.bottom + dpi_scale_.logical_to_pixels(8),
                                    rect_width(layout.workspace_note) - note_pad * 2,
                                    layout.workspace_note.bottom - (note_title.bottom + dpi_scale_.logical_to_pixels(8)));
        nfui::draw_text(hdc,
                        note_body,
                        L"These visuals are for demonstration purposes, highlighting the capabilities of the NativeFrame UI system in a product context. They are not representative of the final product's full data or functionality.",
                        fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::sm),
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    // Inspector — header + body intro, then 3 rows with a label-tag-pill pair.
    {
        RECT inspector_header = make_rect(layout.inspector.left + gap,
                                          layout.inspector.top + gap,
                                          rect_width(layout.inspector) - gap * 2,
                                          dpi_scale_.logical_to_pixels(36));
        nfui::draw_text(hdc,
                        inspector_header,
                        L"Inspector",
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::md),
                        palette.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT inspector_intro = make_rect(layout.inspector.left + gap,
                                         inspector_header.bottom + dpi_scale_.logical_to_pixels(6),
                                         rect_width(layout.inspector) - gap * 2,
                                         dpi_scale_.logical_to_pixels(60));
        nfui::draw_text(hdc,
                        inspector_intro,
                        L"Readable implementation notes for reviewers and release screenshots.",
                        fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::sm),
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        nfui::fill_rounded_rect(hdc, layout.inspector_rows[index], small_radius, palette.surface, palette.surface);
        // Subtle border for the inspector cards in dark mode so the lighter
        // surface still separates from the surface_alt background.
        {
            HBRUSH brush = CreateSolidBrush(palette.border.rgb);
            HPEN pen = CreatePen(PS_SOLID, 1, palette.border.rgb);
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            RoundRect(hdc, layout.inspector_rows[index].left, layout.inspector_rows[index].top,
                      layout.inspector_rows[index].right, layout.inspector_rows[index].bottom,
                      small_radius * 2, small_radius * 2);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
            DeleteObject(brush);
        }

        const int row_pad = dpi_scale_.logical_to_pixels(16);
        RECT label_rect = make_rect(layout.inspector_rows[index].left + row_pad,
                                    layout.inspector_rows[index].top + row_pad,
                                    rect_width(layout.inspector_rows[index]) - row_pad * 2 - dpi_scale_.logical_to_pixels(60),
                                    dpi_scale_.logical_to_pixels(24));
        nfui::draw_text(hdc,
                        label_rect,
                        inspector_labels[index],
                        fetch_font(fonts, dpi(), FontWeight::semibold, nfui::font_pt::base),
                        palette.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Right-edge tag pill — neutral fill, muted text. Width hugs the tag.
        HDC measure_hdc = CreateCompatibleDC(hdc);
        HFONT tag_font = fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::xs);
        HFONT old_font = static_cast<HFONT>(SelectObject(measure_hdc, tag_font));
        SIZE text_size{};
        GetTextExtentPoint32W(measure_hdc, inspector_tags[index].data(),
                              static_cast<int>(inspector_tags[index].size()), &text_size);
        SelectObject(measure_hdc, old_font);
        DeleteDC(measure_hdc);
        const int tag_h = dpi_scale_.logical_to_pixels(22);
        const int tag_w = text_size.cx + dpi_scale_.logical_to_pixels(16);
        RECT tag_rect = make_rect(layout.inspector_rows[index].right - row_pad - tag_w,
                                  layout.inspector_rows[index].top + row_pad + dpi_scale_.logical_to_pixels(1),
                                  tag_w,
                                  tag_h);
        const nfui::Color tag_fill = nfui::alpha_blend(palette.muted_text, palette.surface, dark ? 0.18f : 0.82f);
        nfui::fill_rounded_rect(hdc, tag_rect, pill_radius, tag_fill, tag_fill);
        nfui::draw_text(hdc,
                        inset_rect(tag_rect, dpi_scale_.logical_to_pixels(8), 0,
                                   dpi_scale_.logical_to_pixels(8), 0),
                        inspector_tags[index],
                        tag_font,
                        dark ? palette.text : palette.muted_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT body_rect = make_rect(layout.inspector_rows[index].left + row_pad,
                                   label_rect.bottom + dpi_scale_.logical_to_pixels(6),
                                   rect_width(layout.inspector_rows[index]) - row_pad * 2,
                                   layout.inspector_rows[index].bottom - label_rect.bottom - dpi_scale_.logical_to_pixels(22));
        nfui::draw_text(hdc,
                        body_rect,
                        inspector_values[index],
                        fetch_font(fonts, dpi(), FontWeight::regular, nfui::font_pt::sm),
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    // CP22: focus ring for the currently-focused affordance. Drawn LAST
    // (after the affordance's own fill + text) using paint_focus_border —
    // a stroke-only rounded ring at `focused` rect inset by 2 logical px
    // on each side. Stroking only (not filling) means the affordance's
    // surface + label stay visible underneath; the previous version called
    // fill_rounded_rect with palette.window as the fill, which obliterated
    // the focused control's chrome. (Found by the CP22 adversarial review.)
    if (focus_index_ >= 0) {
        const RECT focused = focused_rect();
        if (focused.right > focused.left && focused.bottom > focused.top) {
            nfui::paint_focus_border(hdc, focused, palette.accent, 2);
        }
    }
}

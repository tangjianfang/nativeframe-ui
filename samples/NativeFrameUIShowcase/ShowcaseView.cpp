#include "ShowcaseView.hpp"

#include <nfui/Paint.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace {

enum class FontWeight { regular, semibold, bold, serif, mono };

struct ShowcasePalette {
    nfui::Color window;
    nfui::Color sidebar;
    nfui::Color header;
    nfui::Color surface;
    nfui::Color surface_alt;
    nfui::Color border;
    nfui::Color accent;
    nfui::Color accent_soft;
    nfui::Color text;
    nfui::Color muted_text;
    nfui::Color success;
    nfui::Color success_soft;
    nfui::Color warning;
    nfui::Color online;
    nfui::Color accent_text;
    nfui::Color shadow;
};

struct ShowcaseLayout {
    RECT sidebar{};
    RECT header{};
    RECT workspace{};
    RECT inspector{};
    RECT search_box{};
    RECT notification{};
    RECT theme_toggle{};
    RECT divider_left{};
    RECT divider_right{};
    RECT brand_title{};
    RECT brand_subtitle{};
    RECT sidebar_footer{};
    RECT avatar{};
    RECT version_pill{};
    std::array<RECT, 5> navigation{};
    std::array<RECT, 5> nav_icons{};
    std::array<RECT, 5> nav_labels{};
    std::array<RECT, 3> cards{};
    std::array<RECT, 3> card_titles{};
    std::array<RECT, 3> card_values{};
    std::array<RECT, 3> card_badges{};
    std::array<RECT, 3> inspector_rows{};
    std::array<RECT, 3> inspector_headers{};
    std::array<RECT, 3> inspector_bodies{};
    std::array<RECT, 3> inspector_tags{};
    RECT note_card{};
    RECT chart_card{};
};

constexpr std::array<std::wstring_view, 5> navigation_labels{
    L"Overview",
    L"Pipelines",
    L"Reviews",
    L"Resources",
    L"Settings",
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

constexpr std::array<std::wstring_view, 3> inspector_tags{
    L"12px",
    L"12px",
    L"12px",
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
    switch (weight) {
    case FontWeight::bold:     return fonts.bold(dpi, point_size);
    case FontWeight::semibold: return fonts.semibold(dpi, point_size);
    case FontWeight::serif:    return fonts.serif(dpi, point_size);
    case FontWeight::mono:     return fonts.mono(dpi, point_size);
    case FontWeight::regular:
    default:                   return fonts.regular(dpi, point_size);
    }
}

[[nodiscard]] ShowcasePalette palette_for(nfui::ThemeMode mode) noexcept {
    const nfui::ThemePalette p = nfui::theme_palette(mode);
    const bool dark = mode == nfui::ThemeMode::dark;

    ShowcasePalette palette{};
    palette.window = p.background;
    palette.sidebar = dark ? nfui::darken(p.background, 0.18f)
                           : nfui::alpha_blend(p.border, p.background, 0.08f);
    palette.header = p.surface;
    palette.surface = p.surface;
    palette.surface_alt = dark ? nfui::darken(p.background, 0.12f)
                               : nfui::alpha_blend(p.border, p.background, 0.06f);
    palette.border = p.border;
    palette.accent = p.accent;
    palette.accent_soft = nfui::alpha_blend(p.background, p.accent, dark ? 0.72f : 0.80f);
    palette.text = p.text;
    palette.muted_text = p.text_secondary;
    palette.success = p.success;
    palette.success_soft = nfui::alpha_blend(p.background, p.success, 0.80f);
    palette.warning = p.warning;
    palette.online = p.success;
    palette.accent_text = p.accent_text;
    palette.shadow = p.shadow;
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
    if (text.empty()) return;
    const HFONT font = fetch_font(fonts, dpi, weight, point_size);
    nfui::draw_text(hdc, rect, text, font, color, format);
}

// --------------------------------------------------------------------------
// Vector icon helpers (filled / stroked with GDI; keep shapes simple and
// geometric so they read crisply at sample sizes without anti-aliasing).
// --------------------------------------------------------------------------

void draw_line_chart_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke;
    const int x0 = r.left + pad;
    const int y0 = r.bottom - pad;
    const int x1 = r.left + rect_width(r) / 2;
    const int y1 = r.top + pad + rect_height(r) / 4;
    const int x2 = r.right - pad;
    const int y2 = r.top + pad;
    const std::array<POINT, 3> pts{{{x0, y0}, {x1, y1}, {x2, y2}}};
    nfui::draw_polyline(hdc, pts.data(), static_cast<int>(pts.size()), color, stroke);
    nfui::fill_ellipse(hdc, make_rect(x2 - stroke, y2 - stroke, stroke * 2 + 1, stroke * 2 + 1), color);
}

void draw_pipelines_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 1;
    const int top_y = r.top + pad;
    const int bot_y = r.bottom - pad;
    const int left_x = r.left + pad;
    const int right_x = r.right - pad;
    const int fork_y = r.top + rect_height(r) * 2 / 5;
    nfui::draw_line(hdc, {left_x, top_y}, {left_x, fork_y}, color, stroke);
    nfui::draw_line(hdc, {left_x, fork_y}, {right_x, top_y + pad}, color, stroke);
    nfui::draw_line(hdc, {left_x, fork_y}, {right_x, bot_y - pad}, color, stroke);
    const int dot = stroke * 2 + 1;
    nfui::fill_ellipse(hdc, make_rect(left_x - stroke, top_y - stroke, dot, dot), color);
    nfui::fill_ellipse(hdc, make_rect(right_x - stroke, top_y + pad - stroke, dot, dot), color);
    nfui::fill_ellipse(hdc, make_rect(right_x - stroke, bot_y - pad - stroke, dot, dot), color);
}

void draw_reviews_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 1;
    const int y1 = r.top + pad;
    const int y2 = (r.top + r.bottom) / 2;
    const int y3 = r.bottom - pad;
    const int x1 = r.left + pad;
    const int x2 = r.right - pad;
    const int line_w = std::max(stroke, 2);
    nfui::draw_line(hdc, {x1, y1}, {x2, y1}, color, line_w);
    nfui::draw_line(hdc, {x1, y2}, {x2 - rect_width(r) / 3, y2}, color, line_w);
    nfui::draw_line(hdc, {x1, y3}, {x2 - rect_width(r) / 3, y3}, color, line_w);
    const int star_cx = x2 - rect_width(r) / 6;
    const int star_cy = y3;
    const int s = std::max(3, rect_height(r) / 6);
    const std::array<POINT, 10> star{{
        {star_cx, star_cy - s},
        {star_cx + s / 4, star_cy - s / 4},
        {star_cx + s, star_cy - s / 4},
        {star_cx + s / 3, star_cy + s / 6},
        {star_cx + s / 2, star_cy + s},
        {star_cx, star_cy + s / 2},
        {star_cx - s / 2, star_cy + s},
        {star_cx - s / 3, star_cy + s / 6},
        {star_cx - s, star_cy - s / 4},
        {star_cx - s / 4, star_cy - s / 4},
    }};
    nfui::fill_polygon(hdc, star.data(), static_cast<int>(star.size()), color, color);
}

void draw_folder_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 1;
    const int tab_h = rect_height(r) / 4;
    const int tab_w = rect_width(r) / 3;
    const POINT body[] = {
        {r.left + pad, r.top + pad + tab_h},
        {r.left + pad + tab_w, r.top + pad + tab_h},
        {r.left + pad + tab_w + stroke * 2, r.top + pad},
        {r.right - pad, r.top + pad},
        {r.right - pad, r.bottom - pad},
        {r.left + pad, r.bottom - pad},
    };
    nfui::fill_polygon(hdc, body, static_cast<int>(std::size(body)), color, color);
}

void draw_settings_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const int outer = std::min(rect_width(r), rect_height(r)) / 2 - stroke;
    const int inner = std::max(outer / 2, 3);
    nfui::draw_ellipse(hdc, make_rect(cx - outer, cy - outer, outer * 2 + 1, outer * 2 + 1), color, stroke);
    nfui::fill_ellipse(hdc, make_rect(cx - inner, cy - inner, inner * 2 + 1, inner * 2 + 1), color);
}

void draw_nav_icon(HDC hdc, const RECT& r, std::size_t index, nfui::Color color, int stroke) noexcept {
    switch (index) {
    case 0: draw_line_chart_icon(hdc, inset_rect(r, stroke), color, stroke); break;
    case 1: draw_pipelines_icon(hdc, inset_rect(r, stroke), color, stroke); break;
    case 2: draw_reviews_icon(hdc, inset_rect(r, stroke), color, stroke); break;
    case 3: draw_folder_icon(hdc, inset_rect(r, stroke), color, stroke); break;
    default: draw_settings_icon(hdc, inset_rect(r, stroke), color, stroke); break;
    }
}

void draw_search_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 1;
    const int cx = r.left + rect_width(r) / 3;
    const int cy = r.top + rect_height(r) / 3;
    const int rad = std::min(rect_width(r), rect_height(r)) / 3 - pad;
    nfui::draw_ellipse(hdc, make_rect(cx - rad, cy - rad, rad * 2 + 1, rad * 2 + 1), color, stroke);
    const int h = rad * 3 / 4;
    nfui::draw_line(hdc, {cx + rad / 2, cy + rad / 2}, {cx + rad / 2 + h, cy + rad / 2 + h}, color, stroke + 1);
}

void draw_bell_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int top = r.top + stroke + 1;
    const int bottom = r.bottom - stroke - 2;
    const int width = rect_width(r) / 2 - stroke;
    const std::array<POINT, 6> body{{
        {cx - width, bottom},
        {cx - width / 2, top + rect_height(r) / 4},
        {cx - width / 2, top},
        {cx + width / 2, top},
        {cx + width / 2, top + rect_height(r) / 4},
        {cx + width, bottom},
    }};
    nfui::draw_polyline(hdc, body.data(), static_cast<int>(body.size()), color, stroke);
    nfui::draw_line(hdc, {cx - width / 3, bottom}, {cx + width / 3, bottom}, color, stroke);
    nfui::fill_ellipse(hdc, make_rect(cx - width / 6, bottom - stroke, width / 3 + 1, stroke * 2 + 1), color);
}

void draw_sun_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const int rad = std::min(rect_width(r), rect_height(r)) / 3;
    nfui::draw_ellipse(hdc, make_rect(cx - rad, cy - rad, rad * 2 + 1, rad * 2 + 1), color, stroke);
    for (int i = 0; i < 8; ++i) {
        const double angle = i * 3.14159265358979323846 / 4.0;
        const int inner = rad + stroke + 1;
        const int outer = std::min(rect_width(r), rect_height(r)) / 2 - stroke;
        const int x1 = cx + static_cast<int>(inner * std::cos(angle));
        const int y1 = cy + static_cast<int>(inner * std::sin(angle));
        const int x2 = cx + static_cast<int>(outer * std::cos(angle));
        const int y2 = cy + static_cast<int>(outer * std::sin(angle));
        nfui::draw_line(hdc, {x1, y1}, {x2, y2}, color, stroke);
    }
}

void draw_moon_icon(HDC hdc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const int rad = std::min(rect_width(r), rect_height(r)) / 2 - stroke - 1;
    // Filled circle with an offset background-filled circle erasing the right
    // side to form a crescent. We use the current background color for the
    // eraser; samples paint over a known fill so this reads correctly.
    nfui::draw_ellipse(hdc, make_rect(cx - rad, cy - rad, rad * 2 + 1, rad * 2 + 1), color, stroke);
    const int erase_rad = rad - stroke * 2;
    const int erase_cx = cx + rad / 2;
    const RECT erase = make_rect(erase_cx - erase_rad, cy - erase_rad, erase_rad * 2 + 1, erase_rad * 2 + 1);
    nfui::fill_ellipse(hdc, erase, color);
}

void draw_theme_icon(HDC hdc, const RECT& r, nfui::ThemeMode mode, nfui::Color color, int stroke) noexcept {
    if (mode == nfui::ThemeMode::dark) {
        draw_moon_icon(hdc, r, color, stroke);
    } else {
        draw_sun_icon(hdc, r, color, stroke);
    }
}

void draw_avatar(HDC hdc, const RECT& r, nfui::Color bg, nfui::Color text, nfui::FontCache& fonts, int dpi,
                 nfui::Color online_color) noexcept {
    nfui::fill_ellipse(hdc, r, bg);
    nfui::draw_ellipse(hdc, r, text, 1);
    RECT text_r = inset_rect(r, dpi / 20);
    draw_text_block(hdc, text_r, L"JD", fonts, dpi, FontWeight::semibold, nfui::font_pt::sm, text,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const int dot = std::max(6, dpi / 16);
    const RECT dot_r = make_rect(r.right - dot * 3 / 2, r.bottom - dot * 3 / 2, dot, dot);
    nfui::fill_ellipse(hdc, dot_r, online_color);
    nfui::draw_ellipse(hdc, dot_r, text, 1);
}

void draw_pill(HDC hdc, const RECT& r, int radius, nfui::Color fill, nfui::Color border,
               std::wstring_view text, nfui::FontCache& fonts, int dpi, FontWeight weight, int pt,
               nfui::Color text_color) noexcept {
    nfui::fill_rounded_rect(hdc, r, radius, fill, border);
    RECT text_r = inset_rect(r, dpi / 32);
    draw_text_block(hdc, text_r, text, fonts, dpi, weight, pt, text_color,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void draw_bar_chart(HDC hdc, const RECT& r, int radius, const std::array<float, 6>& values,
                    nfui::Color bar_color, nfui::Color bg, nfui::Color border) noexcept {
    nfui::fill_rounded_rect(hdc, r, radius, bg, border);
    const int pad = std::max(radius, 12);
    const RECT plot = inset_rect(r, pad);
    const int n = static_cast<int>(values.size());
    const int gap = pad / 2;
    const int bar_w = std::max(4, (rect_width(plot) - gap * (n - 1)) / n);
    const int max_h = rect_height(plot);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const int h = static_cast<int>(max_h * values[i] / 100.0f);
        const int x = plot.left + static_cast<int>(i) * (bar_w + gap);
        const int y = plot.bottom - h;
        const RECT bar = make_rect(x, y, bar_w, h);
        nfui::fill_rounded_rect(hdc, bar, std::min(bar_w / 3, 4), bar_color, bar_color);
    }
}

[[nodiscard]] ShowcaseLayout build_layout(const RECT& client_rect, const nfui::DpiScale& dpi) noexcept {
    ShowcaseLayout layout{};
    const int outer = dpi.logical_to_pixels(20);
    const int gap = dpi.logical_to_pixels(16);
    const int small_gap = dpi.logical_to_pixels(12);
    const int sidebar_width = dpi.logical_to_pixels(280);
    const int inspector_width = dpi.logical_to_pixels(300);
    const int header_height = dpi.logical_to_pixels(120);
    const int nav_height = dpi.logical_to_pixels(44);
    const int nav_gap = dpi.logical_to_pixels(8);
    const int icon_size = dpi.logical_to_pixels(20);
    const int card_height = dpi.logical_to_pixels(150);
    const int inspector_row_height = dpi.logical_to_pixels(120);

    const int client_height = rect_height(client_rect);
    const int content_top = client_rect.top + outer;
    const int content_bottom = client_rect.bottom - outer;

    layout.sidebar = make_rect(client_rect.left + outer,
                               content_top,
                               sidebar_width,
                               std::max(client_height - outer * 2, 0));

    const int workspace_left = layout.sidebar.right + gap;
    const int workspace_right = client_rect.right - outer - inspector_width - gap;
    const int workspace_width = std::max(workspace_right - workspace_left, dpi.logical_to_pixels(480));
    layout.header = make_rect(workspace_left, content_top, workspace_width, header_height);
    layout.workspace = make_rect(workspace_left,
                                 layout.header.bottom + gap,
                                 workspace_width,
                                 std::max(static_cast<int>(content_bottom - layout.header.bottom - gap), 0));
    layout.inspector = make_rect(layout.workspace.right + gap,
                                 content_top,
                                 inspector_width,
                                 std::max(client_height - outer * 2, 0));

    layout.divider_left = make_rect(layout.sidebar.right + gap / 2, content_top, 1, layout.sidebar.bottom - content_top);
    layout.divider_right = make_rect(layout.workspace.right + gap / 2, content_top, 1, layout.sidebar.bottom - content_top);

    // Branding at top of sidebar (tighter horizontal padding so the xl brand fits)
    const int brand_pad = dpi.logical_to_pixels(12);
    const int brand_top = layout.sidebar.top + gap;
    layout.brand_title = make_rect(layout.sidebar.left + brand_pad,
                                   brand_top,
                                   sidebar_width - brand_pad * 2,
                                   dpi.logical_to_pixels(44));
    layout.brand_subtitle = make_rect(layout.sidebar.left + brand_pad,
                                      layout.brand_title.bottom + dpi.logical_to_pixels(2),
                                      sidebar_width - brand_pad * 2,
                                      dpi.logical_to_pixels(22));

    // Navigation rows
    const int nav_left = layout.sidebar.left + gap;
    const int nav_width = sidebar_width - gap * 2;
    const int nav_top_start = layout.brand_subtitle.bottom + dpi.logical_to_pixels(28);
    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const int y = nav_top_start + static_cast<int>(index) * (nav_height + nav_gap);
        layout.navigation[index] = make_rect(nav_left, y, nav_width, nav_height);
        layout.nav_icons[index] = make_rect(nav_left + small_gap,
                                            y + (nav_height - icon_size) / 2,
                                            icon_size,
                                            icon_size);
        layout.nav_labels[index] = make_rect(layout.nav_icons[index].right + small_gap,
                                           y,
                                           nav_width - small_gap * 2 - icon_size - dpi.logical_to_pixels(4),
                                           nav_height);
    }

    // Sidebar footer: version pill + avatar
    const int footer_height = dpi.logical_to_pixels(56);
    const int avatar_size = dpi.logical_to_pixels(40);
    layout.sidebar_footer = make_rect(layout.sidebar.left + gap,
                                      layout.sidebar.bottom - footer_height - gap,
                                      sidebar_width - gap * 2,
                                      footer_height);
    layout.version_pill = make_rect(layout.sidebar_footer.left,
                                      layout.sidebar_footer.top + (footer_height - dpi.logical_to_pixels(28)) / 2,
                                      dpi.logical_to_pixels(64),
                                      dpi.logical_to_pixels(28));
    layout.avatar = make_rect(layout.sidebar_footer.right - avatar_size,
                              layout.sidebar_footer.top + (footer_height - avatar_size) / 2,
                              avatar_size,
                              avatar_size);

    // Header controls: search, notification, theme toggle aligned right
    const int header_ctrl_h = dpi.logical_to_pixels(40);
    const int header_ctrl_y = layout.header.top + (header_height - header_ctrl_h) / 2;
    const int search_width = dpi.logical_to_pixels(170);
    const int icon_btn_size = dpi.logical_to_pixels(40);
    const int ctrl_gap = dpi.logical_to_pixels(12);

    layout.theme_toggle = make_rect(layout.header.right - icon_btn_size,
                                    header_ctrl_y,
                                    icon_btn_size,
                                    header_ctrl_h);
    layout.notification = make_rect(layout.theme_toggle.left - ctrl_gap - icon_btn_size,
                                    header_ctrl_y,
                                    icon_btn_size,
                                    header_ctrl_h);
    layout.search_box = make_rect(layout.notification.left - ctrl_gap - search_width,
                                  header_ctrl_y,
                                  search_width,
                                  header_ctrl_h);

    // KPI cards
    const int card_gap = gap;
    const int total_card_gap = card_gap * 2;
    const int card_width = std::max((workspace_width - total_card_gap) / 3, dpi.logical_to_pixels(160));
    const int cards_top = layout.workspace.top;
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const int x = layout.workspace.left + static_cast<int>(index) * (card_width + card_gap);
        layout.cards[index] = make_rect(x, cards_top, card_width, card_height);
        layout.card_titles[index] = make_rect(x, cards_top + gap, card_width, dpi.logical_to_pixels(24));
        layout.card_values[index] = make_rect(x, cards_top + dpi.logical_to_pixels(38), card_width, dpi.logical_to_pixels(46));
        const int badge_w = dpi.logical_to_pixels(120);
        const int badge_h = dpi.logical_to_pixels(30);
        layout.card_badges[index] = make_rect(x + (card_width - badge_w) / 2,
                                              layout.cards[index].bottom - gap - badge_h,
                                              badge_w,
                                              badge_h);
    }

    // Main content: note card + chart card split the remaining workspace height
    const int content_top_y = layout.cards[0].bottom + gap;
    const int content_height = std::max(static_cast<int>(layout.workspace.bottom - content_top_y), 0);
    const int note_height = content_height / 2;
    const int chart_height = content_height - gap - note_height;
    layout.note_card = make_rect(layout.workspace.left, content_top_y, workspace_width, note_height);
    layout.chart_card = make_rect(layout.workspace.left,
                                  layout.note_card.bottom + gap,
                                  workspace_width,
                                  std::max(chart_height, 0));

    // Inspector rows
    const int inspector_row_top = layout.inspector.top + dpi.logical_to_pixels(90);
    const int inspector_row_width = inspector_width - gap * 2;
    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        const int y = inspector_row_top + static_cast<int>(index) * (inspector_row_height + gap);
        layout.inspector_rows[index] = make_rect(layout.inspector.left + gap, y, inspector_row_width, inspector_row_height);
        const int tag_w = dpi.logical_to_pixels(54);
        const int tag_h = dpi.logical_to_pixels(24);
        layout.inspector_headers[index] = make_rect(layout.inspector_rows[index].left + small_gap,
                                                    y + small_gap,
                                                    inspector_row_width - small_gap * 2 - tag_w - small_gap,
                                                    tag_h);
        layout.inspector_tags[index] = make_rect(layout.inspector_rows[index].right - small_gap - tag_w,
                                                 y + small_gap,
                                                 tag_w,
                                                 tag_h);
        const int body_top = layout.inspector_headers[index].bottom + dpi.logical_to_pixels(4);
        const int body_bottom = layout.inspector_rows[index].bottom - small_gap;
        layout.inspector_bodies[index] = make_rect(layout.inspector_rows[index].left + small_gap,
                                                   body_top,
                                                   inspector_row_width - small_gap * 2,
                                                   std::max(body_bottom - body_top, 0));
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
        focus_index_ = 0;
        toggle_theme();
        return true;
    }

    if (PtInRect(&layout.notification, point) != FALSE) {
        focus_index_ = 6;
        return true;
    }

    if (PtInRect(&layout.search_box, point) != FALSE) {
        focus_index_ = 5;
        return true;
    }

    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        if (PtInRect(&layout.navigation[index], point) != FALSE) {
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
    // 0=toggle, 1..5=nav, 6=notification, 5 would conflict with nav so use 6 for notification
    if (index >= -1 && index <= 6) {
        focus_index_ = index;
    }
}

void ShowcaseView::cycle_focus(bool reverse) noexcept {
    constexpr int kCount = 7;  // 0=toggle, 1..5=nav, 6=notification
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
    if (focus_index_ >= 1 && focus_index_ <= 5) {
        const int nav_index = focus_index_ - 1;
        if (selected_navigation_ != nav_index) {
            selected_navigation_ = nav_index;
        }
        return true;
    }
    return focus_index_ == 6;
}

RECT ShowcaseView::focused_rect() const noexcept {
    RECT r{};
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    if (focus_index_ == 0) {
        r = layout.theme_toggle;
    } else if (focus_index_ >= 1 && focus_index_ <= 5) {
        r = layout.navigation[focus_index_ - 1];
    } else if (focus_index_ == 6) {
        r = layout.notification;
    }
    return r;
}

void ShowcaseView::paint(HDC hdc, nfui::FontCache& fonts) const noexcept {
    const ShowcasePalette palette = palette_for(theme_mode_);
    const ShowcaseLayout layout = build_layout(client_rect_, dpi_scale_);
    const int radius = dpi_scale_.logical_to_pixels(12);
    const int small_radius = dpi_scale_.logical_to_pixels(8);
    const int pill_radius = dpi_scale_.logical_to_pixels(12);
    const int tiny_radius = dpi_scale_.logical_to_pixels(4);
    const int gap = dpi_scale_.logical_to_pixels(16);
    const int small_gap = dpi_scale_.logical_to_pixels(12);
    const int icon_stroke = std::max(1, dpi_scale_.logical_to_pixels(2));

    fill_rect_color(hdc, client_rect_, palette.window);
    fill_rect_color(hdc, layout.sidebar, palette.sidebar);
    fill_rect_color(hdc, layout.divider_left, palette.border);
    fill_rect_color(hdc, layout.divider_right, palette.border);

    // Header card
    nfui::paint_drop_shadow(hdc, layout.header, radius, 1, palette.shadow);
    nfui::fill_rounded_rect(hdc, layout.header, radius, palette.header, palette.border);

    // Inspector panel
    nfui::paint_drop_shadow(hdc, layout.inspector, radius, 1, palette.shadow);
    nfui::fill_rounded_rect(hdc, layout.inspector, radius, palette.surface_alt, palette.border);

    // Branding
    draw_text_block(hdc,
                    layout.brand_title,
                    L"NativeFrame UI",
                    fonts,
                    dpi(),
                    FontWeight::serif,
                    nfui::font_pt::xl,
                    palette.accent,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    draw_text_block(hdc,
                    layout.brand_subtitle,
                    L"Modern Win32 showcase",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    nfui::font_pt::sm,
                    palette.muted_text,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Navigation
    for (std::size_t index = 0; index < layout.navigation.size(); ++index) {
        const bool selected = selected_navigation_ == static_cast<int>(index);
        if (selected) {
            const int bar_w = dpi_scale_.logical_to_pixels(2);
            const RECT bar = make_rect(layout.navigation[index].left,
                                       layout.navigation[index].top,
                                       bar_w,
                                       rect_height(layout.navigation[index]));
            fill_rect_color(hdc, bar, palette.accent);
            const RECT wash = make_rect(layout.navigation[index].left + bar_w,
                                        layout.navigation[index].top,
                                        rect_width(layout.navigation[index]) - bar_w,
                                        rect_height(layout.navigation[index]));
            nfui::fill_rounded_rect(hdc, wash, small_radius, palette.surface_alt, palette.surface_alt);
        }

        draw_nav_icon(hdc, layout.nav_icons[index], index,
                      selected ? palette.accent : palette.muted_text, icon_stroke);
        draw_text_block(hdc,
                        layout.nav_labels[index],
                        navigation_labels[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        nfui::font_pt::base,
                        selected ? palette.text : palette.muted_text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // Sidebar footer
    draw_pill(hdc, layout.version_pill, tiny_radius,
              palette.surface_alt, palette.border,
              L"v1.4.0", fonts, dpi(),
              FontWeight::semibold, nfui::font_pt::xs, palette.text);
    draw_avatar(hdc, layout.avatar, palette.accent, palette.accent_text,
                fonts, dpi(), palette.online);

    // Header content
    RECT title_area = make_rect(layout.header.left + gap,
                                layout.header.top + gap,
                                layout.search_box.left - layout.header.left - gap * 2,
                                dpi_scale_.logical_to_pixels(30));
    draw_text_block(hdc,
                    title_area,
                    L"Product Growth Showcase",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    nfui::font_pt::md,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    title_area.top += dpi_scale_.logical_to_pixels(30);
    title_area.bottom = title_area.top + dpi_scale_.logical_to_pixels(44);
    draw_text_block(hdc,
                    title_area,
                    L"Focused on evaluation-ready light and dark shells, explicit resources, and DPI-aware composition.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    nfui::font_pt::sm,
                    palette.muted_text,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);

    // Search box
    nfui::fill_rounded_rect(hdc, layout.search_box, small_radius, palette.surface, palette.border);
    const int search_icon_w = dpi_scale_.logical_to_pixels(18);
    const RECT search_icon_r = make_rect(layout.search_box.left + small_gap,
                                         layout.search_box.top + (rect_height(layout.search_box) - search_icon_w) / 2,
                                         search_icon_w,
                                         search_icon_w);
    draw_search_icon(hdc, search_icon_r, palette.muted_text, icon_stroke);
    RECT search_text = make_rect(search_icon_r.right + small_gap / 2,
                                 layout.search_box.top,
                                 layout.search_box.right - search_icon_r.right - small_gap * 3 / 2,
                                 rect_height(layout.search_box));
    draw_text_block(hdc,
                    search_text,
                    L"Search...",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    nfui::font_pt::sm,
                    palette.muted_text,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Notification button
    nfui::fill_rounded_rect(hdc, layout.notification, small_radius,
                            focus_index_ == 6 ? palette.accent_soft : palette.surface,
                            palette.border);
    const int bell_size = dpi_scale_.logical_to_pixels(20);
    const RECT bell_r = make_rect(layout.notification.left + (rect_width(layout.notification) - bell_size) / 2,
                                  layout.notification.top + (rect_height(layout.notification) - bell_size) / 2,
                                  bell_size,
                                  bell_size);
    draw_bell_icon(hdc, bell_r, palette.text, icon_stroke);

    // Theme toggle button
    nfui::fill_rounded_rect(hdc, layout.theme_toggle, small_radius,
                            focus_index_ == 0 ? palette.accent_soft : palette.surface,
                            palette.border);
    const int theme_icon_size = dpi_scale_.logical_to_pixels(20);
    const RECT theme_icon_r = make_rect(layout.theme_toggle.left + (rect_width(layout.theme_toggle) - theme_icon_size) / 2,
                                        layout.theme_toggle.top + (rect_height(layout.theme_toggle) - theme_icon_size) / 2,
                                        theme_icon_size,
                                        theme_icon_size);
    draw_theme_icon(hdc, theme_icon_r, theme_mode_, palette.text, icon_stroke);

    // KPI cards
    for (std::size_t index = 0; index < layout.cards.size(); ++index) {
        const bool hovered = hovered_card_ == static_cast<int>(index);
        const nfui::Color fill = hovered
                                     ? nfui::alpha_blend(palette.accent, palette.surface, 0.15f)
                                     : palette.surface;
        const nfui::Color border = hovered ? palette.accent : palette.border;
        nfui::paint_drop_shadow(hdc, layout.cards[index], small_radius, 1, palette.shadow);
        nfui::fill_rounded_rect(hdc, layout.cards[index], small_radius, fill, border);

        draw_text_block(hdc,
                        layout.card_titles[index],
                        card_titles[index],
                        fonts,
                        dpi(),
                        FontWeight::regular,
                        nfui::font_pt::sm,
                        palette.muted_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        draw_text_block(hdc,
                        layout.card_values[index],
                        card_values[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        nfui::font_pt::xl,
                        palette.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        nfui::Color badge_fill;
        nfui::Color badge_border;
        nfui::Color badge_text = palette.text;
        if (index == 0) {
            badge_fill = palette.success_soft;
            badge_border = palette.success;
        } else if (index == 1) {
            badge_fill = nfui::alpha_blend(palette.window, palette.success, 0.85f);
            badge_border = palette.success;
        } else {
            badge_fill = palette.accent_soft;
            badge_border = palette.accent;
        }
        draw_pill(hdc, layout.card_badges[index], pill_radius,
                  badge_fill, badge_border,
                  card_badges[index], fonts, dpi(),
                  FontWeight::semibold, nfui::font_pt::xs, badge_text);
    }

    // Note card
    nfui::paint_drop_shadow(hdc, layout.note_card, radius, 1, palette.shadow);
    nfui::fill_rounded_rect(hdc, layout.note_card, radius, palette.surface, palette.border);
    RECT note_title = make_rect(layout.note_card.left + gap,
                                layout.note_card.top + gap,
                                rect_width(layout.note_card) - gap * 2,
                                dpi_scale_.logical_to_pixels(30));
    draw_text_block(hdc,
                    note_title,
                    L"Showcase-only visuals",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    nfui::font_pt::md,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    RECT note_body = make_rect(layout.note_card.left + gap,
                               note_title.bottom + dpi_scale_.logical_to_pixels(4),
                               rect_width(layout.note_card) - gap * 2,
                               rect_height(layout.note_card) - gap - note_title.bottom + layout.note_card.top);
    draw_text_block(hdc,
                    note_body,
                    L"These visuals are for demonstration purposes, highlighting the capabilities of the NativeFrame UI system in a product context. They are not representative of the final product's full data or functionality.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    nfui::font_pt::xs,
                    palette.muted_text,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    // Chart card
    nfui::paint_drop_shadow(hdc, layout.chart_card, radius, 1, palette.shadow);
    nfui::fill_rounded_rect(hdc, layout.chart_card, radius, palette.surface, palette.border);
    RECT chart_title = make_rect(layout.chart_card.left + gap,
                                 layout.chart_card.top + gap,
                                 rect_width(layout.chart_card) - gap * 2,
                                 dpi_scale_.logical_to_pixels(26));
    draw_text_block(hdc,
                    chart_title,
                    L"Adoption trend",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    nfui::font_pt::md,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    const RECT chart_plot = make_rect(layout.chart_card.left + gap,
                                      chart_title.bottom + dpi_scale_.logical_to_pixels(4),
                                      rect_width(layout.chart_card) - gap * 2,
                                      rect_height(layout.chart_card) - gap - chart_title.bottom + layout.chart_card.top);
    constexpr std::array<float, 6> chart_values{40.0f, 55.0f, 45.0f, 70.0f, 85.0f, 65.0f};
    draw_bar_chart(hdc, chart_plot, small_radius, chart_values, palette.accent, palette.surface_alt, palette.border);

    // Inspector header
    RECT inspector_title = make_rect(layout.inspector.left + gap,
                                     layout.inspector.top + gap,
                                     rect_width(layout.inspector) - gap * 2,
                                     dpi_scale_.logical_to_pixels(30));
    draw_text_block(hdc,
                    inspector_title,
                    L"Inspector",
                    fonts,
                    dpi(),
                    FontWeight::semibold,
                    nfui::font_pt::md,
                    palette.text,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    inspector_title.top += dpi_scale_.logical_to_pixels(28);
    draw_text_block(hdc,
                    inspector_title,
                    L"Readable implementation notes for reviewers and release screenshots.",
                    fonts,
                    dpi(),
                    FontWeight::regular,
                    nfui::font_pt::sm,
                    palette.muted_text,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    // Inspector rows
    for (std::size_t index = 0; index < layout.inspector_rows.size(); ++index) {
        nfui::paint_drop_shadow(hdc, layout.inspector_rows[index], small_radius, 1, palette.shadow);
        nfui::fill_rounded_rect(hdc, layout.inspector_rows[index], small_radius, palette.surface, palette.border);
        draw_text_block(hdc,
                        layout.inspector_headers[index],
                        inspector_labels[index],
                        fonts,
                        dpi(),
                        FontWeight::semibold,
                        nfui::font_pt::sm,
                        palette.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        draw_pill(hdc, layout.inspector_tags[index], tiny_radius,
                  palette.surface_alt, palette.border,
                  inspector_tags[index], fonts, dpi(),
                  FontWeight::regular, nfui::font_pt::xs, palette.text);
        draw_text_block(hdc,
                        layout.inspector_bodies[index],
                        inspector_values[index],
                        fonts,
                        dpi(),
                        FontWeight::regular,
                        nfui::font_pt::xs,
                        palette.muted_text,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    // Focus ring (drawn last so it sits on top).
    if (focus_index_ >= 0) {
        const RECT focused = focused_rect();
        if (focused.right > focused.left && focused.bottom > focused.top) {
            nfui::paint_focus_border(hdc, focused, palette.accent, 2);
        }
    }
}

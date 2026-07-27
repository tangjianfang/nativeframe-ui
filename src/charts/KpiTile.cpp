// CP40: KPI tile implementation. Self-paints a card with a label/value/
// delta trio on the top + an owned Sparkline child at the bottom. All
// setters invalidate the relevant HWND without touching siblings.

#include <nfui/KpiTile.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <cstdio>

namespace nfui {

namespace {

constexpr const wchar_t* kKpiTileClassName = L"NativeFrameUIKpiTile";

[[nodiscard]] std::wstring delta_text(double delta) noexcept {
    // Direction is communicated via the glyph (▲ positive, ▼ negative,
    // — neutral) AND a signed number so meaning does not collapse when
    // the user is colorblind or the tile is printed in greyscale.
    wchar_t buf[32]{};
    if (delta > 0.0) {
        std::swprintf(buf, std::size(buf), L"\x25B2 +%.1f%%", delta);
    } else if (delta < 0.0) {
        std::swprintf(buf, std::size(buf), L"\x25BC %.1f%%", delta);
    } else {
        std::swprintf(buf, std::size(buf), L"\x2014 0.0%%");
    }
    return buf;
}

} // namespace

bool KpiTile::create(const WindowCreateParams& params) noexcept {
    WindowCreateParams owned = params;
    owned.class_name = kKpiTileClassName;
    if (!Window::create(owned)) return false;

    // Create the Sparkline child. We use a placeholder rect because
    // layout_sparkline() resizes it on WM_SIZE so it tracks the tile's
    // bottom band regardless of the tile's current size.
    WindowCreateParams spark{
        owned.instance,
        L"",
        L"",
        WS_CHILD | WS_VISIBLE,
        0,
        0, 0, 10, 10,
        hwnd(),
    };
    sparkline_created_ = sparkline_.create(spark);
    if (sparkline_created_) {
        if (palette_) sparkline_.set_palette(palette_);
        if (sparkline_color_) sparkline_.set_color(*sparkline_color_);
        layout_sparkline();
    }
    return true;
}

void KpiTile::set_label(std::wstring label) {
    label_ = std::move(label);
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void KpiTile::set_value(std::wstring value) {
    value_ = std::move(value);
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void KpiTile::set_delta_percent(double delta) noexcept {
    delta_percent_ = delta;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void KpiTile::set_sparkline(std::vector<ChartPoint> points, Color color) noexcept {
    sparkline_color_ = color;
    if (sparkline_created_) {
        sparkline_.set_color(color);
        sparkline_.set_points(std::move(points));
    } else {
        // Tile not yet created; the data survives in the auto-managed
        // Sparkline once create() finishes.
    }
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void KpiTile::inject_theme(const ThemePalette* palette, FontCache* fonts) noexcept {
    palette_ = palette;
    fonts_ = fonts;
    if (sparkline_created_) {
        if (palette_) sparkline_.set_palette(palette);
    }
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void KpiTile::layout_sparkline() noexcept {
    if (!sparkline_created_ || hwnd() == nullptr) return;
    RECT rc{};
    GetClientRect(hwnd(), &rc);
    // The sparkline lives in the lower half of the card with a small
    // inset so the rounded corner doesn't clip the AA endpoint.
    const int inset = 12;
    const int band_h = (rc.bottom - rc.top) * 4 / 10;
    const int spark_top = rc.bottom - inset - band_h;
    const int spark_bottom = rc.bottom - inset;
    const int spark_left = rc.left + inset;
    const int spark_right = rc.right - inset;
    if (spark_right > spark_left && spark_bottom > spark_top) {
        SetWindowPos(sparkline_.hwnd(), nullptr,
                     spark_left, spark_top,
                     spark_right - spark_left, spark_bottom - spark_top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

LRESULT KpiTile::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd(), &ps);
        RECT client{};
        GetClientRect(hwnd(), &client);
        on_paint(hdc, client);
        EndPaint(hwnd(), &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        layout_sparkline();
        return 0;
    default:
        break;
    }
    return Window::handle_message(message, wparam, lparam);
}

void KpiTile::on_paint(HDC hdc, const RECT& bounds) noexcept {
    const ThemePalette fallback = theme_palette(ThemeMode::light);
    const ThemePalette& pal = palette_ ? *palette_ : fallback;

    // 8 px inset so the elevation-2 drop shadow isn't clipped by the
    // tile HWND's edge.
    RECT card = bounds;
    InflateRect(&card, -8, -8);
    if (card.right <= card.left || card.bottom <= card.top) return;

    const ThemeMetrics metrics = theme_metrics();
    paint_drop_shadow(hdc, card, metrics.corner_radius_card, 2, pal.shadow);
    fill_rounded_rect(hdc, card, metrics.corner_radius_card, pal.surface, pal.border);

    const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
    HFONT label_font = (fonts_ != nullptr) ? fonts_->regular(dpi, font_pt::xs)
                                           : nullptr;
    HFONT value_font = (fonts_ != nullptr) ? fonts_->semibold(dpi, font_pt::lg)
                                           : nullptr;
    HFONT delta_font = (fonts_ != nullptr) ? fonts_->mono(dpi, font_pt::sm)
                                           : nullptr;

    // Label (top of the card, secondary text).
    RECT label_rc{card.left + 14, card.top + 10,
                  card.right - 14, card.top + 10 + 18};
    draw_text(hdc, label_rc, label_, label_font, pal.text_secondary,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Value (large, primary text — never a series color).
    RECT value_rc{card.left + 14, card.top + 32,
                  card.right - 14, card.top + 32 + 36};
    draw_text(hdc, value_rc, value_, value_font, pal.text,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Delta (just under the value; color + glyph carry the direction).
    const std::wstring delta = delta_text(delta_percent_);
    const Color delta_color =
        (delta_percent_ > 0.0) ? pal.success
        : (delta_percent_ < 0.0) ? pal.danger
        : pal.text_secondary;
    RECT delta_rc{card.left + 14, card.top + 70,
                  card.right - 14, card.top + 70 + 18};
    draw_text(hdc, delta_rc, delta, delta_font, delta_color,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
}

} // namespace nfui

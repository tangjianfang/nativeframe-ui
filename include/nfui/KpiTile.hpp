#pragma once

// CP40: dashboard KPI tile. A self-painted card that shows
//   label (small, secondary)
//   value (large, primary)
//   delta (% change vs. previous, with explicit ▲/▼ glyph + signed %)
//   sparkline (mini trendline at the bottom of the card)
//
// The tile owns one Sparkline child window. All setters invalidate
// just the affected HWND (the tile or its child) so a dashboard with
// many tiles doesn't trigger a full repaint when only one value moves.

#include <nfui/Font.hpp>
#include <nfui/Sparkline.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <optional>
#include <string>
#include <vector>

namespace nfui {

class KpiTile : public Window {
public:
    KpiTile() = default;
    ~KpiTile() override = default;

    KpiTile(const KpiTile&) = delete;
    KpiTile& operator=(const KpiTile&) = delete;
    KpiTile(KpiTile&&) = delete;
    KpiTile& operator=(KpiTile&&) = delete;

    [[nodiscard]] bool create(const WindowCreateParams& params) noexcept;

    void set_label(std::wstring label);
    void set_value(std::wstring value);
    // delta_percent > 0 -> ▲ +N.N%, < 0 -> ▼ -N.N%, == 0 -> — 0.0%.
    // Direction is communicated via the glyph AND sign so meaning does
    // not depend on color alone.
    void set_delta_percent(double delta) noexcept;
    // Optional sparkline. The supplied color overrides the tile's
    // default accent stroke; pass an empty points vector to clear.
    void set_sparkline(std::vector<ChartPoint> points, Color color) noexcept;

    // Bind the tile (and its Sparkline child) to an externally-owned
    // palette + FontCache so the demo can rewire all tiles with one
    // call when the theme changes.
    void inject_theme(const ThemePalette* palette, FontCache* fonts) noexcept;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    void layout_sparkline() noexcept;
    void on_paint(HDC hdc, const RECT& bounds) noexcept;

    std::wstring label_{};
    std::wstring value_{};
    double delta_percent_{};
    std::optional<Color> sparkline_color_{};
    const ThemePalette* palette_{};
    FontCache* fonts_{};
    Sparkline sparkline_{};
    bool sparkline_created_{false};
};

} // namespace nfui

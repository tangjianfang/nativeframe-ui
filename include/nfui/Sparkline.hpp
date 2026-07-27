#pragma once

// CP40: minimal line-only chart used inside KPI tiles and dashboard
// tiles. No axes, no grid, no legend, no tooltip, no markers, no focus
// chrome. Just a smooth antialiased polyline that draws from data
// points to pixels. The host owns the source data and rebinds via
// set_points() when a value changes.

#include <nfui/ChartInteraction.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <vector>

namespace nfui {

class Sparkline : public Window {
public:
    Sparkline() = default;
    ~Sparkline() override = default;

    Sparkline(const Sparkline&) = delete;
    Sparkline& operator=(const Sparkline&) = delete;
    Sparkline(Sparkline&&) = delete;
    Sparkline& operator=(Sparkline&&) = delete;

    [[nodiscard]] bool create(const WindowCreateParams& params) noexcept;

    // Replace the displayed points. Stored by move; an empty vector is
    // valid (clears the surface to the palette background and returns).
    void set_points(std::vector<ChartPoint> points);

    // Stroke color (theme palette or any COLORREF). Defaults to
    // palette.accent on creation so the tile adopts the active theme.
    void set_color(Color color) noexcept;

    // Optional pointer to an externally-owned palette. Sparkline paints
    // a fallback light palette when this is null; useful when the host
    // binds to a theme via inject_theme() and the sparkline is created
    // before that happens.
    void set_palette(const ThemePalette* palette) noexcept;

    // Stroke width in logical pixels. < 1 is clamped to 1.
    void set_line_width(int logical_px) noexcept;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    void on_paint(HDC hdc, const RECT& bounds) noexcept;

    std::vector<ChartPoint> points_{};
    Color color_{};
    const ThemePalette* palette_{};
    int line_width_px_{2};
};

} // namespace nfui

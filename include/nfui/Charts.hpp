#pragma once

#include <nfui/Font.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include <windows.h>

namespace nfui {

enum class ChartKind {
    bar_vertical,
    bar_horizontal,
    line,
    spline,
};

struct ChartPoint {
    double x{};
    double y{};
};

struct ChartSeries {
    std::wstring_view name;       // borrowed; non-owning
    Color color{};
    std::vector<ChartPoint> points;
};

struct ChartAxisRange {
    double min{};
    double max{};
    std::wstring_view label_format = L"{:.1f}";  // printf-style placeholders
};

struct ChartLayout {
    RECT plot_bounds{};       // pixel coords, filled by compute_chart_layout
    ChartAxisRange x{};
    ChartAxisRange y{};
    int legend_width_px = 120; // reserved for legend column
};

// Pure helpers (HWND-free, unit-testable):
[[nodiscard]] ChartLayout compute_chart_layout(RECT content_bounds,
                                              ChartKind kind,
                                              std::size_t series_count) noexcept;

[[nodiscard]] std::vector<POINT> normalize_points(const std::vector<ChartPoint>& points,
                                                  const ChartLayout& layout,
                                                  const ChartAxisRange& x,
                                                  const ChartAxisRange& y) noexcept;

[[nodiscard]] std::vector<RECT> compute_bar_geometry(const ChartLayout& layout,
                                                    std::size_t series_count,
                                                    std::size_t bar_count,
                                                    double gap_ratio = 0.2) noexcept;

// Catmull-Rom -> cubic Bezier control points for spline rendering.
// Returns 4*(n-1) POINTs (cubic segments share endpoints with neighbors).
[[nodiscard]] std::vector<POINT> catmull_rom_to_bezier(const std::vector<POINT>& points,
                                                      double tension = 0.5) noexcept;

// Self-painted HWND-based chart control. Receives ChartKind + ChartSeries data,
// renders into its own client area. C2 wires the control + paint cycle (MemoryDC
// scope-before-EndPaint, WM_ERASEBKGND suppression, WM_PRINTCLIENT for the future
// OCM_DRAWITEM path). Subclasses (C3 bar / C4 line+spline) override on_paint to
// call the appropriate renderer's draw routine.
//
// Threading: all members are touched only on the UI thread that owns the HWND.
// Data setters copy the inputs into the storage vectors; the caller is still
// responsible for keeping any std::wstring_view payloads (ChartSeries::name)
// alive until the next paint.
class ChartView : public Window {
public:
    ChartView() = default;
    ~ChartView() override = default;

    ChartView(const ChartView&) = delete;
    ChartView& operator=(const ChartView&) = delete;
    ChartView(ChartView&&) = delete;
    ChartView& operator=(ChartView&&) = delete;

    // Creates the chart window. Uses a unique window class ("NativeFrameUIChartView")
    // so Window::register_window_class can bind our window proc; the system "STATIC"
    // class is rejected by Window::register_window_class because its proc differs
    // from Window::window_proc. Real subclass registration will land in a later task.
    [[nodiscard]] bool create(const WindowCreateParams& params) noexcept;

    // Setters. Values are copied into the storage vectors; the caller still owns
    // any std::wstring_view payloads (ChartSeries::name) until the next paint.
    void set_kind(ChartKind kind) noexcept;
    void set_series(std::vector<ChartSeries> series) noexcept;
    void set_axis_x(ChartAxisRange axis) noexcept;
    void set_axis_y(ChartAxisRange axis) noexcept;
    void set_palette(const ThemePalette* palette) noexcept;
    void set_font_cache(FontCache* fonts) noexcept;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

    // Default paint: clear background and draw a placeholder frame (axes +
    // legend box + mono tick labels). Subclasses override per chart kind to
    // dispatch the appropriate renderer's draw routine.
    virtual void on_paint(HDC hdc, const RECT& bounds);

private:
    ChartKind kind_{ChartKind::line};
    std::vector<ChartSeries> series_{};
    ChartAxisRange axis_x_{0.0, 1.0};
    ChartAxisRange axis_y_{0.0, 1.0};
    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
};

} // namespace nfui

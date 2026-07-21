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

// Process-lifetime GDI+ bring-up for the chart line + spline renderers.
// Must be called BEFORE the first paint of a LineChartView / SplineChartView
// when antialiased strokes are desired; until then the renderers fall back to
// pure GDI. Idempotent — each successful initialize_chart_aa() must be paired
// with one shutdown_chart_aa(). No-op when GDI+ isn't available, so callers
// must not assume AA is enabled just because initialize_chart_aa() returned
// true (return value reflects successful startup, not success-on-a-given-HDC).
[[nodiscard]] bool initialize_chart_aa() noexcept;
void           shutdown_chart_aa() noexcept;

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

    // Storage and service pointers are protected so subclass renderers (C3 bar,
    // C4 line/spline) can read them directly inside their on_paint overrides.
    // Mutation stays exclusively via the public setters above.
    ChartKind kind_{ChartKind::line};
    std::vector<ChartSeries> series_{};
    ChartAxisRange axis_x_{0.0, 1.0};
    ChartAxisRange axis_y_{0.0, 1.0};
    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
};

// C3: vertical grouped or stacked bar chart renderer. Bars grow upward from the
// plot baseline. Default (set_stacked(false)): sub-bars for each x slot are placed
// side-by-side horizontally within that slot. set_stacked(true): each x slot's
// series segments pile vertically inside the slot, with column totals scaled
// against the max column sum (so a max-sized stack reaches plot_top) and the
// y-axis tick labels widened to cover that range.
class BarChartView : public ChartView {
public:
    BarChartView() = default;
    ~BarChartView() override = default;

    BarChartView(const BarChartView&) = delete;
    BarChartView& operator=(const BarChartView&) = delete;
    BarChartView(BarChartView&&) = delete;
    BarChartView& operator=(BarChartView&&) = delete;

    // When true, y values are summed column-wise and each x renders as a single
    // stacked bar (segments pile vertically, column totals scaled to the global
    // max). Default: false (grouped, sub-bars side-by-side).
    void set_stacked(bool stacked) noexcept;

protected:
    void on_paint(HDC hdc, const RECT& bounds) override;

private:
    bool stacked_ = false;
};

// C3: horizontal grouped or stacked bar chart renderer. Same data shape as
// BarChartView but the plot width/height are swapped via compute_chart_layout(
// bar_horizontal). Default (set_stacked(false)): sub-bars for each y slot
// subdivide the row vertically. set_stacked(true): each y slot's series
// segments tile horizontally inside the row, with row totals scaled against
// the max row sum (so a max-sized row reaches plot_right) and the x-axis tick
// labels widened to cover that range.
class HBarChartView : public ChartView {
public:
    HBarChartView() = default;
    ~HBarChartView() override = default;

    HBarChartView(const HBarChartView&) = delete;
    HBarChartView& operator=(const HBarChartView&) = delete;
    HBarChartView(HBarChartView&&) = delete;
    HBarChartView& operator=(HBarChartView&&) = delete;

    void set_stacked(bool stacked) noexcept;

protected:
    void on_paint(HDC hdc, const RECT& bounds) override;

private:
    bool stacked_ = false;
};

// C4: multi-series line chart renderer. One polyline per series, optionally
// punctuated by filled circle markers (point_radius_px_ > 0). Shares the C3
// plot frame + tick labels + legend column so multi-series line charts read
// the same as bar charts when shown side-by-side.
class LineChartView : public ChartView {
public:
    LineChartView() = default;
    ~LineChartView() override = default;

    LineChartView(const LineChartView&) = delete;
    LineChartView& operator=(const LineChartView&) = delete;
    LineChartView(LineChartView&&) = delete;
    LineChartView& operator=(LineChartView&&) = delete;

    // Marker radius in logical pixels. 0 disables markers so the line reads
    // as a clean unbroken stroke.
    void set_point_radius(int logical_px) noexcept;

protected:
    void on_paint(HDC hdc, const RECT& bounds) override;

private:
    int point_radius_px_ = 3;
};

// C4: multi-series smooth-curve chart renderer. Each polyline is converted
// to cubic Bezier control points via catmull_rom_to_bezier and drawn with
// GDI PolyBezier. Markers are intentionally omitted; the smooth curve and
// fixed markers fight each other and produce a muddy result.
class SplineChartView : public ChartView {
public:
    SplineChartView() = default;
    ~SplineChartView() override = default;

    SplineChartView(const SplineChartView&) = delete;
    SplineChartView& operator=(const SplineChartView&) = delete;
    SplineChartView(SplineChartView&&) = delete;
    SplineChartView& operator=(SplineChartView&&) = delete;

    // Catmull-Rom tension in [0, 1]; clamped. 0 collapses the curve to a
    // straight-line Catmull-Rom; 1 maximises the control-point offset.
    void set_tension(double t) noexcept;

protected:
    void on_paint(HDC hdc, const RECT& bounds) override;

private:
    double tension_ = 0.5;
};

} // namespace nfui

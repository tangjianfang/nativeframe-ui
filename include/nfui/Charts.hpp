#pragma once

#include <nfui/ChartInteraction.hpp>
#include <nfui/Font.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Window.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

namespace nfui {

class ChartGroup;  // CP40: declared in <nfui/ChartGroup.hpp>; friended below.

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
    area,
};

// ChartPoint and ChartAxisRange are defined in ChartInteraction.hpp
// (included above) to avoid circular header dependencies.

// A single data series on a chart. `name` is a borrowed string view (the
// caller is responsible for keeping the underlying string alive while the
// series is in use; ChartView does not copy it). `color` is used for the
// line/area/bar fill, the legend swatch, and the point markers. Set
// `visible = false` to exclude the series from rendering, hit-testing,
// statistics, and the legend column; toggle at runtime via
// ChartView::set_series_visible(idx, bool). Invisible series still occupy
// their slot in `set_series()` — the index is stable across visibility
// changes so callers can pair the setter with a checkbox state.
struct ChartSeries {
    std::wstring_view name;       // borrowed; non-owning
    Color color{};
    std::vector<ChartPoint> points;
    bool visible{true};
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
                                              std::size_t series_count,
                                              int y_label_w_px,
                                              int x_label_h_px) noexcept;

// Convenience overload that uses the fixed default gutter sizes.
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

// Formats a double using a Rust-style placeholder (L"{:.Nf}"|L"{:.N%}"} where N is
// an optional decimal digit count). Supported forms: L"{:.0f}", L"{:.1f}",
// L"{:.2f}", L"{:.0%}", L"{:.1%}", L"{:.2%}". Anything else falls back to L"{:.0f}"
// so a malformed label_format never panics out of on_paint.
[[nodiscard]] std::wstring format_axis_tick(double value,
                                            std::wstring_view label_format) noexcept;

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
    ChartView();
    ~ChartView() override;  // Defined in ChartInteraction.cpp (InteractionImpl is incomplete here).

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

    // Per-series visibility (CP39 — series checklist). Hidden series are
    // skipped by every renderer (line/spline/area/bar/hbar) and excluded
    // from the legend column, stacked-bar/hbar column/row sums, and the
    // interaction hit-test/hover/drag/range-select pipelines. `idx` is
    // the slot in the `set_series()` vector; out-of-range calls are
    // ignored silently so the host can pair the setter with a checkbox
    // state without bounding checks.
    void set_series_visible(std::size_t idx, bool visible) noexcept;
    [[nodiscard]] bool is_series_visible(std::size_t idx) const noexcept;
    [[nodiscard]] std::size_t series_count() const noexcept { return series_.size(); }

    // CP39: chart-level settings (theme, kind, overlay toggles, animation
    // duration, hit tolerance). The interaction-owned fields
    // (animation_ms, hit_tolerance_px, show_crosshair, show_tooltip) are
    // forwarded into the active ChartInteractionOptions when interaction
    // is enabled. Out-of-range fields are clamped (animation_ms <= 5000,
    // hit_tolerance_px in [1,64]). When any field changes, the
    // ChartCallbacks::on_settings_changed callback (if set) is invoked.
    //
    // CP40: not noexcept — the settings struct now owns std::wstring
    // axis labels, so the copy-in / copy-out may allocate. Allocation
    // failures must propagate rather than terminate.
    void apply_settings(ChartSettings settings);
    [[nodiscard]] ChartSettings settings() const;

    // CP40: export the chart's client surface to a raster file. Returns
    // false for a null, hidden, zero-sized, uncapturable, or unencodable
    // HWND. Both methods reuse the existing WM_PRINTCLIENT path so the
    // exported image matches the on-screen render exactly (including the
    // interaction overlay). PNG uses 32bpp BGRA with opaque alpha; BMP
    // uses 32bpp BGR.
    [[nodiscard]] bool export_to_png(const std::wstring& path) const noexcept;
    [[nodiscard]] bool export_to_bmp(const std::wstring& path) const noexcept;
    // CP34: hide the in-renderer legend column when the host draws its
    // own (e.g. NativeFrameUICharts paints a per-card footer legend). The
    // default stays true so existing call sites that don't pass this
    // continue to get the chart's own legend.
    void set_show_legend(bool enabled) noexcept { show_legend_ = enabled; }

    // Bind both palette + FontCache in one call. Equivalent to:
    //   set_palette(palette);
    //   set_font_cache(fonts);
    void inject_theme(const ThemePalette* palette, FontCache* fonts) noexcept {
        palette_ = palette;
        fonts_ = fonts;
    }

    // --- Interaction API (opt-in; default is disabled) -----------------------

    // Enables the interaction controller with the given options.
    void enable_interaction(ChartInteractionOptions opts) noexcept;
    void disable_interaction() noexcept;
    [[nodiscard]] bool interaction_enabled() const noexcept;

    void set_callbacks(ChartCallbacks callbacks);

    // View range management (zoom/pan).
    void set_visible_range(ChartAxisRange x, ChartAxisRange y) noexcept;
    void reset_view() noexcept;
    [[nodiscard]] ChartAxisRange visible_x() const noexcept;
    [[nodiscard]] ChartAxisRange visible_y() const noexcept;

    // Undo/redo for drag-edit operations.
    void undo() noexcept;
    void redo() noexcept;
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;

    // Selection access.
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> selected_points() const;
    void clear_selection() noexcept;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

    // Default paint: clear background and draw a placeholder frame (axes +
    // legend box + mono tick labels). Subclasses override per chart kind to
    // dispatch the appropriate renderer's draw routine.
    virtual void on_paint(HDC hdc, const RECT& bounds);

    // Called at the end of on_paint to draw interaction overlays (crosshair,
    // rubber-band, tooltip, drag highlight, selection rings). Subclasses
    // call this from their on_paint overrides after rendering chart data.
    void paint_interaction_overlay(HDC hdc, const RECT& bounds);

    // Storage and service pointers are protected so subclass renderers (C3 bar,
    // C4 line/spline) can read them directly inside their on_paint overrides.
    // Mutation stays exclusively via the public setters above.
    ChartKind kind_{ChartKind::line};
    std::vector<ChartSeries> series_{};
    ChartAxisRange axis_x_{0.0, 1.0};
    ChartAxisRange axis_y_{0.0, 1.0};
    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
    bool show_legend_{true};
    ChartSettings settings_{};  // CP39: applied via apply_settings()

    // Cached palette resolved from settings_.theme when palette_ is null.
    // Refreshed by apply_settings(); renderers read it via
    // effective_palette() so the lifetime is the chart's own.
    ThemePalette fallback_palette_{};

    // Returns the current layout for the given bounds (used by interaction).
    [[nodiscard]] ChartLayout current_layout(const RECT& bounds) const noexcept;

    // CP39: central palette resolver. Prefers the host-supplied
    // palette_ pointer; falls back to the theme dictated by
    // ChartSettings::theme. Renderers call this so a chart configured
    // for dark mode still paints dark when the host forgot to rebind.
    [[nodiscard]] const ThemePalette& effective_palette() const noexcept;

    // CP39: refresh the cached fallback palette from settings_.theme.
    // apply_settings() calls this; tests / hosts that change settings_
    // directly can also call it before the next paint.
    void refresh_fallback_palette() noexcept;

    // Returns the axis range that should drive rendering right now. When
    // interaction is enabled this is the zoomed/panned view range; otherwise
    // it is the user-configured axis. Renderers (and the interaction overlay)
    // must use these so they stay in sync.
    [[nodiscard]] ChartAxisRange effective_axis_x() const noexcept;
    [[nodiscard]] ChartAxisRange effective_axis_y() const noexcept;

private:
    // Interaction message handlers (implemented in ChartInteraction.cpp).
    LRESULT handle_interaction_message(UINT message, WPARAM wparam, LPARAM lparam);
    void on_mouse_move_interaction(POINT cursor);
    void on_lbutton_down_interaction(POINT cursor);
    void on_lbutton_up_interaction(POINT cursor);
    void on_mbutton_down_interaction(POINT cursor);
    void on_mbutton_up_interaction(POINT cursor);
    void on_mouse_wheel_interaction(POINT cursor, short delta);
    void on_lbutton_dblclk_interaction(POINT cursor);
    // Returns true iff the keystroke was consumed by a known binding.
    [[nodiscard]] bool on_keydown_interaction(WPARAM vkey);
    void on_capture_changed_interaction();
    void on_tooltip_timer();
    void on_animation_timer();

    // CP40: ChartGroup coordination hooks. ChartGroup calls these to
    // subscribe to view-range and cursor changes, to push an external
    // cursor x value, and to detach before the group is destroyed. They
    // are private so only the friend class ChartGroup can invoke them.
    friend class ChartGroup;
    void set_group_observers(
        std::function<void(ChartAxisRange, ChartAxisRange)> on_view,
        std::function<void(std::optional<double>)> on_cursor) noexcept;
    void clear_group_observers() noexcept;
    void set_external_cursor_x(std::optional<double> x) noexcept;

    // Interaction state (heap-allocated to keep Charts.hpp include-light;
    // nullptr when interaction is disabled).
    struct InteractionImpl;
    std::unique_ptr<InteractionImpl> interaction_{};
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
// GDI PolyBezier. Small markers are drawn at the sample anchors (CP30) so
// readers can see where the curve actually interpolates.
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

// CP14: filled-area chart renderer. One filled polygon per series spanning
// the line and the plot baseline (y == y.min). Uses the same data shape as
// LineChartView (normalized points + axis ranges) so a caller can swap a
// line view for an area view by changing only the subclass. The fill is a
// translucent vertical gradient blended from the series color toward
// palette.surface so stacked area charts stay distinguishable. An optional
// outline (stroke in the original series color) is drawn on top.
class AreaChartView : public ChartView {
public:
    AreaChartView() = default;
    ~AreaChartView() override = default;

    AreaChartView(const AreaChartView&) = delete;
    AreaChartView& operator=(const AreaChartView&) = delete;
    AreaChartView(AreaChartView&&) = delete;
    AreaChartView& operator=(AreaChartView&&) = delete;

    // When true (default), a 1-px stroke in the original series color is
    // drawn along the upper edge of the filled polygon so the boundary
    // reads cleanly against busy backgrounds. Set false for a pure-fill
    // area chart (useful when stacking many series).
    void set_outline(bool enabled) noexcept;

    // Fill alpha in [0, 1]; clamped. 1 is fully opaque series color; 0
    // produces an invisible fill (the outline, if enabled, still draws).
    // Intermediate values fade the gradient toward palette.surface.
    void set_fill_alpha(double alpha) noexcept;

protected:
    void on_paint(HDC hdc, const RECT& bounds) override;

private:
    bool outline_ = true;
    double fill_alpha_ = 1.0;
};

} // namespace nfui

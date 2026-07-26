#pragma once

// Public API for the chart interaction system. Defines interaction modes,
// hit-test results, configuration options, and the callback interface that
// ChartView uses to notify the host application of user interactions.
//
// Interaction is opt-in: ChartView defaults to mode_flags == 0 (no interaction)
// so existing samples and consumers are unaffected until enable_interaction()
// is called with a non-zero configuration.
//
// This header is the canonical definition site for ChartPoint and
// ChartAxisRange (fundamental data types shared by both the chart renderers
// and the interaction system). Charts.hpp includes this header.

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

namespace nfui {

// Fundamental chart data types (defined here to avoid circular includes).
struct ChartPoint {
    double x{};
    double y{};
};

struct ChartAxisRange {
    double min{};
    double max{};
    std::wstring_view label_format = L"{:.0f}";  // printf-style placeholders
};

// Interaction modes. Combine via bitwise OR in ChartInteractionOptions::mode_flags.
enum class ChartInteractionMode : unsigned {
    none         = 0,
    select       = 1u << 0,  // Click to select data points.
    drag_edit    = 1u << 1,  // Drag data points to modify values.
    range_select = 1u << 2,  // Rubber-band rectangle to select a data range.
    pan          = 1u << 3,  // Middle-button or Space+Left drag to pan.
    zoom         = 1u << 4,  // Mouse wheel to zoom; double-click to reset.
};

[[nodiscard]] inline constexpr unsigned operator|(ChartInteractionMode a,
                                                   ChartInteractionMode b) noexcept {
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

[[nodiscard]] inline constexpr unsigned operator|(unsigned a,
                                                   ChartInteractionMode b) noexcept {
    return a | static_cast<unsigned>(b);
}

inline constexpr bool has_mode(unsigned flags, ChartInteractionMode m) noexcept {
    return (flags & static_cast<unsigned>(m)) != 0;
}

// Result of a hit-test against chart data points.
struct ChartHitResult {
    bool hit{false};
    std::size_t series_index{0};
    std::size_t point_index{0};
    ChartPoint data_point{};   // Data-space coordinates of the hit point.
    POINT pixel_point{};       // Pixel-space position of the data point.
    double distance_px{0.0};   // Euclidean distance from cursor to point (px).
};

// CP39: chart-level presentation settings (separate from data and from
// the interaction controller). Hosts the settings dialog (see
// samples/NativeFrameUIChartsInteractive) writes a ChartSettings and
// hands it to ChartView::apply_settings(); every field is read on the
// next paint, so apply_settings() is safe to call from any UI-thread
// spot. Defaults mirror the prior hard-coded behaviour so existing
// call sites are unaffected until they opt in.
//
// `kind_id` mirrors `ChartKind` from Charts.hpp but is stored as int
// here to avoid a circular header dependency (ChartInteraction.hpp is
// included by Charts.hpp). ChartView::apply_settings() narrows it back
// to ChartKind; an unknown value is treated as line.
struct ChartSettings {
    enum class ThemeMode { light, dark };

    ThemeMode theme{ThemeMode::light};
    // Chart kind identifier. Stored as int to avoid the circular
    // Charts.hpp / ChartInteraction.hpp dependency. Mapping:
    //   0 = ChartKind::bar_vertical
    //   1 = ChartKind::bar_horizontal
    //   2 = ChartKind::line
    //   3 = ChartKind::spline   (default)
    //   4 = ChartKind::area
    // Unknown values are coerced to ChartKind::line by apply_settings().
    int kind_id{3};

    // Overlay toggles. show_crosshair and show_tooltip require the
    // interaction controller (the overlay is drawn in
    // paint_interaction_overlay).
    bool show_crosshair{true};
    bool show_tooltip{true};
    bool show_legend{true};

    // Animation duration for zoom/pan/reset transitions. 0 disables
    // animation (instant snap). Capped at 5000 ms by apply_settings()
    // so a malicious dialog can't lock the UI thread.
    unsigned animation_ms{200};

    // Pixel tolerance for data-point hit testing. 1..64; values outside
    // the range are clamped on apply.
    int hit_tolerance_px{8};

    // CP40: axis titles drawn beneath the plot (x) and inside the
    // left gutter (y). Owned strings so the host can hand them straight
    // off a std::wstring without lifetime juggling. An empty label
    // suppresses the corresponding title.
    std::wstring x_axis_label{ L"Time" };
    std::wstring y_axis_label{ L"Value" };
};

// Per-series statistics for a completed rubber-band range selection. Only
// series that contain at least one point inside the selection appear in
// ChartRangeSelection::series_stats; series with zero hits are omitted so the
// caller can iterate without filtering.
struct ChartSeriesStats {
    std::size_t series_index{0};
    std::size_t count{0};
    double min_x{0.0};
    double max_x{0.0};
    double min_y{0.0};
    double max_y{0.0};
    double mean_y{0.0};
    double sum_y{0.0};
};

// Describes a completed rubber-band range selection. Carries both the raw
// extents and a per-series statistical summary so host code can show the same
// numbers in its own UI (status bar, properties panel, etc.) without having
// to recompute them.
struct ChartRangeSelection {
    ChartPoint origin{};       // Data-space start (top-left in data coords).
    ChartPoint extent{};       // Data-space end (bottom-right in data coords).
    RECT pixel_rect{};         // Pixel rectangle as drawn on screen.
    std::vector<ChartSeriesStats> series_stats{};  // one per series with hits
    std::size_t total_count{0};                    // sum of series_stats[].count
    double min_y{0.0};                            // overall y minimum
    double max_y{0.0};                            // overall y maximum
    double mean_y{0.0};                           // overall y mean (0 if empty)
};

// Configuration for the chart interaction controller.
struct ChartInteractionOptions {
    unsigned mode_flags{0};        // Bitwise OR of ChartInteractionMode values.
    int hit_tolerance_px{8};       // Max cursor-to-point distance for a hit.
    bool edit_x{false};            // Allow dragging to modify the x coordinate.
    bool edit_y{true};             // Allow dragging to modify the y coordinate.
    bool zoom_x{true};             // Wheel zoom affects the x axis.
    bool zoom_y{true};             // Wheel zoom affects the y axis.
    bool show_crosshair{true};     // Draw crosshair lines on hover.
    bool show_tooltip{true};       // Draw data tooltip on point hover.
    bool animate_on_edit{true};    // Animate transitions after edits.
    unsigned animation_ms{200};    // Duration of edit/zoom animations.
    unsigned undo_depth{64};       // Maximum undo stack depth.
};

// Callback interface. The host sets individual std::function members; unset
// callbacks are simply not invoked. All callbacks fire on the UI thread that
// owns the chart HWND, and never while an internal lock is held.
struct ChartCallbacks {
    // A data point was edited via drag (series_idx, point_idx, new_value).
    std::function<void(std::size_t, std::size_t, ChartPoint)> on_point_edited;

    // Rubber-band range selection completed.
    std::function<void(ChartRangeSelection)> on_range_selected;

    // Visible axis range changed (zoom/pan). Args: new x range, new y range.
    std::function<void(ChartAxisRange, ChartAxisRange)> on_view_changed;

    // Hover state changed. hit==false means the cursor left all points.
    std::function<void(ChartHitResult)> on_hover_changed;

    // CP40: cursor moved in data-space terms. Fires with a value when the
    // cursor is inside the plot area; fires with std::nullopt when it
    // leaves. ChartGroup uses this to synchronize a vertical crosshair
    // across linked charts.
    std::function<void(std::optional<double>)> on_cursor_x_changed;

    // Selection set changed. Vector of (series_index, point_index) pairs.
    std::function<void(std::vector<std::pair<std::size_t, std::size_t>>)> on_selection_changed;

    // View was reset to the home range (double-click).
    std::function<void()> on_view_reset;

    // CP39: ChartSettings changed via apply_settings(). The host reads
    // the new values from chart.settings() (e.g. to rewire its palette
    // for theme changes, or rebuild controls for kind changes).
    std::function<void(ChartSettings)> on_settings_changed;
};

// A single undoable/redoable edit command.
struct ChartEditCommand {
    std::size_t series_index{0};
    std::size_t point_index{0};
    ChartPoint old_value{};
    ChartPoint new_value{};
};

} // namespace nfui

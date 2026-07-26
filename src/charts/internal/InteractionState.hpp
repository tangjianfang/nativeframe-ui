#pragma once

// Internal interaction state machine for the chart interaction controller.
// Tracks the current interaction phase and validates state transitions.
// HWND-free; driven by ChartView's message handler.

#include <nfui/ChartInteraction.hpp>

#include <cstddef>

#include <windows.h>

namespace nfui::charts_internal {

// The interaction phases. Transitions are validated by transition().
enum class InteractionPhase {
    idle,           // No active interaction.
    hover,          // Cursor is over a data point (highlight active).
    drag_point,     // Dragging a data point to edit its value.
    range_select,   // Drawing a rubber-band selection rectangle.
    pan,            // Panning the visible axis range.
};

// Mutable state for the interaction controller. Owned by ChartView;
// all access is on the UI thread.
struct InteractionState {
    InteractionPhase phase{InteractionPhase::idle};

    // The data point being dragged (valid during drag_point).
    std::size_t drag_series{0};
    std::size_t drag_point{0};
    ChartPoint drag_original{};   // Value before the drag started.

    // Rubber-band selection state (valid during range_select).
    POINT select_origin_px{};     // Pixel where the drag started.
    POINT select_current_px{};    // Current pixel during drag.

    // Pan state (valid during pan).
    POINT pan_origin_px{};        // Pixel where pan started.
    ChartAxisRange pan_start_x{}; // Axis range at pan start.
    ChartAxisRange pan_start_y{};

    // Current hover hit (valid during hover and idle-with-crosshair).
    ChartHitResult hover_hit{};

    // Current cursor position in pixels (updated on every WM_MOUSEMOVE).
    POINT cursor_px{};
    bool cursor_in_plot{false};

    // Tooltip delay tracking.
    unsigned long long hover_start_ms{0};
    bool tooltip_visible{false};

    // Floating range-summary card state. Set when a rubber-band completes;
    // cleared when the next interaction starts. The card is anchored to
    // range_summary_anchor_px (the rubber-band pixel rect) and the stats
    // vectors mirror the data that the host receives via on_range_selected.
    bool range_summary_visible{false};
    RECT range_summary_anchor_px{};
    std::vector<ChartSeriesStats> range_summary_stats{};
    std::size_t range_summary_total{0};
    double range_summary_min_x{0.0};
    double range_summary_max_x{0.0};
    double range_summary_min_y{0.0};
    double range_summary_max_y{0.0};
    double range_summary_mean_y{0.0};
};

// Attempts a state transition. Returns true if the transition is valid
// and updates `state.phase`. Returns false for illegal transitions
// (state is unchanged).
[[nodiscard]] bool transition(InteractionState& state,
                              InteractionPhase to) noexcept;

// Resets the state machine to idle, clearing all transient fields.
void reset_to_idle(InteractionState& state) noexcept;

// Begins a point-drag operation.
void begin_drag(InteractionState& state,
                std::size_t series_idx,
                std::size_t point_idx,
                ChartPoint original) noexcept;

// Begins a rubber-band range selection.
void begin_range_select(InteractionState& state, POINT origin_px) noexcept;

// Begins a pan operation.
void begin_pan(InteractionState& state,
               POINT origin_px,
               ChartAxisRange current_x,
               ChartAxisRange current_y) noexcept;

} // namespace nfui::charts_internal

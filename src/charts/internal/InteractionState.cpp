#include "InteractionState.hpp"

namespace nfui::charts_internal {

namespace {

// Validates whether a transition from `from` to `to` is legal.
[[nodiscard]] bool is_valid_transition(InteractionPhase from,
                                       InteractionPhase to) noexcept {
    switch (from) {
    case InteractionPhase::idle:
        // From idle we can enter any active state.
        return to == InteractionPhase::hover ||
               to == InteractionPhase::drag_point ||
               to == InteractionPhase::range_select ||
               to == InteractionPhase::pan;

    case InteractionPhase::hover:
        // From hover we can start a drag, start a selection, or go idle.
        return to == InteractionPhase::idle ||
               to == InteractionPhase::drag_point ||
               to == InteractionPhase::range_select ||
               to == InteractionPhase::pan;

    case InteractionPhase::drag_point:
        // Drag can only end back to idle (or hover if cursor still on point).
        return to == InteractionPhase::idle ||
               to == InteractionPhase::hover;

    case InteractionPhase::range_select:
        // Selection can only end back to idle.
        return to == InteractionPhase::idle;

    case InteractionPhase::pan:
        // Pan can only end back to idle.
        return to == InteractionPhase::idle;
    }
    return false;
}

} // namespace

bool transition(InteractionState& state, InteractionPhase to) noexcept {
    if (state.phase == to) {
        return true;  // No-op transition is always valid.
    }
    if (!is_valid_transition(state.phase, to)) {
        return false;
    }
    state.phase = to;
    return true;
}

void reset_to_idle(InteractionState& state) noexcept {
    state.phase = InteractionPhase::idle;
    state.drag_series = 0;
    state.drag_point = 0;
    state.drag_original = ChartPoint{};
    state.select_origin_px = POINT{};
    state.select_current_px = POINT{};
    state.pan_origin_px = POINT{};
    state.pan_start_x = ChartAxisRange{};
    state.pan_start_y = ChartAxisRange{};
    state.tooltip_visible = false;
    // Any in-flight operation invalidates the previous range summary;
    // commit() on the next rubber-band release will re-populate it.
    state.range_summary_visible = false;
    state.range_summary_stats.clear();
    state.range_summary_total = 0;
    state.range_summary_min_x = 0.0;
    state.range_summary_max_x = 0.0;
    state.range_summary_min_y = 0.0;
    state.range_summary_max_y = 0.0;
    state.range_summary_mean_y = 0.0;
}

void begin_drag(InteractionState& state,
                std::size_t series_idx,
                std::size_t point_idx,
                ChartPoint original) noexcept {
    state.phase = InteractionPhase::drag_point;
    state.drag_series = series_idx;
    state.drag_point = point_idx;
    state.drag_original = original;
    state.tooltip_visible = false;
}

void begin_range_select(InteractionState& state, POINT origin_px) noexcept {
    state.phase = InteractionPhase::range_select;
    state.select_origin_px = origin_px;
    state.select_current_px = origin_px;
    state.tooltip_visible = false;
}

void begin_pan(InteractionState& state,
               POINT origin_px,
               ChartAxisRange current_x,
               ChartAxisRange current_y) noexcept {
    state.phase = InteractionPhase::pan;
    state.pan_origin_px = origin_px;
    state.pan_start_x = current_x;
    state.pan_start_y = current_y;
    state.tooltip_visible = false;
}

} // namespace nfui::charts_internal

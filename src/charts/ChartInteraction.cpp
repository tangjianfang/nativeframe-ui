// ChartView interaction controller implementation. Handles mouse/keyboard
// messages, drives the interaction state machine, and renders overlays
// (crosshair, rubber-band, tooltip, drag highlight, selection rings).
//
// All code runs on the UI thread that owns the chart HWND. Callbacks fire
// after internal state is committed and no lock is held.

#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

#include "internal/ChartAnimator.hpp"
#include "internal/HitTest.hpp"
#include "internal/InteractionState.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include <windowsx.h>

namespace nfui {

// Timer IDs used by the interaction controller.
namespace {
constexpr UINT_PTR kTimerTooltip = 0x4301;
constexpr UINT_PTR kTimerAnimation = 0x4302;
constexpr UINT kTooltipDelayMs = 300;
constexpr UINT kAnimationIntervalMs = 16;
constexpr double kZoomFactor = 1.15;

// Linearly interpolate y for the given x across a monotonic series.
[[nodiscard]] double interpolate_series_value(
    const std::vector<ChartPoint>& points, double x) noexcept {
    if (points.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (x <= points.front().x) {
        return points.front().y;
    }
    if (x >= points.back().x) {
        return points.back().y;
    }
    auto it = std::lower_bound(points.begin(), points.end(), x,
        [](const ChartPoint& p, double value) { return p.x < value; });
    if (it == points.begin()) {
        return it->y;
    }
    const auto prev = it - 1;
    const double dx = it->x - prev->x;
    if (dx == 0.0) {
        return prev->y;
    }
    const double t = (x - prev->x) / dx;
    return prev->y + t * (it->y - prev->y);
}
} // namespace

// PIMPL-style internal state. Heap-allocated so Charts.hpp stays light.
struct ChartView::InteractionImpl {
    ChartInteractionOptions opts{};
    ChartCallbacks callbacks{};
    charts_internal::InteractionState state{};

    // CP40: ChartGroup observer hooks. Both are set/cleared by the
    // private friend hooks in Charts.hpp; when non-empty they get
    // notified in addition to the host's on_view_changed / on_cursor_x_changed
    // callbacks (ChartGroup uses these to synchronize linked charts).
    std::function<void(ChartAxisRange, ChartAxisRange)> group_view_observer{};
    std::function<void(std::optional<double>)>        group_cursor_observer{};

    // CP40: external cursor x value. set_external_cursor_x stores a value
    // (or clears) and invalidates; on_paint draws a vertical crosshair
    // using the chart's own y scale without forwarding the value back to
    // the group (avoids feedback loops).
    std::optional<double> external_cursor_x{};

    // View range (current visible + home for reset).
    ChartAxisRange view_x{};
    ChartAxisRange view_y{};
    ChartAxisRange home_x{};
    ChartAxisRange home_y{};
    bool view_initialized{false};
    // Set when reset_view initiates an animation; cleared by on_animation_timer
    // when the axis animation settles, at which point on_view_reset fires.
    bool reset_pending{false};

    // Selection set: (series_index, point_index).
    std::set<std::pair<std::size_t, std::size_t>> selection{};

    // Undo/redo stacks.
    std::vector<ChartEditCommand> undo_stack{};
    std::vector<ChartEditCommand> redo_stack{};

    // Animation state.
    charts_internal::AxisAnimation axis_anim{};
    Win32Clock clock{};
};

// --- Lifecycle ---------------------------------------------------------------

ChartView::ChartView() = default;
ChartView::~ChartView() = default;

void ChartView::enable_interaction(ChartInteractionOptions opts) noexcept {
    if (!interaction_) {
        interaction_ = std::make_unique<InteractionImpl>();
    }
    interaction_->opts = opts;
    // Initialize view ranges from current axes if not yet done.
    if (!interaction_->view_initialized) {
        interaction_->view_x = axis_x_;
        interaction_->view_y = axis_y_;
        interaction_->home_x = axis_x_;
        interaction_->home_y = axis_y_;
        interaction_->view_initialized = true;
    }
}

void ChartView::disable_interaction() noexcept {
    if (interaction_ && hwnd()) {
        KillTimer(hwnd(), kTimerTooltip);
        KillTimer(hwnd(), kTimerAnimation);
        if (GetCapture() == hwnd()) {
            ReleaseCapture();
        }
    }
    // Drop the interaction state so the next enable_interaction() re-snapshots
    // the current axis_x_ / axis_y_ into home_x / home_y. Otherwise a host
    // that adjusts the axes while interaction is off would see stale home
    // ranges on re-enable, and reset_view() would jump back to the wrong spot.
    interaction_.reset();
}

bool ChartView::interaction_enabled() const noexcept {
    return interaction_ != nullptr && interaction_->opts.mode_flags != 0;
}

void ChartView::set_callbacks(ChartCallbacks callbacks) {
    if (!interaction_) {
        interaction_ = std::make_unique<InteractionImpl>();
    }
    interaction_->callbacks = std::move(callbacks);
}

// --- CP40: ChartGroup observer hooks + cursor broadcast ----------------------

void ChartView::set_group_observers(
    std::function<void(ChartAxisRange, ChartAxisRange)> on_view,
    std::function<void(std::optional<double>)> on_cursor) noexcept {
    if (!interaction_) {
        // CP40: ChartGroup can attach to a chart that never had
        // enable_interaction() called. Materialise InteractionImpl here
        // and seed the view + home ranges from the chart's own axes so
        // effective_axis_y() / effective_axis_x() return the configured
        // values instead of the default (0,1). Without this seed, a
        // group-attached chart would paint its first frame with all
        // data clamped to the bottom-left corner.
        interaction_ = std::make_unique<InteractionImpl>();
        interaction_->view_x = axis_x_;
        interaction_->view_y = axis_y_;
        interaction_->home_x = axis_x_;
        interaction_->home_y = axis_y_;
        interaction_->view_initialized = true;
    }
    interaction_->group_view_observer = std::move(on_view);
    interaction_->group_cursor_observer = std::move(on_cursor);
}

void ChartView::clear_group_observers() noexcept {
    if (!interaction_) return;
    interaction_->group_view_observer = {};
    interaction_->group_cursor_observer = {};
    interaction_->external_cursor_x = std::nullopt;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void ChartView::set_external_cursor_x(std::optional<double> x) noexcept {
    if (!interaction_) {
        interaction_ = std::make_unique<InteractionImpl>();
    }
    if (interaction_->external_cursor_x == x) return;
    interaction_->external_cursor_x = x;
    if (hwnd() != nullptr) InvalidateRect(hwnd(), nullptr, FALSE);
}

void ChartView::apply_settings(ChartSettings s) {
    // Clamp bounds so a malformed settings payload can't break the
    // controller (e.g. animation_ms that locks the UI thread, or a
    // hit tolerance of 0 that disables all interaction).
    if (s.animation_ms > 5000u) s.animation_ms = 5000u;
    if (s.hit_tolerance_px < 1) s.hit_tolerance_px = 1;
    if (s.hit_tolerance_px > 64) s.hit_tolerance_px = 64;

    // CP40: snapshot the existing settings before we overwrite so the
    // change-detection check below compares against the prior values.
    // The settings struct now owns std::wstring axis labels, so this
    // copy is a real allocation that must not live under noexcept.
    const ChartSettings previous = settings_;
    settings_ = std::move(s);

    // Direct ChartView-owned fields. kind_id maps onto ChartKind (see
    // ChartSettings doc comment); unknown values snap to line.
    switch (settings_.kind_id) {
    case 0:  kind_ = ChartKind::bar_vertical;   break;
    case 1:  kind_ = ChartKind::bar_horizontal; break;
    case 2:  kind_ = ChartKind::line;           break;
    case 3:  kind_ = ChartKind::spline;         break;
    case 4:  kind_ = ChartKind::area;           break;
    default: kind_ = ChartKind::line;           break;
    }
    show_legend_ = settings_.show_legend;

    // CP39: refresh the cached fallback palette whenever settings
    // change so renderers that consult effective_palette() see the
    // new theme on the next paint.
    refresh_fallback_palette();

    // Forward into the interaction controller if it's active. Otherwise
    // the values will be re-read by the next enable_interaction() call.
    if (interaction_ != nullptr) {
        interaction_->opts.hit_tolerance_px = settings_.hit_tolerance_px;
        interaction_->opts.animation_ms = settings_.animation_ms;
        interaction_->opts.show_crosshair = settings_.show_crosshair;
        interaction_->opts.show_tooltip = settings_.show_tooltip;
    }

    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // Fire on_settings_changed when any field actually changed so the
    // host can re-resolve its palette (theme) or rebuild controls (kind).
    // Skip the dispatch when the chart is mid-teardown (interaction_
    // null but we still want settings_ to record the latest request).
    const bool changed =
        previous.theme != settings_.theme ||
        previous.kind_id != settings_.kind_id ||
        previous.show_crosshair != settings_.show_crosshair ||
        previous.show_tooltip != settings_.show_tooltip ||
        previous.show_legend != settings_.show_legend ||
        previous.animation_ms != settings_.animation_ms ||
        previous.hit_tolerance_px != settings_.hit_tolerance_px ||
        previous.x_axis_label != settings_.x_axis_label ||
        previous.y_axis_label != settings_.y_axis_label;
    if (changed && interaction_ != nullptr &&
        interaction_->callbacks.on_settings_changed) {
        interaction_->callbacks.on_settings_changed(settings_);
    }
}

ChartSettings ChartView::settings() const {
    return settings_;
}

// --- View range --------------------------------------------------------------

void ChartView::set_visible_range(ChartAxisRange x, ChartAxisRange y) noexcept {
    if (!interaction_) return;
    interaction_->view_x = x;
    interaction_->view_y = y;
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void ChartView::reset_view() noexcept {
    if (!interaction_) return;
    auto& impl = *interaction_;

    if (impl.opts.animate_on_edit && impl.opts.animation_ms > 0) {
        const auto now = impl.clock.now_ms();
        impl.axis_anim.begin(impl.view_x, impl.home_x,
                             impl.view_y, impl.home_y,
                             now, impl.opts.animation_ms);
        impl.reset_pending = true;
        SetTimer(hwnd(), kTimerAnimation, kAnimationIntervalMs, nullptr);
    } else {
        impl.view_x = impl.home_x;
        impl.view_y = impl.home_y;
        InvalidateRect(hwnd(), nullptr, FALSE);
        if (impl.callbacks.on_view_changed) {
            impl.callbacks.on_view_changed(impl.view_x, impl.view_y);
        }
        if (impl.group_view_observer) {
            impl.group_view_observer(impl.view_x, impl.view_y);
        }
        if (impl.callbacks.on_view_reset) {
            impl.callbacks.on_view_reset();
        }
    }
}

ChartAxisRange ChartView::visible_x() const noexcept {
    if (interaction_) return interaction_->view_x;
    return axis_x_;
}

ChartAxisRange ChartView::visible_y() const noexcept {
    if (interaction_) return interaction_->view_y;
    return axis_y_;
}

// --- Undo/Redo ---------------------------------------------------------------

void ChartView::undo() noexcept {
    if (!interaction_) return;
    auto& impl = *interaction_;
    if (impl.undo_stack.empty()) return;

    ChartEditCommand cmd = impl.undo_stack.back();
    impl.undo_stack.pop_back();

    // Apply the old value.
    if (cmd.series_index < series_.size() &&
        cmd.point_index < series_[cmd.series_index].points.size()) {
        series_[cmd.series_index].points[cmd.point_index] = cmd.old_value;
    }
    impl.redo_stack.push_back(cmd);
    InvalidateRect(hwnd(), nullptr, FALSE);

    if (impl.callbacks.on_point_edited) {
        impl.callbacks.on_point_edited(cmd.series_index, cmd.point_index, cmd.old_value);
    }
}

void ChartView::redo() noexcept {
    if (!interaction_) return;
    auto& impl = *interaction_;
    if (impl.redo_stack.empty()) return;

    ChartEditCommand cmd = impl.redo_stack.back();
    impl.redo_stack.pop_back();

    if (cmd.series_index < series_.size() &&
        cmd.point_index < series_[cmd.series_index].points.size()) {
        series_[cmd.series_index].points[cmd.point_index] = cmd.new_value;
    }
    impl.undo_stack.push_back(cmd);
    InvalidateRect(hwnd(), nullptr, FALSE);

    if (impl.callbacks.on_point_edited) {
        impl.callbacks.on_point_edited(cmd.series_index, cmd.point_index, cmd.new_value);
    }
}

bool ChartView::can_undo() const noexcept {
    return interaction_ && !interaction_->undo_stack.empty();
}

bool ChartView::can_redo() const noexcept {
    return interaction_ && !interaction_->redo_stack.empty();
}

// --- Selection ---------------------------------------------------------------

std::vector<std::pair<std::size_t, std::size_t>> ChartView::selected_points() const {
    if (!interaction_) return {};
    return {interaction_->selection.begin(), interaction_->selection.end()};
}

void ChartView::clear_selection() noexcept {
    if (!interaction_) return;
    interaction_->selection.clear();
    InvalidateRect(hwnd(), nullptr, FALSE);
}

// --- Layout helper -----------------------------------------------------------

ChartLayout ChartView::current_layout(const RECT& bounds) const noexcept {
    return compute_chart_layout(bounds, kind_, series_.size());
}

ChartAxisRange ChartView::effective_axis_x() const noexcept {
    return (interaction_ != nullptr) ? interaction_->view_x : axis_x_;
}

ChartAxisRange ChartView::effective_axis_y() const noexcept {
    return (interaction_ != nullptr) ? interaction_->view_y : axis_y_;
}

// --- Message routing ---------------------------------------------------------

LRESULT ChartView::handle_interaction_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (!interaction_) return -1;  // Signal: not handled.

    switch (message) {
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        // CP40: subscribe to WM_MOUSELEAVE so we can notify the host
        // and any linked group charts when the cursor exits the plot.
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd();
        TrackMouseEvent(&tme);
        on_mouse_move_interaction(pt);
        return 0;
    }
    case WM_MOUSELEAVE: {
        // CP40: cursor left the chart; fire nullopt to the host and
        // group so any crosshair on a linked chart is cleared.
        if (interaction_) {
            if (interaction_->callbacks.on_cursor_x_changed) {
                interaction_->callbacks.on_cursor_x_changed(std::nullopt);
            }
            if (interaction_->group_cursor_observer) {
                interaction_->group_cursor_observer(std::nullopt);
            }
            if (interaction_->callbacks.on_cursor_readout) {
                interaction_->callbacks.on_cursor_readout(ChartCursorReadout{});
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_lbutton_down_interaction(pt);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_lbutton_up_interaction(pt);
        return 0;
    }
    case WM_MBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_mbutton_down_interaction(pt);
        return 0;
    }
    case WM_MBUTTONUP: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_mbutton_up_interaction(pt);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        // WM_MOUSEWHEEL coords are screen-relative; convert to client.
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd(), &pt);
        const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
        on_mouse_wheel_interaction(pt, delta);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_lbutton_dblclk_interaction(pt);
        return 0;
    }
    case WM_KEYDOWN:
        if (on_keydown_interaction(wparam)) return 0;
        break;  // fall through to default handler
    case WM_CAPTURECHANGED:
        on_capture_changed_interaction();
        return 0;
    case WM_TIMER:
        if (wparam == kTimerTooltip) {
            on_tooltip_timer();
            return 0;
        }
        if (wparam == kTimerAnimation) {
            on_animation_timer();
            return 0;
        }
        break;
    default:
        break;
    }
    return -1;  // Not handled.
}

// --- Mouse handlers ----------------------------------------------------------

void ChartView::on_mouse_move_interaction(POINT cursor) {
    auto& impl = *interaction_;
    auto& st = impl.state;
    st.cursor_px = cursor;

    RECT client{};
    GetClientRect(hwnd(), &client);
    const ChartLayout layout = current_layout(client);
    st.cursor_in_plot = charts_internal::point_in_plot(cursor, layout);

    using Phase = charts_internal::InteractionPhase;

    switch (st.phase) {
    case Phase::drag_point: {
        // Update the dragged point's value in real time.
        ChartPoint data = charts_internal::pixel_to_data(
            cursor, layout, impl.view_x, impl.view_y);
        auto& pts = series_[st.drag_series].points;
        if (st.drag_point < pts.size()) {
            ChartPoint new_val = pts[st.drag_point];
            if (impl.opts.edit_x) {
                new_val.x = charts_internal::clamp_to_axis(data.x, impl.view_x);
            }
            if (impl.opts.edit_y) {
                new_val.y = charts_internal::clamp_to_axis(data.y, impl.view_y);
            }
            pts[st.drag_point] = new_val;
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
        break;
    }
    case Phase::range_select: {
        st.select_current_px = cursor;
        InvalidateRect(hwnd(), nullptr, FALSE);
        break;
    }
    case Phase::pan: {
        // Compute delta in data units and shift the view.
        const int dx_px = cursor.x - st.pan_origin_px.x;
        const int dy_px = cursor.y - st.pan_origin_px.y;
        const int plot_w = layout.plot_bounds.right - layout.plot_bounds.left;
        const int plot_h = layout.plot_bounds.bottom - layout.plot_bounds.top;
        if (plot_w > 0 && plot_h > 0) {
            const double range_x = st.pan_start_x.max - st.pan_start_x.min;
            const double range_y = st.pan_start_y.max - st.pan_start_y.min;
            const double dx_data = -static_cast<double>(dx_px) / plot_w * range_x;
            const double dy_data = static_cast<double>(dy_px) / plot_h * range_y;
            impl.view_x = charts_internal::pan_axis(st.pan_start_x, dx_data);
            impl.view_y = charts_internal::pan_axis(st.pan_start_y, dy_data);
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
        break;
    }
    default: {
        // Idle or hover: perform hit-test for crosshair/tooltip/hover.
        ChartHitResult hit = charts_internal::hit_test_point(
            cursor, series_, layout, impl.view_x, impl.view_y,
            impl.opts.hit_tolerance_px);

        const bool was_hover = st.hover_hit.hit;
        st.hover_hit = hit;

        if (hit.hit) {
            (void)charts_internal::transition(st, Phase::hover);
            // Start tooltip timer if not already running.
            if (!st.tooltip_visible) {
                st.hover_start_ms = impl.clock.now_ms();
                SetTimer(hwnd(), kTimerTooltip, kTooltipDelayMs, nullptr);
            }
        } else {
            if (st.phase == Phase::hover) {
                (void)charts_internal::transition(st, Phase::idle);
            }
            st.tooltip_visible = false;
            KillTimer(hwnd(), kTimerTooltip);
        }

        // Fire hover callback on change.
        if (hit.hit != was_hover && impl.callbacks.on_hover_changed) {
            impl.callbacks.on_hover_changed(hit);
        }

        // Invalidate for crosshair repaint.
        if (impl.opts.show_crosshair && st.cursor_in_plot) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }

        // Update cursor shape.
        if (hit.hit && has_mode(impl.opts.mode_flags, ChartInteractionMode::drag_edit)) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        }
        break;
    }
    }

    // CP40: notify host + group of the cursor's data-space x. Fired
    // on every move (inside the plot) so a linked group can paint a
    // vertical crosshair at this x against its own y scale. Outside the
    // plot we fire nullopt via WM_MOUSELEAVE.
    if (st.cursor_in_plot) {
        const ChartPoint data_px = charts_internal::pixel_to_data(
            cursor, layout, impl.view_x, impl.view_y);
        if (impl.callbacks.on_cursor_x_changed) {
            impl.callbacks.on_cursor_x_changed(data_px.x);
        }
        if (impl.group_cursor_observer) {
            impl.group_cursor_observer(data_px.x);
        }

        // CP40: emit interpolated per-series readout for the cursor x.
        if (impl.callbacks.on_cursor_readout) {
            ChartCursorReadout readout{};
            readout.x = data_px.x;
            const std::size_t n = series_.size();
            readout.series_values.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                if (!is_series_visible(i)) {
                    continue;
                }
                const double y = interpolate_series_value(series_[i].points, data_px.x);
                readout.series_values.emplace_back(i, y);
            }
            impl.callbacks.on_cursor_readout(std::move(readout));
        }
    } else {
        // Cursor left plot: clear readout so stale values do not linger.
        if (impl.callbacks.on_cursor_readout) {
            impl.callbacks.on_cursor_readout(ChartCursorReadout{});
        }
    }
}

void ChartView::on_lbutton_down_interaction(POINT cursor) {
    auto& impl = *interaction_;
    auto& st = impl.state;

    RECT client{};
    GetClientRect(hwnd(), &client);
    const ChartLayout layout = current_layout(client);

    // Check if Space is held (for pan mode).
    const bool space_held = (GetKeyState(VK_SPACE) & 0x8000) != 0;

    // Pan: Space+Left or pan mode with no point hit.
    if (space_held && has_mode(impl.opts.mode_flags, ChartInteractionMode::pan)) {
        charts_internal::begin_pan(st, cursor, impl.view_x, impl.view_y);
        SetCapture(hwnd());
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }

    // Hit-test for drag_edit.
    if (has_mode(impl.opts.mode_flags, ChartInteractionMode::drag_edit)) {
        ChartHitResult hit = charts_internal::hit_test_point(
            cursor, series_, layout, impl.view_x, impl.view_y,
            impl.opts.hit_tolerance_px);
        if (hit.hit) {
            charts_internal::begin_drag(st, hit.series_index, hit.point_index,
                                        hit.data_point);
            SetCapture(hwnd());
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
            return;
        }
    }

    // Select mode: click to select/deselect.
    if (has_mode(impl.opts.mode_flags, ChartInteractionMode::select)) {
        ChartHitResult hit = charts_internal::hit_test_point(
            cursor, series_, layout, impl.view_x, impl.view_y,
            impl.opts.hit_tolerance_px);
        if (hit.hit) {
            auto key = std::make_pair(hit.series_index, hit.point_index);
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl) {
                // Toggle.
                if (impl.selection.count(key)) {
                    impl.selection.erase(key);
                } else {
                    impl.selection.insert(key);
                }
            } else {
                impl.selection.clear();
                impl.selection.insert(key);
            }
            InvalidateRect(hwnd(), nullptr, FALSE);
            if (impl.callbacks.on_selection_changed) {
                impl.callbacks.on_selection_changed(
                    {impl.selection.begin(), impl.selection.end()});
            }
            return;
        }
    }

    // Range select: start rubber-band on empty area.
    if (has_mode(impl.opts.mode_flags, ChartInteractionMode::range_select)) {
        if (charts_internal::point_in_plot(cursor, layout)) {
            charts_internal::begin_range_select(st, cursor);
            SetCapture(hwnd());
            return;
        }
    }
}

void ChartView::on_lbutton_up_interaction(POINT cursor) {
    auto& impl = *interaction_;
    auto& st = impl.state;

    using Phase = charts_internal::InteractionPhase;

    if (st.phase == Phase::drag_point) {
        // Commit the edit.
        const std::size_t si = st.drag_series;
        const std::size_t pi = st.drag_point;
        ChartPoint new_val{};
        if (si < series_.size() && pi < series_[si].points.size()) {
            new_val = series_[si].points[pi];
        }
        // Push undo command.
        ChartEditCommand cmd{si, pi, st.drag_original, new_val};
        impl.undo_stack.push_back(cmd);
        if (impl.undo_stack.size() > impl.opts.undo_depth) {
            impl.undo_stack.erase(impl.undo_stack.begin());
        }
        impl.redo_stack.clear();

        charts_internal::reset_to_idle(st);
        if (GetCapture() == hwnd()) ReleaseCapture();
        InvalidateRect(hwnd(), nullptr, FALSE);

        if (impl.callbacks.on_point_edited) {
            impl.callbacks.on_point_edited(si, pi, new_val);
        }
        return;
    }

    if (st.phase == Phase::range_select) {
        // Complete the rubber-band selection.
        RECT client{};
        GetClientRect(hwnd(), &client);
        const ChartLayout layout = current_layout(client);

        ChartPoint origin_data = charts_internal::pixel_to_data(
            st.select_origin_px, layout, impl.view_x, impl.view_y);
        ChartPoint extent_data = charts_internal::pixel_to_data(
            cursor, layout, impl.view_x, impl.view_y);

        // Normalize so origin <= extent in data space.
        ChartRangeSelection sel{};
        sel.origin.x = std::min(origin_data.x, extent_data.x);
        sel.origin.y = std::min(origin_data.y, extent_data.y);
        sel.extent.x = std::max(origin_data.x, extent_data.x);
        sel.extent.y = std::max(origin_data.y, extent_data.y);
        sel.pixel_rect = RECT{
            std::min(st.select_origin_px.x, cursor.x),
            std::min(st.select_origin_px.y, cursor.y),
            std::max(st.select_origin_px.x, cursor.x),
            std::max(st.select_origin_px.y, cursor.y),
        };

        // Walk every series, collect points inside the data-space rect, and
        // accumulate per-series statistics. The summary is stored on the
        // InteractionState so paint_interaction_overlay can render it after
        // the rubber-band disappears, and the same numbers are shipped to
        // the host via the on_range_selected callback.
        std::vector<ChartSeriesStats> stats;
        stats.reserve(series_.size());
        std::size_t total = 0;
        double sum_y = 0.0;
        double overall_min_x = std::numeric_limits<double>::infinity();
        double overall_max_x = -std::numeric_limits<double>::infinity();
        double overall_min_y = std::numeric_limits<double>::infinity();
        double overall_max_y = -std::numeric_limits<double>::infinity();
        for (std::size_t si = 0; si < series_.size(); ++si) {
            if (!series_[si].visible) continue;  // CP39: invisible series excluded
            const auto& pts = series_[si].points;
            ChartSeriesStats s{};
            s.series_index = si;
            bool initialized = false;
            for (const ChartPoint& p : pts) {
                if (p.x < sel.origin.x || p.x > sel.extent.x) continue;
                if (p.y < sel.origin.y || p.y > sel.extent.y) continue;
                if (!initialized) {
                    s.min_x = s.max_x = p.x;
                    s.min_y = s.max_y = p.y;
                    initialized = true;
                } else {
                    if (p.x < s.min_x) s.min_x = p.x;
                    if (p.x > s.max_x) s.max_x = p.x;
                    if (p.y < s.min_y) s.min_y = p.y;
                    if (p.y > s.max_y) s.max_y = p.y;
                }
                s.sum_y += p.y;
                ++s.count;
            }
            if (s.count == 0) continue;
            s.mean_y = s.sum_y / static_cast<double>(s.count);
            total += s.count;
            sum_y += s.sum_y;
            if (s.min_x < overall_min_x) overall_min_x = s.min_x;
            if (s.max_x > overall_max_x) overall_max_x = s.max_x;
            if (s.min_y < overall_min_y) overall_min_y = s.min_y;
            if (s.max_y > overall_max_y) overall_max_y = s.max_y;
            stats.push_back(s);
        }
        sel.series_stats = std::move(stats);
        sel.total_count = total;
        if (total > 0) {
            sel.mean_y = sum_y / static_cast<double>(total);
            sel.min_y = overall_min_y;
            sel.max_y = overall_max_y;
        }

        // Cache for the floating card. Visible even when total==0 so the
        // user gets feedback that the rubber-band registered, just with
        // zeros in the stats.
        st.range_summary_visible = true;
        st.range_summary_anchor_px = sel.pixel_rect;
        st.range_summary_stats.clear();
        for (const ChartSeriesStats& s : sel.series_stats) {
            st.range_summary_stats.push_back(s);
        }
        st.range_summary_total = sel.total_count;
        st.range_summary_min_x = (total > 0) ? overall_min_x : 0.0;
        st.range_summary_max_x = (total > 0) ? overall_max_x : 0.0;
        st.range_summary_min_y = sel.min_y;
        st.range_summary_max_y = sel.max_y;
        st.range_summary_mean_y = sel.mean_y;

        charts_internal::reset_to_idle(st);
        // reset_to_idle() just flipped range_summary_visible back off;
        // re-enable because the card should outlive the idle transition.
        st.range_summary_visible = true;

        if (GetCapture() == hwnd()) ReleaseCapture();
        InvalidateRect(hwnd(), nullptr, FALSE);

        if (impl.callbacks.on_range_selected) {
            impl.callbacks.on_range_selected(sel);
        }
        return;
    }

    // Default: release capture if held.
    if (GetCapture() == hwnd()) ReleaseCapture();
}

void ChartView::on_mbutton_down_interaction(POINT cursor) {
    auto& impl = *interaction_;
    if (!has_mode(impl.opts.mode_flags, ChartInteractionMode::pan)) return;

    charts_internal::begin_pan(impl.state, cursor, impl.view_x, impl.view_y);
    SetCapture(hwnd());
    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
}

void ChartView::on_mbutton_up_interaction(POINT) {
    auto& impl = *interaction_;
    using Phase = charts_internal::InteractionPhase;

    if (impl.state.phase == Phase::pan) {
        charts_internal::reset_to_idle(impl.state);
        if (GetCapture() == hwnd()) ReleaseCapture();

        if (impl.callbacks.on_view_changed) {
            impl.callbacks.on_view_changed(impl.view_x, impl.view_y);
        }
        if (impl.group_view_observer) {
            impl.group_view_observer(impl.view_x, impl.view_y);
        }
    }
}

void ChartView::on_mouse_wheel_interaction(POINT cursor, short delta) {
    auto& impl = *interaction_;
    if (!has_mode(impl.opts.mode_flags, ChartInteractionMode::zoom)) return;

    RECT client{};
    GetClientRect(hwnd(), &client);
    const ChartLayout layout = current_layout(client);

    // Compute the data-space point under the cursor (zoom center).
    const ChartPoint center = charts_internal::pixel_to_data(
        cursor, layout, impl.view_x, impl.view_y);

    const double factor = (delta > 0) ? (1.0 / kZoomFactor) : kZoomFactor;

    ChartAxisRange new_x = impl.view_x;
    ChartAxisRange new_y = impl.view_y;

    if (impl.opts.zoom_x) {
        new_x = charts_internal::zoom_axis(impl.view_x, center.x, factor);
    }
    if (impl.opts.zoom_y) {
        new_y = charts_internal::zoom_axis(impl.view_y, center.y, factor);
    }

    if (impl.opts.animate_on_edit && impl.opts.animation_ms > 0) {
        const auto now = impl.clock.now_ms();
        impl.axis_anim.begin(impl.view_x, new_x, impl.view_y, new_y,
                             now, impl.opts.animation_ms);
        SetTimer(hwnd(), kTimerAnimation, kAnimationIntervalMs, nullptr);
    } else {
        impl.view_x = new_x;
        impl.view_y = new_y;
    }

    InvalidateRect(hwnd(), nullptr, FALSE);

    if (impl.callbacks.on_view_changed) {
        impl.callbacks.on_view_changed(new_x, new_y);
    }
    if (impl.group_view_observer) {
        impl.group_view_observer(new_x, new_y);
    }
}

void ChartView::on_lbutton_dblclk_interaction(POINT) {
    auto& impl = *interaction_;
    if (!has_mode(impl.opts.mode_flags, ChartInteractionMode::zoom)) return;
    reset_view();
}

bool ChartView::on_keydown_interaction(WPARAM vkey) {
    auto& impl = *interaction_;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (ctrl && vkey == 'Z') {
        undo();
        return true;
    }
    if (ctrl && vkey == 'Y') {
        redo();
        return true;
    }
    if (ctrl && vkey == 'A') {
        if (has_mode(impl.opts.mode_flags, ChartInteractionMode::select)) {
            impl.selection.clear();
            for (std::size_t si = 0; si < series_.size(); ++si) {
                for (std::size_t pi = 0; pi < series_[si].points.size(); ++pi) {
                    impl.selection.insert({si, pi});
                }
            }
            InvalidateRect(hwnd(), nullptr, FALSE);
            if (impl.callbacks.on_selection_changed) {
                impl.callbacks.on_selection_changed(
                    {impl.selection.begin(), impl.selection.end()});
            }
        }
        return true;
    }
    if (vkey == VK_ESCAPE) {
        clear_selection();
        if (impl.callbacks.on_selection_changed) {
            impl.callbacks.on_selection_changed({});
        }
        return true;
    }
    return false;  // Let default processing see it.
}

void ChartView::on_capture_changed_interaction() {
    if (!interaction_) return;
    // If capture was lost unexpectedly, cancel any in-progress operation.
    charts_internal::reset_to_idle(interaction_->state);
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void ChartView::on_tooltip_timer() {
    if (!interaction_) return;
    auto& impl = *interaction_;

    // The hover-delay timer fires after the cursor has rested on a point for
    // kTooltipDelayMs. Only commit if we're still hovering the same point;
    // a fast mouse move cancels the tooltip via KillTimer in on_mouse_move.
    if (impl.state.hover_hit.hit && !impl.state.tooltip_visible) {
        impl.state.tooltip_visible = true;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
    KillTimer(hwnd(), kTimerTooltip);
}

void ChartView::on_animation_timer() {
    if (!interaction_) return;
    auto& impl = *interaction_;
    if (impl.axis_anim.active) {
        const auto now = impl.clock.now_ms();
        impl.axis_anim.sample(now, impl.view_x, impl.view_y);
        InvalidateRect(hwnd(), nullptr, FALSE);
        if (!impl.axis_anim.active) {
            KillTimer(hwnd(), kTimerAnimation);
            if (impl.reset_pending) {
                impl.reset_pending = false;
                if (impl.callbacks.on_view_reset) {
                    impl.callbacks.on_view_reset();
                }
            }
        }
    }
}

// --- Overlay rendering -------------------------------------------------------

void ChartView::paint_interaction_overlay(HDC hdc, const RECT& bounds) {
    if (!interaction_ || interaction_->opts.mode_flags == 0) return;
    auto& impl = *interaction_;
    auto& st = impl.state;

    const ThemePalette& pal =
        palette_ != nullptr ? *palette_ : theme_palette(ThemeMode::light);
    const ChartLayout layout = current_layout(bounds);
    const RECT& pb = layout.plot_bounds;

    // 1. Crosshair.
    if (impl.opts.show_crosshair && st.cursor_in_plot &&
        st.phase != charts_internal::InteractionPhase::drag_point) {
        HPEN pen = CreatePen(PS_DASH, 1, pal.text_secondary.rgb);
        HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
        // Vertical line.
        MoveToEx(hdc, st.cursor_px.x, pb.top, nullptr);
        LineTo(hdc, st.cursor_px.x, pb.bottom);
        // Horizontal line.
        MoveToEx(hdc, pb.left, st.cursor_px.y, nullptr);
        LineTo(hdc, pb.right, st.cursor_px.y);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);

        // Axis coordinate labels.
        const ChartPoint data = charts_internal::pixel_to_data(
            st.cursor_px, layout, impl.view_x, impl.view_y);
        wchar_t buf_x[32]{}, buf_y[32]{};
        std::swprintf(buf_x, std::size(buf_x), L"%.1f", data.x);
        std::swprintf(buf_y, std::size(buf_y), L"%.1f", data.y);

        HFONT font = (fonts_ != nullptr)
            ? fonts_->mono(dpi_of(hwnd()), font_pt::chart_tick) : nullptr;

        // X label at bottom.
        RECT xl{st.cursor_px.x - 24, pb.bottom + 1, st.cursor_px.x + 24, pb.bottom + 15};
        fill_rect(hdc, xl, pal.surface);
        draw_text(hdc, xl, buf_x, font, pal.text_secondary,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        // Y label at left.
        RECT yl{pb.left - 36, st.cursor_px.y - 7, pb.left - 2, st.cursor_px.y + 7};
        fill_rect(hdc, yl, pal.surface);
        draw_text(hdc, yl, buf_y, font, pal.text_secondary,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // 2. Drag highlight.
    if (st.phase == charts_internal::InteractionPhase::drag_point) {
        if (st.drag_series < series_.size() &&
            st.drag_point < series_[st.drag_series].points.size()) {
            const ChartPoint& dp = series_[st.drag_series].points[st.drag_point];
            const POINT px = charts_internal::data_to_pixel(
                dp, layout, impl.view_x, impl.view_y);
            // Enlarged accent ring.
            const int r = 6;
            RECT ring{px.x - r, px.y - r, px.x + r, px.y + r};
            fill_rounded_rect(hdc, ring, r, pal.accent, pal.accent);
            // Inner dot.
            RECT dot{px.x - 3, px.y - 3, px.x + 3, px.y + 3};
            fill_rounded_rect(hdc, dot, 3, pal.accent_text, pal.accent_text);

            // Value label following the point.
            wchar_t buf[32]{};
            std::swprintf(buf, std::size(buf), L"%.2f", dp.y);
            HFONT font = (fonts_ != nullptr)
                ? fonts_->mono(dpi_of(hwnd()), font_pt::chart_tick) : nullptr;
            RECT label{px.x + 10, px.y - 18, px.x + 70, px.y - 2};
            fill_rounded_rect(hdc, label, 3, pal.surface, pal.border);
            draw_text(hdc, label, buf, font, pal.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Horizontal reference line from y-axis to the point.
            HPEN pen = CreatePen(PS_DOT, 1, pal.accent.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            MoveToEx(hdc, pb.left, px.y, nullptr);
            LineTo(hdc, px.x, px.y);
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
        }
    }

    // 3. Rubber-band rectangle.
    if (st.phase == charts_internal::InteractionPhase::range_select) {
        RECT band{
            std::min(st.select_origin_px.x, st.select_current_px.x),
            std::min(st.select_origin_px.y, st.select_current_px.y),
            std::max(st.select_origin_px.x, st.select_current_px.x),
            std::max(st.select_origin_px.y, st.select_current_px.y),
        };
        // Surface-derived tint so the selection reads as a slight variation
        // of the background — never a separate accent color. surface_hover
        // is the canonical "selected surface" token in ThemePalette and is
        // automatically slightly darker than surface in light mode, slightly
        // lighter in dark mode. Blending at ~31% keeps the data lines visible.
        fill_rect_alpha(hdc, band, pal.surface_hover, 80);
        HPEN pen = CreatePen(PS_SOLID, 1, pal.border.rgb);
        HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, band.left, band.top, band.right, band.bottom);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }

    // 4. Selection rings.
    if (!impl.selection.empty()) {
        for (const auto& [si, pi] : impl.selection) {
            if (si >= series_.size() || pi >= series_[si].points.size()) continue;
            const POINT px = charts_internal::data_to_pixel(
                series_[si].points[pi], layout, impl.view_x, impl.view_y);
            const int r = 5;
            RECT ring{px.x - r, px.y - r, px.x + r, px.y + r};
            // Draw accent ring (hollow).
            HPEN pen = CreatePen(PS_SOLID, 2, pal.accent.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            Ellipse(hdc, ring.left, ring.top, ring.right, ring.bottom);
            SelectObject(hdc, old_brush);
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
        }
    }

    // 5. Tooltip.
    if (impl.opts.show_tooltip && st.tooltip_visible && st.hover_hit.hit) {
        const auto& hit = st.hover_hit;
        if (hit.series_index < series_.size()) {
            const auto& series = series_[hit.series_index];
            wchar_t buf[128]{};
            std::swprintf(buf, std::size(buf), L"%.*s: (%.1f, %.1f)",
                          static_cast<int>(series.name.size()), series.name.data(),
                          hit.data_point.x, hit.data_point.y);

            HFONT font = (fonts_ != nullptr)
                ? fonts_->regular(dpi_of(hwnd()), font_pt::chart_tick) : nullptr;

            // Measure text.
            HFONT old_font = font ? static_cast<HFONT>(SelectObject(hdc, font)) : nullptr;
            SIZE text_size{};
            GetTextExtentPoint32W(hdc, buf, static_cast<int>(wcslen(buf)), &text_size);
            if (old_font) SelectObject(hdc, old_font);

            const int pad = 6;
            const int tw = text_size.cx + 2 * pad;
            const int th = text_size.cy + 2 * pad;

            // Position: top-right of the data point, flip if near edge.
            int tx = hit.pixel_point.x + 8;
            int ty = hit.pixel_point.y - 8 - th;
            if (tx + tw > bounds.right) tx = hit.pixel_point.x - 8 - tw;
            if (ty < bounds.top) ty = hit.pixel_point.y + 8;

            RECT tip{tx, ty, tx + tw, ty + th};
            fill_rounded_rect(hdc, tip, 4, pal.surface, pal.border);
            draw_text(hdc, tip, buf, font, pal.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    // 6. Range summary card. Drawn last so it sits on top of every other
    // overlay. The card is anchored to the rubber-band rect that just
    // committed; it flips sides if it would clip the client edges.
    if (st.range_summary_visible) {
        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        HFONT regular_font = (fonts_ != nullptr)
            ? fonts_->regular(dpi, font_pt::chart_tick) : nullptr;
        HFONT mono_font = (fonts_ != nullptr)
            ? fonts_->mono(dpi, font_pt::chart_tick) : nullptr;

        // Build per-series text blocks. Each block is 2 lines: a series-name
        // line in the regular font, and a stats line in the mono font.
        struct Block {
            std::wstring name;
            std::wstring stats;
            Color dot_color;
        };
        std::vector<Block> blocks;
        blocks.reserve(st.range_summary_stats.size());
        for (const auto& s : st.range_summary_stats) {
            Block b{};
            if (s.series_index < series_.size()) {
                const auto& sr = series_[s.series_index];
                b.name.assign(sr.name.data(), sr.name.size());
                b.dot_color = sr.color;
            } else {
                b.name = L"(series)";
                b.dot_color = pal.accent;
            }
            wchar_t stats_buf[160]{};
            if (s.count == 0) {
                std::swprintf(stats_buf, std::size(stats_buf), L"n=0");
            } else {
                std::swprintf(stats_buf, std::size(stats_buf),
                              L"n=%zu   y %.2f \x2192 %.2f   \x03bc = %.2f",
                              s.count, s.min_y, s.max_y, s.mean_y);
            }
            b.stats = stats_buf;
            blocks.push_back(std::move(b));
        }

        // Footer: overall x/y range and total count. Falls back to the
// data-space selection extents when no points were inside the box.
        wchar_t footer_buf[200]{};
        if (st.range_summary_total == 0) {
            std::swprintf(footer_buf, std::size(footer_buf),
                          L"No points inside the selection");
        } else {
            std::swprintf(footer_buf, std::size(footer_buf),
                          L"Range  x[%.2f .. %.2f]   y[%.2f .. %.2f]",
                          st.range_summary_min_x, st.range_summary_max_x,
                          st.range_summary_min_y, st.range_summary_max_y);
        }
        std::wstring footer = footer_buf;

        // Measure every line. Switch into the font we want to draw with
        // before measuring so the width/height reflect that face.
        auto measure = [&](const std::wstring& s, HFONT f) -> SIZE {
            if (s.empty()) return SIZE{0, 0};
            HFONT old = f ? static_cast<HFONT>(SelectObject(hdc, f)) : nullptr;
            SIZE sz{};
            GetTextExtentPoint32W(hdc, s.c_str(), static_cast<int>(s.size()), &sz);
            if (old) SelectObject(hdc, old);
            return sz;
        };

        int max_text_w = 0;
        int total_h = 0;
        SIZE name_sz{};
        for (const Block& b : blocks) {
            name_sz = measure(b.name, regular_font);
            const SIZE stats_sz = measure(b.stats, mono_font);
            const int block_w = std::max(name_sz.cx, stats_sz.cx);
            if (block_w > max_text_w) max_text_w = block_w;
            // name line + stats line + 2-px gap between them
            total_h += name_sz.cy + stats_sz.cy + 2;
        }
        const SIZE footer_sz = measure(footer, mono_font);
        if (footer_sz.cx > max_text_w) max_text_w = footer_sz.cx;
        total_h += footer_sz.cy + 8 /*separator gap*/ + 6 /*header gap*/;

        // Card geometry. 10-px outer padding, 16-px left strip reserved for
        // the colored dot, 4-px radius. Min width 180 to keep the card
        // visually substantial even with very short series names.
        const int pad = 10;
        const int strip = 16;
        int card_w = max_text_w + pad * 2 + strip;
        const int min_w = 180;
        const int max_w = 320;
        if (card_w < min_w) card_w = min_w;
        if (card_w > max_w) card_w = max_w;
        int card_h = total_h + pad * 2;

        // Position: prefer the right of the rubber-band; flip to left or
        // above if it would clip. Final clamp keeps the card inside the
        // chart client area even if the rubber-band hugs an edge.
        const RECT anchor = st.range_summary_anchor_px;
        int cx = anchor.right + 8;
        int cy = anchor.top;
        if (cx + card_w > bounds.right) {
            cx = anchor.left - 8 - card_w;
        }
        if (cx + card_w > bounds.right || cx < bounds.left) {
            cx = bounds.left + 4;
        }
        if (cy + card_h > bounds.bottom) {
            cy = anchor.top - 8 - card_h;
        }
        if (cy + card_h > bounds.bottom || cy < bounds.top) {
            cy = bounds.top + 4;
        }
        RECT card{cx, cy, cx + card_w, cy + card_h};

        // Card background and border.
        fill_rounded_rect(hdc, card, 4, pal.surface, pal.border);
        // 3-px accent stripe down the left edge for "this is a selection".
        RECT stripe{card.left, card.top + 2, card.left + 3, card.bottom - 2};
        fill_rect(hdc, stripe, pal.accent);

        // Header line: bold-ish "Selection" + total count badge.
        const int text_left = card.left + strip + 4;
        const int text_right = card.right - pad;
        HFONT old_font = regular_font
            ? static_cast<HFONT>(SelectObject(hdc, regular_font)) : nullptr;
        SetTextColor(hdc, pal.text.rgb);
        RECT header{text_left, card.top + pad, text_right, card.top + pad + 16};
        wchar_t header_buf[80]{};
        std::swprintf(header_buf, std::size(header_buf),
                      L"Selection  \x2014  %zu pts",
                      st.range_summary_total);
        draw_text(hdc, header, header_buf, regular_font, pal.text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (old_font) SelectObject(hdc, old_font);

        // Per-series blocks.
        int y = header.bottom + 4;
        for (const Block& b : blocks) {
            // 6-px colored dot.
            const int dot_r = 3;
            const int dot_cx = text_left + dot_r;
            const int dot_cy = y + 7;
            HBRUSH dot_brush = CreateSolidBrush(b.dot_color.rgb);
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, dot_brush));
            HPEN old_pen = static_cast<HPEN>(SelectObject(
                hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, dot_cx - dot_r, dot_cy - dot_r,
                        dot_cx + dot_r, dot_cy + dot_r);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(dot_brush);

            // Series name (regular font, left-aligned next to the dot).
            RECT name_rect{text_left + dot_r * 2 + 4, y,
                           text_right, y + 14};
            draw_text(hdc, name_rect, b.name, regular_font, pal.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Stats (mono, dimmed) directly under the name.
            HFONT old_mono = mono_font
                ? static_cast<HFONT>(SelectObject(hdc, mono_font)) : nullptr;
            RECT stats_rect{text_left + dot_r * 2 + 4, y + 14,
                            text_right, y + 14 + 13};
            draw_text(hdc, stats_rect, b.stats, mono_font, pal.text_secondary,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (old_mono) SelectObject(hdc, old_mono);

            y = stats_rect.bottom + 2;
        }

        // Thin separator above the footer.
        HPEN sep_pen = CreatePen(PS_SOLID, 1, pal.border.rgb);
        HPEN old_sep = static_cast<HPEN>(SelectObject(hdc, sep_pen));
        MoveToEx(hdc, text_left, y + 2, nullptr);
        LineTo(hdc, text_right, y + 2);
        SelectObject(hdc, old_sep);
        DeleteObject(sep_pen);

        // Footer (mono, dimmed).
        HFONT old_footer = mono_font
            ? static_cast<HFONT>(SelectObject(hdc, mono_font)) : nullptr;
        RECT footer_rect{text_left, y + 6, text_right, card.bottom - pad};
        draw_text(hdc, footer_rect, footer, mono_font, pal.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (old_footer) SelectObject(hdc, old_footer);
    }
}

} // namespace nfui

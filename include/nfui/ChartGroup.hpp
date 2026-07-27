#pragma once

// CP40: non-owning controller that synchronizes a small set of
// ChartViews: linked x-axis (the primary's x range propagates to every
// child), synchronized cursor x (mouse moves on one chart paint a
// vertical crosshair on the others), and an explicit primary role so
// ranges can be set from the outside without the group picking a winner.
//
// Ownership contract:
//   - ChartGroup does NOT own or destroy its children.
//   - All children must be UI-thread-affine and outlive the group.
//   - In a containing class, declare ChartViews before ChartGroup so
//     reverse destruction destroys the group first.
//   - If that lifetime can't be guaranteed, call remove_chart() before
//     destroying a child.

#include <nfui/ChartInteraction.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace nfui {

class ChartView;

enum class ChartGroupRole {
    primary,    // Source of truth for ranges; only one primary allowed.
    overlay,    // Co-renders with the primary (not yet differentiated).
    sub,        // Linked to the primary via x-axis + cursor.
};

class ChartGroup {
public:
    ChartGroup() = default;
    ~ChartGroup();

    ChartGroup(const ChartGroup&) = delete;
    ChartGroup& operator=(const ChartGroup&) = delete;
    ChartGroup(ChartGroup&&) = delete;
    ChartGroup& operator=(ChartGroup&&) = delete;

    // Add a chart to the group. Null charts, duplicates, or a second
    // primary when one already exists return false without mutation.
    [[nodiscard]] bool add_chart(ChartView* chart, ChartGroupRole role) noexcept;

    // Remove a chart. No-op for unknown charts. Clears that chart's
    // observer hooks and external cursor before it leaves the group.
    void remove_chart(ChartView* chart) noexcept;

    // Drop every chart. Each chart's observers + external cursor are
    // cleared before the entry is dropped.
    void clear() noexcept;

    // Linked x-axis. When enabled (default true), changes to the
    // primary's x range propagate to every sub chart's x range. The y
    // ranges stay independent — this avoids a dual-axis chart.
    void link_x_axis(bool enabled) noexcept;
    [[nodiscard]] bool x_axis_linked() const noexcept { return x_linked_; }

    // Synchronized cursor. When enabled (default true), the cursor x
    // broadcast from one chart draws an external crosshair on every
    // linked chart.
    void sync_cursor(bool enabled) noexcept;
    [[nodiscard]] bool cursor_synced() const noexcept { return cursor_synced_; }

    // Push a new x range into the primary (and, if x is linked, into
    // every sub chart). No-op when no primary exists.
    void set_primary_x_axis(ChartAxisRange range) noexcept;

    [[nodiscard]] ChartView* primary() const noexcept { return primary_; }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    struct Entry {
        ChartView* chart{};
        ChartGroupRole role{ChartGroupRole::sub};
    };

    void on_view_changed(ChartView* source,
                         ChartAxisRange x,
                         ChartAxisRange y) noexcept;
    void on_cursor_changed(ChartView* source,
                           std::optional<double> x) noexcept;

    // Linear search by chart pointer. Returns entries_.size() on miss.
    [[nodiscard]] std::size_t index_of(ChartView* chart) const noexcept;

    std::vector<Entry> entries_{};
    ChartView* primary_{};
    bool x_linked_{true};
    bool cursor_synced_{true};
    // Re-entrancy guard: propagation from a child back into set_visible_range
    // must not re-fire the observer. Set on the synchronous propagation path
    // and cleared on the way out.
    bool broadcasting_{false};
};

} // namespace nfui

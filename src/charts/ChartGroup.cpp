// CP40: non-owning ChartGroup controller. Subscribes to view-range
// and cursor-x observers on each child and propagates the primary's
// x range to every linked chart. Y ranges stay independent so a
// dashboard can place a 0-100 primary next to a 35-85 comparison
// chart without drama.
//
// All operations are O(N) in the entry count and assume the UI thread.
// No background work or threading primitives.

#include <nfui/ChartGroup.hpp>
#include <nfui/Charts.hpp>

#include <algorithm>

namespace nfui {

namespace {

} // namespace

ChartGroup::~ChartGroup() {
    clear();
}

std::size_t ChartGroup::index_of(ChartView* chart) const noexcept {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].chart == chart) return i;
    }
    return entries_.size();
}

bool ChartGroup::add_chart(ChartView* chart, ChartGroupRole role) noexcept {
    if (chart == nullptr) return false;
    if (index_of(chart) < entries_.size()) return false;  // duplicate
    if (role == ChartGroupRole::primary && primary_ != nullptr) return false;

    entries_.push_back({chart, role});
    if (role == ChartGroupRole::primary || primary_ == nullptr) {
        primary_ = chart;
    }

    // Wire observer hooks so the chart forwards view + cursor changes
    // into this group. The group keeps the chart alive through its
    // clear()/remove_chart() destructor contract.
    chart->set_group_observers(
        [this, chart](ChartAxisRange x, ChartAxisRange y) {
            on_view_changed(chart, x, y);
        },
        [this, chart](std::optional<double> x) {
            on_cursor_changed(chart, x);
        });
    return true;
}

void ChartGroup::remove_chart(ChartView* chart) noexcept {
    if (chart == nullptr) return;
    const std::size_t idx = index_of(chart);
    if (idx >= entries_.size()) return;

    // Clear observer hooks + external cursor on the leaving chart so
    // it doesn't keep ticking on a stale pointer.
    chart->clear_group_observers();

    entries_.erase(entries_.begin()
                   + static_cast<std::ptrdiff_t>(idx));

    // If we removed the primary, promote the first remaining entry or
    // clear primary_. Prefer explicit primary roles over the implicit
    // "first chart wins" fallback.
    if (primary_ == chart) {
        primary_ = nullptr;
        for (const Entry& e : entries_) {
            if (e.role == ChartGroupRole::primary) {
                primary_ = e.chart;
                break;
            }
        }
        if (primary_ == nullptr && !entries_.empty()) {
            primary_ = entries_.front().chart;
        }
    }
}

void ChartGroup::clear() noexcept {
    for (const Entry& e : entries_) {
        if (e.chart != nullptr) e.chart->clear_group_observers();
    }
    entries_.clear();
    primary_ = nullptr;
}

void ChartGroup::link_x_axis(bool enabled) noexcept {
    if (x_linked_ == enabled) return;
    x_linked_ = enabled;
    // When (re-)enabling, broadcast the primary's current range to
    // every other chart so they catch up immediately.
    if (enabled && primary_ != nullptr) {
        const ChartAxisRange x = primary_->visible_x();
        broadcasting_ = true;
        for (const Entry& e : entries_) {
            if (e.chart == nullptr || e.chart == primary_) continue;
            e.chart->set_visible_range(x, e.chart->visible_y());
        }
        broadcasting_ = false;
    }
}

void ChartGroup::sync_cursor(bool enabled) noexcept {
    cursor_synced_ = enabled;
    if (!enabled) {
        // Clearing the cursor sync drops the external crosshair on
        // every non-source chart. Push nullopt through the observer
        // path so this happens via the normal invalidate path.
        for (const Entry& e : entries_) {
            if (e.chart == nullptr) continue;
            e.chart->set_external_cursor_x(std::nullopt);
        }
    }
}

void ChartGroup::set_primary_x_axis(ChartAxisRange range) noexcept {
    if (primary_ == nullptr) return;
    primary_->set_visible_range(range, primary_->visible_y());
    if (!x_linked_) return;
    broadcasting_ = true;
    for (const Entry& e : entries_) {
        if (e.chart == nullptr || e.chart == primary_) continue;
        e.chart->set_visible_range(range, e.chart->visible_y());
    }
    broadcasting_ = false;
}

void ChartGroup::on_view_changed(ChartView* source,
                                 ChartAxisRange x,
                                 ChartAxisRange /*y*/) noexcept {
    if (!x_linked_ || broadcasting_) return;
    if (source == nullptr) return;
    // Only the primary broadcasts x. Sub chart view changes don't
    // trigger propagation — that would let a user-controlled sub
    // chart hijack the primary.
    if (source != primary_) return;

    broadcasting_ = true;
    for (const Entry& e : entries_) {
        if (e.chart == nullptr || e.chart == source) continue;
        e.chart->set_visible_range(x, e.chart->visible_y());
    }
    broadcasting_ = false;
}

void ChartGroup::on_cursor_changed(ChartView* source,
                                   std::optional<double> x) noexcept {
    if (!cursor_synced_) return;
    if (source == nullptr) return;
    for (const Entry& e : entries_) {
        if (e.chart == nullptr || e.chart == source) continue;
        e.chart->set_external_cursor_x(x);
    }
}

} // namespace nfui

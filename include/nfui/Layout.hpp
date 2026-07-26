#pragma once

#include <initializer_list>
#include <vector>

namespace nfui {

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct SplitterLayout {
    Rect first{};
    Rect splitter{};
    Rect second{};
};

// Splitter layouts: divide a bounds rect into two panes with a splitter bar.
[[nodiscard]] SplitterLayout split_horizontally(Rect bounds, double ratio, int splitter_width) noexcept;
[[nodiscard]] SplitterLayout split_vertically(Rect bounds, double ratio, int splitter_height) noexcept;

// Linear layouts: distribute items along one axis with fixed sizes and gaps.
[[nodiscard]] std::vector<Rect> layout_horizontal(Rect bounds, std::initializer_list<int> widths, int gap);
[[nodiscard]] std::vector<Rect> layout_vertical(Rect bounds, std::initializer_list<int> heights, int gap);

// Anchor flags for anchor_layout. Mirror Win32 anchor semantics:
// the control maintains its distance from the anchored edges.
enum AnchorFlags : unsigned {
    anchor_none   = 0,
    anchor_left   = 1u << 0,
    anchor_top    = 1u << 1,
    anchor_right  = 1u << 2,
    anchor_bottom = 1u << 3,
    anchor_all    = anchor_left | anchor_top | anchor_right | anchor_bottom,
};

// Compute the new rect for a control anchored within a resized parent.
// original_parent is the parent size at the time the control was placed;
// new_parent is the current parent size. The control's initial rect is
// anchor_rect (relative to the parent origin). Distances to anchored edges
// are preserved; unanchored edges keep their original offset from the
// parent origin.
[[nodiscard]] Rect anchor_layout(Rect anchor_rect, Rect original_parent, Rect new_parent, unsigned flags) noexcept;

} // namespace nfui

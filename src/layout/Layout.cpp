#include <nfui/Layout.hpp>

namespace nfui {
namespace {

double clamp_ratio(double ratio) noexcept {
    if (ratio < 0.0) {
        return 0.0;
    }
    if (ratio > 1.0) {
        return 1.0;
    }
    return ratio;
}

} // namespace

SplitterLayout split_horizontally(Rect bounds, double ratio, int splitter_width) noexcept {
    ratio = clamp_ratio(ratio);
    if (splitter_width < 0) {
        splitter_width = 0;
    }

    int first_width = static_cast<int>(bounds.width * ratio);
    if (first_width > bounds.width - splitter_width) {
        first_width = bounds.width - splitter_width;
    }
    if (first_width < 0) {
        first_width = 0;
    }

    SplitterLayout layout{};
    layout.first = Rect{bounds.x, bounds.y, first_width, bounds.height};
    layout.splitter = Rect{bounds.x + first_width, bounds.y, splitter_width, bounds.height};
    layout.second = Rect{bounds.x + first_width + splitter_width,
                         bounds.y,
                         bounds.width - first_width - splitter_width,
                         bounds.height};
    return layout;
}

std::vector<Rect> layout_horizontal(Rect bounds, std::initializer_list<int> widths, int gap) {
    std::vector<Rect> result;
    result.reserve(widths.size());
    int x = bounds.x;
    for (int width : widths) {
        result.push_back(Rect{x, bounds.y, width, bounds.height});
        x += width + gap;
    }
    return result;
}

SplitterLayout split_vertically(Rect bounds, double ratio, int splitter_height) noexcept {
    ratio = clamp_ratio(ratio);
    if (splitter_height < 0) {
        splitter_height = 0;
    }

    int first_height = static_cast<int>(bounds.height * ratio);
    if (first_height > bounds.height - splitter_height) {
        first_height = bounds.height - splitter_height;
    }
    if (first_height < 0) {
        first_height = 0;
    }

    SplitterLayout layout{};
    layout.first = Rect{bounds.x, bounds.y, bounds.width, first_height};
    layout.splitter = Rect{bounds.x, bounds.y + first_height, bounds.width, splitter_height};
    layout.second = Rect{bounds.x,
                         bounds.y + first_height + splitter_height,
                         bounds.width,
                         bounds.height - first_height - splitter_height};
    return layout;
}

std::vector<Rect> layout_vertical(Rect bounds, std::initializer_list<int> heights, int gap) {
    std::vector<Rect> result;
    result.reserve(heights.size());
    int y = bounds.y;
    for (int height : heights) {
        result.push_back(Rect{bounds.x, y, bounds.width, height});
        y += height + gap;
    }
    return result;
}

Rect anchor_layout(Rect anchor_rect, Rect original_parent, Rect new_parent, unsigned flags) noexcept {
    Rect result = anchor_rect;

    int delta_w = new_parent.width - original_parent.width;
    int delta_h = new_parent.height - original_parent.height;

    bool left   = (flags & anchor_left)   != 0;
    bool top    = (flags & anchor_top)    != 0;
    bool right  = (flags & anchor_right)  != 0;
    bool bottom = (flags & anchor_bottom) != 0;

    if (left && right) {
        // Both horizontal edges anchored: grow/shrink width.
        result.width = anchor_rect.width + delta_w;
        if (result.width < 0) { result.width = 0; }
    } else if (right) {
        // Only right anchored: shift x by delta.
        result.x = anchor_rect.x + delta_w;
    }
    // If only left or neither: x stays unchanged.

    if (top && bottom) {
        // Both vertical edges anchored: grow/shrink height.
        result.height = anchor_rect.height + delta_h;
        if (result.height < 0) { result.height = 0; }
    } else if (bottom) {
        // Only bottom anchored: shift y by delta.
        result.y = anchor_rect.y + delta_h;
    }
    // If only top or neither: y stays unchanged.

    return result;
}

} // namespace nfui

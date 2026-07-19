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

} // namespace nfui

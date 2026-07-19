#pragma once

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

[[nodiscard]] SplitterLayout split_horizontally(Rect bounds, double ratio, int splitter_width) noexcept;
[[nodiscard]] std::vector<Rect> layout_horizontal(Rect bounds, std::initializer_list<int> widths, int gap);

} // namespace nfui

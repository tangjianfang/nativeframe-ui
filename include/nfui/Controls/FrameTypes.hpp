#pragma once

#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct FrameStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> accent;
};

} // namespace nfui
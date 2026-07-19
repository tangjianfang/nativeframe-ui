#include <nfui/Dpi.hpp>

#include <cstdlib>

namespace nfui {

DpiScale::DpiScale(int dpi) noexcept
    : dpi_(dpi > 0 ? dpi : 96) {
}

int DpiScale::dpi() const noexcept {
    return dpi_;
}

int DpiScale::logical_to_pixels(int logical) const noexcept {
    return (logical * dpi_ + 48) / 96;
}

int DpiScale::pixels_to_logical(int pixels) const noexcept {
    return (pixels * 96 + dpi_ / 2) / dpi_;
}

int DpiScale::scale_font_height(int logical_height) const noexcept {
    int sign = logical_height < 0 ? -1 : 1;
    return sign * logical_to_pixels(std::abs(logical_height));
}

} // namespace nfui

#pragma once

namespace nfui {

class DpiScale {
public:
    explicit DpiScale(int dpi = 96) noexcept;

    [[nodiscard]] int dpi() const noexcept;
    [[nodiscard]] int logical_to_pixels(int logical) const noexcept;
    [[nodiscard]] int pixels_to_logical(int pixels) const noexcept;
    [[nodiscard]] int scale_font_height(int logical_height) const noexcept;

private:
    int dpi_{96};
};

} // namespace nfui

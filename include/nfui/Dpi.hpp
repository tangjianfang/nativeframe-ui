#pragma once

#include <windows.h>

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

// Returns the per-window DPI using GetDpiForWindow, falling back to 96 on failure.
[[nodiscard]] int dpi_of(HWND hwnd) noexcept;

} // namespace nfui

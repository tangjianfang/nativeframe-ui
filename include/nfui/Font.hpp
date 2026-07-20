#pragma once

#include <windows.h>

namespace nfui {

[[nodiscard]] int font_pixel_height(int point_size, int dpi) noexcept;

class FontCache {
public:
    FontCache() noexcept = default;
    ~FontCache() noexcept;
    FontCache(const FontCache&) = delete;
    FontCache& operator=(const FontCache&) = delete;

    // Returns Segoe UI, DPI-scaled. Stable for the cache lifetime; do not DeleteObject() the result.
    [[nodiscard]] HFONT regular(int dpi, int point_size) noexcept;   // FW_NORMAL
    [[nodiscard]] HFONT semibold(int dpi, int point_size) noexcept;  // FW_SEMIBOLD

private:
    HFONT make(int dpi, int point_size, int weight) noexcept;
    HFONT regular_12_{};
    HFONT semibold_12_{};
    int   cached_dpi_{0};
};

} // namespace nfui

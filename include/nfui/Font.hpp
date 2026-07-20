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
    FontCache(FontCache&&) = delete;
    FontCache& operator=(FontCache&&) = delete;

    // Returns Segoe UI, DPI-scaled. Stable for the cache lifetime; do not DeleteObject() the result.
    [[nodiscard]] HFONT regular(int dpi, int point_size) noexcept;   // FW_NORMAL, Segoe UI
    [[nodiscard]] HFONT semibold(int dpi, int point_size) noexcept;  // FW_SEMIBOLD, Segoe UI
    [[nodiscard]] HFONT serif(int dpi, int point_size) noexcept;     // FW_NORMAL, Georgia
    [[nodiscard]] HFONT mono(int dpi, int point_size) noexcept;      // FW_NORMAL, Cascadia Code (fallback Consolas)

private:
    HFONT make(int dpi, int point_size, int weight, DWORD pitch_and_family, const wchar_t* family) noexcept;
    void rebuild(HFONT& slot, int& slot_dpi, int& slot_pt, int dpi, int pt, int weight,
                 DWORD pitch_and_family, const wchar_t* family) noexcept;

    HFONT regular_{};
    HFONT semibold_{};
    HFONT serif_{};
    HFONT mono_{};
    int   regular_dpi_{0};  int regular_pt_{0};
    int   semibold_dpi_{0}; int semibold_pt_{0};
    int   serif_dpi_{0};    int serif_pt_{0};
    int   mono_dpi_{0};     int mono_pt_{0};
};

} // namespace nfui

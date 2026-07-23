#pragma once

#include <windows.h>

namespace nfui {

// Centralised point-size tokens used by every caller of FontCache. The
// default UI size (9 pt) is the dominant text size across buttons, static
// labels, list rows, combo rows, list-view items, chart tick labels, and the
// chart legend. Naming the magic number avoids the scattered "9" literal
// the codebase had before CP8 and keeps every visual surface in sync when
// the design system bumps it.
namespace font_pt {
constexpr int ui = 9;       // Back-compat: smallest UI chrome (status bar, tiny badges)

// CP32 design scale: sans-serif UI typography. Names are independent of
// the old "ui" value so the new scale can coexist with legacy call sites.
constexpr int xs    = 12;   // Captions, small badges, eyebrow labels
constexpr int sm    = 13;   // Secondary body, card metadata
constexpr int base  = 14;   // Primary body, list rows, form labels
constexpr int md    = 16;   // Emphasised body, buttons, section headers
constexpr int lg    = 20;   // Card titles, KPI values, sidebar section labels
constexpr int xl    = 28;   // Window title / brand mark

// Chart typography aliases (same scale, semantic names).
constexpr int chart_tick   = xs;
constexpr int chart_legend = sm;
} // namespace font_pt

[[nodiscard]] int font_pixel_height(int point_size, int dpi) noexcept;

class FontCache {
public:
    FontCache() noexcept = default;
    ~FontCache() noexcept;
    FontCache(const FontCache&) = delete;
    FontCache& operator=(const FontCache&) = delete;
    FontCache(FontCache&&) = delete;
    FontCache& operator=(FontCache&&) = delete;

    // Returns Segoe UI / Georgia / Cascadia Code, DPI-scaled. Stable for the
    // cache lifetime; do not DeleteObject() the result. Each slot is keyed
    // on (dpi, point_size); a request whose key matches the cache returns
    // the existing HFONT, otherwise the slot is rebuilt. Callers that query
    // with the live per-window DPI get the rebuilt face automatically on a
    // WM_DPICHANGED turn.
    [[nodiscard]] HFONT regular(int dpi, int point_size) noexcept;   // FW_NORMAL, Segoe UI
    [[nodiscard]] HFONT bold(int dpi, int point_size) noexcept;      // FW_BOLD, Segoe UI
    [[nodiscard]] HFONT semibold(int dpi, int point_size) noexcept;  // FW_SEMIBOLD, Segoe UI
    [[nodiscard]] HFONT serif(int dpi, int point_size) noexcept;     // FW_NORMAL, Georgia
    [[nodiscard]] HFONT mono(int dpi, int point_size) noexcept;      // FW_NORMAL, Cascadia Code (fallback Consolas)

private:
    HFONT make(int dpi, int point_size, int weight, DWORD pitch_and_family, const wchar_t* family) noexcept;
    void rebuild(HFONT& slot, int& slot_dpi, int& slot_pt, int dpi, int pt, int weight,
                 DWORD pitch_and_family, const wchar_t* family) noexcept;

    HFONT regular_{};
    HFONT bold_{};
    HFONT semibold_{};
    HFONT serif_{};
    HFONT mono_{};
    int   regular_dpi_{0};  int regular_pt_{0};
    int   bold_dpi_{0};     int bold_pt_{0};
    int   semibold_dpi_{0}; int semibold_pt_{0};
    int   serif_dpi_{0};    int serif_pt_{0};
    int   mono_dpi_{0};     int mono_pt_{0};
};

} // namespace nfui
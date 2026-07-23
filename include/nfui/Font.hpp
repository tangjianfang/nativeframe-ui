#pragma once

#include <windows.h>

namespace nfui {

// Centralised point-size tokens used by every caller of FontCache. The
// default UI size (9 pt) is the dominant text size across buttons, static
// labels, list rows, combo rows, list-view items, chart tick labels, and the
// chart legend. Naming the magic number avoids the scattered "9" literal
// the codebase had before CP8 and keeps every visual surface in sync when
// the design system bumps it.
//
// CP32: extended to a full 6-step scale (xs/sm/base/md/lg/xl) so samples
// can pull sizes from one place. The numeric values map to the design
// scale 12 / 13 / 14 / 16 / 20 / 28. All sizes compose with DpiScale and
// FontCache so any DPI retains the proportions of the design. Existing
// `font_pt::ui` callers stay on 9 pt; new sample surfaces should pick the
// step that fits the design (base for body, lg for KPI values, xl for
// page titles).
namespace font_pt {
constexpr int xs    = 12;   // CP32: micro copy, table rows, dense chips
constexpr int sm    = 13;   // CP32: secondary text, helper labels
constexpr int base  = 14;   // CP32: design baseline body size
constexpr int md    = 16;   // CP32: section titles, table headers
constexpr int lg    = 20;   // CP32: card values, page subtitles
constexpr int xl    = 28;   // CP32: page titles, hero numbers
// historical / chart aliases — back-compat for the codebase pre-CP32.
constexpr int ui    = 9;    // buttons, list rows, combobox rows, listview items
constexpr int chart_tick   = ui;   // Chart axis tick labels (mono)
constexpr int chart_legend = sm;   // Chart legend series names
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
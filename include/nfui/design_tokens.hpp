#pragma once

// CP-A1: single source of truth for layout / control / type tokens. Every
// demo and self-painted control should read these instead of writing raw
// pixel values. Resolve via dpi_scale before painting — these are logical
// units, not device pixels.

namespace nfui::design {

// 8px spacing system (logical units; resolve via dpi_scale before painting)
inline constexpr int spacing_xs = 4;
inline constexpr int spacing_sm = 8;
inline constexpr int spacing_md = 16;
inline constexpr int spacing_lg = 24;
inline constexpr int spacing_xl = 32;

// Control heights (logical)
inline constexpr int control_height_sm = 24;   // dense list rows
inline constexpr int control_height_md = 32;   // default inputs / buttons
inline constexpr int control_height_lg = 40;   // primary actions

// Corner radii
inline constexpr int radius_sm = 4;
inline constexpr int radius_md = 8;
inline constexpr int radius_lg = 12;

// Typography (font point sizes; converted to pixels via FontCache)
inline constexpr int font_caption  = 12;
inline constexpr int font_body     = 14;
inline constexpr int font_subtitle = 16;
inline constexpr int font_title    = 20;
inline constexpr int font_hero     = 28;

} // namespace nfui::design

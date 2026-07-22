#pragma once

#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

// Optional theme overrides for frame-level chrome (StatusBar, ProgressBar,
// Panel, Splitter; TabControl + Tooltip stay on native ComCtl32 chrome in V1).
// All fields are optional; when unset the component falls back to the injected
// ThemePalette defaults. The per-component consumption table today:
//
//   Panel       : surface_brush (fill), accent (hairline border)
//   Splitter    : surface_brush (idle fill); hover/dragging derive from palette
//   StatusBar   : surface_brush (fill)
//   ProgressBar : surface_brush (track), bar_color (fill, defaults to accent)
//
// `background` / `foreground` / `chrome_text` / `chrome_bg` are reserved for
// upcoming TabControl / Tooltip / TreeView custom-draw hooks (per
// docs/KNOWLEDGE/polish/2026-07-23-cp3-component-states.md, TabControl stays
// native for now). Field names are stable so consumers can set them ahead of
// the consumer-facing theming arriving.
struct FrameStyle {
    std::optional<Color> background;    // reserved: future TabControl tab fill
    std::optional<Color> foreground;    // reserved: future TabControl tab text
    std::optional<Color> accent;        // Panel hairline border (default: palette.border)
    std::optional<Color> surface_brush; // Panel / Splitter / StatusBar / ProgressBar track fill
    std::optional<Color> chrome_text;   // reserved: ListView / Tooltip / Tab text
    std::optional<Color> chrome_bg;     // reserved: ListView / Tooltip / Tab bg
    std::optional<Color> bar_color;     // ProgressBar fill (default: palette.accent)
    std::optional<int>   elevation;     // CP16: 0 (flat, default) / 1 / 2 / 3 — Panel-only; ignored elsewhere
};

} // namespace nfui

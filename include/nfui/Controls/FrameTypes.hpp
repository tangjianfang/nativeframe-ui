#pragma once

#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

// Optional theme overrides for frame-level chrome (StatusBar, ProgressBar,
// Panel, Splitter, Tooltip). All fields are optional; when unset the component
// falls back to the injected ThemePalette defaults. The per-component
// consumption table today:
//
//   Panel       : surface_brush (fill), accent (hairline border), elevation
//   Splitter    : surface_brush (idle fill); hover/dragging derive from palette
//   StatusBar   : surface_brush (fill)
//   ProgressBar : surface_brush (track), bar_color (fill, defaults to accent)
//   Tooltip     : chrome_text (tip text), chrome_bg (tip fill), accent (border),
//                 balloon (TTS_BALLOON at create; opts back into native paint)
//
// `background` / `foreground` remain reserved for future consumers. Field
// names are stable so consumers can set them ahead of further adoption.
struct FrameStyle {
    std::optional<Color> background;    // reserved: future consumers
    std::optional<Color> foreground;    // reserved: future consumers
    std::optional<Color> accent;        // Panel hairline / Tooltip border (default: palette.border)
    std::optional<Color> surface_brush; // Panel / Splitter / StatusBar / ProgressBar track fill
    std::optional<Color> chrome_text;   // Tooltip tip text (default: palette.text)
    std::optional<Color> chrome_bg;     // Tooltip tip fill (default: palette.surface)
    std::optional<Color> bar_color;     // ProgressBar fill (default: palette.accent)
    std::optional<int>   elevation;     // CP16: 0 (flat, default) / 1 / 2 / 3 — Panel-only; ignored elsewhere
    std::optional<bool>  balloon;        // CP19: Tooltip only — opt into TTS_BALLOON at create
};

} // namespace nfui

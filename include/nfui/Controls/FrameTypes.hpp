#pragma once

#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

// Optional theme overrides for frame-level chrome (TabControl, Tooltip,
// ProgressBar, Panel, Splitter). All fields are optional; when unset the
// component falls back to the injected ThemePalette defaults. The semantic
// intent of each field is documented per-component:
//   - background / foreground: window-level chrome text/background surfaces
//     (e.g. TabControl tabs).
//   - accent / bar_color: ProgressBar fill colour (defaults to palette accent).
//   - surface_brush: solid fill for chrome surfaces like Panel / Splitter.
//   - chrome_text / chrome_bg: ComCtl32 SetXxxColor APIs (ListView, Tooltip,
//     TabControl tabs where supported).
struct FrameStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> accent;
    // P1.4 fields (P1.4-1.6 chrome theming):
    std::optional<Color> surface_brush; // Panel / Splitter solid fill
    std::optional<Color> chrome_text;   // ListView / TreeView / Tab / Tooltip text
    std::optional<Color> chrome_bg;     // ListView / TreeView / Tab / Tooltip bg
    std::optional<Color> bar_color;     // ProgressBar fill (default: palette.accent)
};

} // namespace nfui

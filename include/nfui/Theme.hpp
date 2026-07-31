#pragma once

#include <array>
#include <cstddef>
#include <windows.h>

namespace nfui {

enum class ThemeMode {
    system,
    light,
    dark,
    high_contrast,
};

struct Color {
    COLORREF rgb{};
};

// CP17: straight per-channel lerp between two colors (a at t=0, b at t=1).
// Lives with Color (theme layer) rather than in Paint so the theme layer can
// cross-fade palettes without depending upward on paint. t is clamped to
// [0,1] so callers can pass a raw eased value. Distinct from paint's
// alpha_blend, which composites src over dst.
[[nodiscard]] Color lerp_color(Color a, Color b, float t) noexcept;

struct ThemeTokens {           // back-compat, lighter view
    COLORREF window_background{};
    COLORREF window_text{};
    COLORREF accent{};
};

struct ThemePalette {
    Color background;          // window chrome
    Color surface;             // cards / panels
    Color surface_hover;
    Color surface_variant;     // CP-B17: elevated / nested surface (alt rows, tabs, cards)
    Color border;
    Color divider;             // CP-B16: hairline separator (distinct from border)
    Color text;
    Color text_secondary;
    Color accent;
    Color accent_hover;
    Color accent_text;         // text drawn on accent
    Color selection;
    Color selection_text;
    Color danger;
    Color success;
    Color warning;
    Color info;                // CP32: informational/blue semantic used by samples
    Color shadow;              // CP16: tint used by paint_drop_shadow; alpha is overridden by the helper
};

struct ThemeMetrics {
    int corner_radius_control{6};   // buttons, inputs
    int corner_radius_card{10};      // panels, cards
    int border_width{1};
    int spacing{8};                  // base spacing unit
    int row_height{28};              // list row baseline
};

// CP-A1: per-control visual states. Each control maps its current state to
// one of these so a single palette resolver produces every chrome variant
// that the self-painted controls need (CP-A2/A3/A4 all consume StatePalette).
//
// Note: the brief originally named the resting case `default`. `default` is
// a C++ keyword (used in `switch ... default:` and `= default;` member
// functions) and cannot legally be an enumerator in MSVC /permissive-, so
// the closest non-reserved synonym `rest` is used here. Consumers should
// treat it as the no-special-interaction baseline.
enum class ControlState {
    rest,
    hover,
    pressed,
    focused,
    disabled,
    error,
};

// CP-A1: per-state palette derived from a base ThemePalette. Used by every
// self-painted control to fill bg / border / fg consistently across the
// three application themes. The `accent` field is what the focus ring or
// pressed highlight resolves to — it swaps with the state so a control can
// always paint a single accent derivative without having to branch on
// mode + state itself.
struct StatePalette {
    Color background;
    Color border;
    Color foreground;
    Color accent;       // focus ring / pressed highlight
};

// CP-A1: resolve a StatePalette from a base ThemePalette for the requested
// state. HC mode defers background/foreground/border to GetSysColor so
// high-contrast users see the OS-correct WCAG colours regardless of any
// theme the application has injected. The HC branch is selected by inspecting
// the palette itself (via `is_high_contrast(base)`) rather than a separate
// mode argument — this keeps the signature compact and prevents call sites
// from having to thread a mode value that the resolver ignores in non-HC
// cases. UI thread (no exceptions).
[[nodiscard]] StatePalette state_palette(const ThemePalette& base,
                                         ControlState state) noexcept;

[[nodiscard]] ThemeTokens  theme_tokens(ThemeMode mode) noexcept;   // back-compat
[[nodiscard]] ThemePalette theme_palette(ThemeMode mode) noexcept;
[[nodiscard]] ThemeMetrics theme_metrics() noexcept;

// Detect the current system theme by querying the Windows registry
// (AppsUseLightTheme) and high-contrast system parameter. Returns
// ThemeMode::dark, ThemeMode::light, or ThemeMode::high_contrast.
// Never returns ThemeMode::system. Thread-safe, no HWND required.
[[nodiscard]] ThemeMode detect_system_theme_mode() noexcept;

// Resolve ThemeMode::system to the detected system mode; all other
// modes pass through unchanged. Convenience wrapper around
// detect_system_theme_mode() for callers that hold a user preference.
[[nodiscard]] ThemeMode resolve_theme_mode(ThemeMode preferred) noexcept;

// CP31: 8-color categorical palette for chart series. Returns the full array
// for a mode so callers can build legends, or use chart_series_color() below
// to pick a single series color by index.
[[nodiscard]] const std::array<Color, 8>& chart_series_palette(ThemeMode mode) noexcept;
[[nodiscard]] Color chart_series_color(ThemeMode mode, std::size_t index) noexcept;

// True when the palette behaves like the WCAG high-contrast profile:
// a near-black background paired with a near-white foreground and a
// bright, saturated accent. Callers use this to switch control paint
// formulas (Button pressed/disabled, etc.) into paths that satisfy the
// 3:1 UI-component and 7:1 text thresholds that the standard light/dark
// formulas cannot reach against extreme accents.
[[nodiscard]] bool is_high_contrast(const ThemePalette& p) noexcept;

// CP17: per-field lerp between two palettes (a at t=0, b at t=1). Used by
// samples to cross-fade a theme switch over ~200 ms: each tick builds an
// interpolated palette and hands it to Control::set_palette. Radius/spacing
// (ThemeMetrics) do not change across themes, so only colors interpolate.
[[nodiscard]] ThemePalette lerp_palette(const ThemePalette& a, const ThemePalette& b, float t) noexcept;

// CP42: disable system visual theming for an HWND so self-painted common
// controls are not overdrawn by the native comctl32 theme background (e.g.
// the light empty area of a ListView or the light tabs of a TabControl in
// dark mode). No-op on Windows versions without uxtheme.dll.
void theme_disable_window_theme(HWND hwnd) noexcept;

} // namespace nfui

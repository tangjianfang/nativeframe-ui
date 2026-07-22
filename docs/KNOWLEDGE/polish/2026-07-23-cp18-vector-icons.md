# CP18 — Vector Icon System

**Date:** 2026-07-23
**Branch:** polish/CP18-vector-icons
**Roadmap:** CP15→CP19 visual polish, item CP18.

## Goal

A lightweight, theme-aware, DPI-scaled vector icon system so common UI
glyphs (chevrons, check, close, plus/minus, search, gear, info, warning,
dot, hamburger) are drawn from GDI primitives — not a TrueType icon font,
not GDI+, not raster `HICON` resources. Replaces the one hand-rolled glyph
(ComboBox dropdown triangle) and gives Button / IconView a vector mode.

## Constraints (from CLAUDE.md / ARCHITECTURE.md)

- No GDI+ / Direct2D (V1 non-goals). GDI primitives only.
- Layered: the helper needs `ThemePalette` (for the `Color` type and
  palette-aware colour resolution), so it must live where it may depend on
  `theme`. `icon` may depend on `core` only → **cannot** hold the helper.
  `paint` may depend on `core,theme` and already hosts `fill_polygon` /
  `draw_polyline` / `draw_line` → **paint is the home**.
- Logical-vs-device: glyphs are authored on a 16×16 **logical** grid and
  scaled into the device-pixel target rect at draw time. Nothing persists
  physical pixels.
- `noexcept` paint helpers; no exceptions cross any callback boundary.

## Design

### `include/nfui/VectorIcon.hpp` (paint module)

```cpp
enum class IconKind { none, chevron_down, chevron_up, chevron_left, chevron_right,
                      check, close, plus, minus, search, gear, info, warning,
                      dot, hamburger };
void draw_vector_icon(HDC, IconKind, const RECT& bounds, Color color,
                      int stroke_width) noexcept;
```

- Maps the 16×16 logical grid into the largest aspect-preserving square
  inside `bounds`, centred (non-square cells letterbox, never stretch).
- `color` is caller-resolved from the palette (matches the existing
  paint-helper convention — callers resolve palette → `Color`).
- `stroke_width` is in **device** px (caller scales via `DpiScale`).
- Filled glyphs (chevrons, dot) ignore `stroke_width`.
- `IconKind::none` and a degenerate rect are no-ops.

### Glyph geometry (16×16 logical grid)

| Glyph | Primitive | Points / shape |
|---|---|---|
| chevron_down/up/left/right | `fill_polygon` triangle | (4,6)(12,6)(8,11) etc. |
| check | `draw_polyline` 3-pt | (3,8)(7,12)(13,4) |
| close | two `draw_line` | (4,4)-(12,12), (12,4)-(4,12) |
| plus / minus | `draw_line` | vertical+horizontal / horizontal |
| search | `draw_ellipse` ring + `draw_line` handle | ring (3,3)-(11,11), handle (10,10)-(14,14) |
| gear | 3 rails (`draw_line`) + 3 knob `fill_ellipse` | "sliders" settings glyph |
| info | `draw_ellipse` ring + `fill_ellipse` dot + `draw_line` stem | ring (2,2)-(14,14) |
| warning | `draw_polyline` triangle outline + line + `fill_ellipse` dot | (8,2)(15,14)(1,14) + "!" |
| dot | `fill_ellipse` | (5,5)-(11,11) |
| hamburger | three `draw_line` | y=4,8,12 |

### New paint primitives

`fill_ellipse(HDC, RECT, Color)` and `draw_ellipse(HDC, RECT, Color, int width)`
in `Paint.hpp`/`Paint.cpp`. `fill_ellipse` selects `NULL_PEN` (pure fill);
`draw_ellipse` selects `NULL_BRUSH` (pure stroke). Both restore + delete
GDI objects and no-op on degenerate rects, matching the rect helpers.

### Consumers

- **ComboBox** (`paint_chrome`): the hand-rolled arrow triangle is replaced
  by `draw_vector_icon(IconKind::chevron_down, …)`. Cell = `max(8, dpi/8)`
  device px, centred on the dropdown button. Colour unchanged
  (`text_secondary`, disabled alpha-blended → background).
- **Button**: `ButtonStyle::icon` (`std::optional<IconKind>`) + optional
  `icon_color`. When set, a 16-logical-px glyph is drawn left of the
  caption and the caption rect is narrowed (`DT_CENTER` in the remaining
  space) so the icon + text do not overlap. Icon colour defaults to the
  caption colour so it tracks disabled / HC-pressed the same way the text
  does.
- **IconView**: `set_glyph(IconKind, Color)` — vector mode takes priority
  over the raster `HICON` when set; `IconKind::none` falls back to the
  raster icon. Disabled glyph is dimmed via the established
  `alpha_blend(color, background, 0.55)` pattern (GDI primitives have no
  `DST_ICON|DSS_DISABLED` equivalent).

### Showcase sample

`NativeFrameUIIconGallery` — a card grid (custom `WM_PAINT`, paint-layer
API) rendering all 14 glyphs with semantic colours (warning→`warning`,
check→`success`, info→`accent`), plus a row of real icon `Button`s
(`Button::set_icon`) and Light/Dark/High-Contrast toggle buttons that
re-palette both the custom-painted grid and the framework controls live.

## Test plan

`tests/nativeframeui_smoke.cpp` draws every glyph into a `MemoryDC` and
asserts each paints ≥1 foreground pixel (the sentinel-pixel idiom); asserts
`IconKind::none` paints nothing; asserts a degenerate rect is a no-op.
Architecture check maps `VectorIcon.hpp → paint`; `verify_boundaries` still
passes (no BCG/MFC/ATL/WTL). All three ctest tests green.

## Non-goals

- No TrueType icon font, no GDI+/Direct2D.
- No owner-draw CheckBox/RadioButton check glyph (those stay native for V1;
  flagged as a future opt-in — would require taking over `BS_AUTOCHECK*`
  painting).
- No tab close button (TabControl stays native for V1).
- No animated glyphs (CP17 animation system is orthogonal; a glyph could
  later ride `on_animation_tick` if needed).
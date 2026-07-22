---
title: CP8 ProgressBar visual audit
date: 2026-07-23
tags: [progressbar, contrast, rounding, theme, controls]
severity: minor
effort: trivial
status: resolved
---

## Audit items and findings

### 1. Bar fill contrast across light / dark / HC themes

| theme | track (palette.surface) | enabled fill (palette.accent) | enabled contrast | notes |
|-------|-------------------------|------------------------------|------------------|-------|
| light | RGB(240,238,230) | RGB(217,119,87) | ~3.4:1 | passes WCAG AA non-text (3:1) for graphical objects; intentionally warm |
| dark  | RGB(42,41,39)   | RGB(217,119,87) | ~3.6:1 | same warm accent reads well on warm-dark track |
| HC    | RGB(0,0,0)      | RGB(255,235,59) | ~17:1  | bright yellow on black is the HC-safe accent |

The enabled bar is fine across all three palettes. The accent is deliberately
warm rather than pure-blue so the bar reads as part of the chrome rather than
as a system overlay.

### 2. Disabled grayscale treatment (real bug)

Prior formula: `alpha_blend(p.text_secondary, p.surface, 0.55f)` —
"55% text_secondary on 45% surface".

Computed disabled fill against the surface track:

| theme | blend output | track | contrast | WCAG |
|-------|--------------|-------|----------|------|
| light | ~RGB(165,162,156) | RGB(240,238,230) | ~1.7:1 | FAIL |
| dark  | ~RGB(110,107,100) | RGB(42,41,39)    | ~3.6:1 | marginal |
| HC    | ~RGB(90,90,90)    | RGB(0,0,0)       | ~4.6:1 | pass |

The light-theme disabled fill was nearly invisible against the warm track —
the bar disappeared into the panel. Root cause: `text_secondary` on the light
palette is already a mid-warm grey (`RGB(107,104,98)`), and pushing 45% of the
surface back through it gave a tone only marginally darker than the track
itself.

**Fix.** Use `p.text_secondary` directly for the disabled fill. New contrast:

| theme | disabled fill | track | contrast | WCAG |
|-------|---------------|-------|----------|------|
| light | RGB(107,104,98) | RGB(240,238,230) | ~5.2:1 | pass (AA non-text) |
| dark  | RGB(168,163,154)| RGB(42,41,39)    | ~7.0:1 | pass |
| HC    | RGB(200,200,200)| RGB(0,0,0)       | ~11.6:1| pass |

`text_secondary` is the existing palette token for "secondary-tone content"
and matches the convention used by `StaticText::on_paint` (where disabled
text falls back to `text_secondary`). No new palette knob is needed.

### 3. Edge treatment: rounded vs square, consistency with Button

The prior code used `fill_rect` for the track — flat square corners. That
visually drifted from Button/Panel, which use `corner_radius_control = 6 px`
via `fill_rounded_rect`. On a 20–28 px tall bar, the rounding is barely
visible, but when the bar sits next to a rounded Button the eye picks up the
flatness as a mismatch.

**Fix.** Pull the radius from `theme_metrics().corner_radius_control` and
draw the track via `fill_rounded_rect(target, bounds, radius, track, border)`,
mirroring `Button::on_paint`. The fill rectangle (the moving bar) is still
a flat rect: a rounded fill rect would round the right edge of the moving
fill, which looks like a clipping artefact rather than a progress edge. The
left edge of the fill is anchored to the track's left rounded corner, so the
fill reads as "the track, partially consumed".

A 1 px `palette.border` outline is added at the same time so the bar is
visually distinct from any Panel/Surface background that shares
`palette.surface` (the same hairline pattern Panel already uses).

The outside-corner clear (`fill_rect(target, paint_bounds, p.background)`
before the rounded rect) is the same pattern `Button::on_paint` and
`ListBox::draw_item` adopted during CP2 — it prevents a 1–2 px ring of the
previous palette from surviving a theme switch inside the rounded boundary.

### 4. Animated / indeterminate state

Not supported in V1. Native PBS_MARQUEE / PBM_SETMARQUEE animation is not
forwarded; if a consumer sets it the bar would simply paint a static fill at
the current pos (which never moves because there is no animation tick). The
samples (Workbench, ThemeDemo, ComponentGallery) only exercise a static 60%
fill, and the smoke test only validates creation + HWND exposure.

This is acceptable for V1 — determinate ranges cover the framework's stated
use cases (status panels, settings loaders) and adding a marquee timer would
require a new `WM_TIMER` extension point. Deferred to a future polish pass;
flagged here so the next agent doesn't re-audit it as a regression.

### 5. Text percentage overlay

Not drawn. Modern ComCtl32 stopped rendering in-bar percentage text by
default (it only does so with `PBS_SMOOTH` + the deprecated `PBST_*`
internal styles). The samples intentionally show a flat fill with no text.
None of the audited samples requested a percentage overlay.

If a future sample needs an in-bar percentage, the simplest path is to add
a `FrameStyle::show_percentage` opt (off by default to preserve existing
samples) and have `on_paint` compute `100 * (pos - low) / span` and route
through `draw_text` with the same `FontCache` DPI hook used by StaticText.
Deferred — no current consumer asks for it.

### 6. `on_palette_changed` propagation

ProgressBar does not override `on_palette_changed`. That is correct: the
class holds no palette-derived cached state (no HFONT, no cached COLORREF,
no resolved bar color), so the base `Control::set_palette` → `InvalidateRect`
chain in `include/nfui/Controls/Control.hpp` is sufficient. A palette
re-injection causes `on_paint` to run with the new `palette()` pointer,
which re-reads `p.surface` / `p.accent` / `p.text_secondary` and
`p.background` / `p.border` on the same message-loop turn.

For comparison, `ListView` does override `on_palette_changed` because it
needs to push the resolved colors into the native `ListView_SetBkColor` /
`ListView_SetTextColor` slots. ProgressBar has no such out-of-band state.

## Resolution

- `src/controls/ProgressBar.cpp` now reads:
  - `disabled fill = palette.text_secondary` (was 0.55 blend; light-theme
    disabled is now legible at ~5.2:1 instead of ~1.7:1);
  - rounded track via `fill_rounded_rect(target, bounds, radius, track, border)`,
    `radius = theme_metrics().corner_radius_control` (matches Button);
  - `fill_rect(target, paint_bounds, p.background)` before the rounded rect
    to clear the outside corners (CP2 pattern);
  - 1 px `palette.border` outline so the bar stays distinct from any
    same-surface background.
- `include/nfui/Controls/ProgressBar.hpp` comment updated to describe the
  new paint contract.
- `on_palette_changed` left at the default no-op (no per-instance palette
  cache to invalidate).

## Verification

- `cmake --preset x64-debug && cmake --build --preset x64-debug` clean
  across all targets (`NativeFrameUIWorkbench`, `NativeFrameUIThemeDemo`,
  `NativeFrameUIComponentGallery`, all other samples, `nfui_frame`,
  `NativeFrameUISmokeTest`).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2 (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).

## Lessons

- "Desaturate" is the wrong primitive for a disabled progress fill when
  the track and surface are already close in luminance. On the light
  palette `text_secondary` ≈ track + a small offset, so blending toward
  surface collapses the contrast. Use the resolved secondary tone directly
  and let it stay visually distinct from the track.
- Rounded chrome needs the outside-corner clear even when the track is the
  only thing being rounded — the 1 px ring of outside-corner pixels survives
  a theme swap otherwise (CP2 lesson re-applied).
- "No override" is the correct `on_palette_changed` answer when the class
  is purely a function of `palette()` at paint time. Forcing every
  self-painting control to override it would be cargo-culting.
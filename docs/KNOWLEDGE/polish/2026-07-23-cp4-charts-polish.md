---
title: nfui_charts polish audit (CP4)
date: 2026-07-23
tags: [polish, charts, audit]
status: resolved
---

## Context

CP4 polish iteration #3 audited the `nfui_charts` module (both `nfui_charts`
and `nfui_charts_aa` static libraries) plus the V1 product sample
`samples/NativeFrameUICharts`. Scope per the iteration brief:

1. Chart main module (`src/charts/`) — visual consistency: grid colors, axis
   text, data series colors, hover/selected feedback, disabled state.
2. AA rendering (`src/charts/aa` = `nfui_charts_aa`) — font AA, corner
   smoothing, scale-induced aliasing.
3. Font consistency — re-use `fonts()->regular(dpi, 9)` vs bespoke HFONT
   retrieval; font-size parity; bold usage.
4. Theme transition — `set_palette()` entry point, auto-repaint on theme
   swap.

## Findings

### F1 — Legend series-name labels used the wrong font [FIXED]

`src/charts/internal/ChartsPaint.cpp` rendered the legend's series-name
labels (e.g. "Revenue", "Monthly revenue (USDk)", "Sensor A (Hz)") with
`fonts->mono(dpi, 9)` (Cascadia Code). Mono is the right choice for the
**numeric axis tick labels** ("0", "1", "2.5", "12%") — they read as
column-aligned tabular data. But the series-name column is **prose**, not
numeric data, so it should match the rest of the chrome's body font
(`fonts->regular(dpi, 9)` Segoe UI) the way Button, ListBox, StaticText,
and the sample banner already do.

The Visual Studio comparison vs `samples/NativeFrameUICharts` chrome (which
already uses `fonts_.regular(dpi_value, 9)` for the body and "Vertical
bar — monthly revenue" caption strips) showed the inconsistency clearly:
the legend labels were the only chrome in the demo using Cascadia Code.

**Fix**: introduce `kLegendUseMonoFont = false` so the helper picks
`fonts->regular(dpi, 9)` by default. The constant is a deliberate toggle
point so a future caller can flip the legend back to mono without
re-introducing a parallel helper.

### F2 — Placeholder tick font size diverged from the real tick font size [FIXED]

`src/charts/ChartView.cpp:53` (the C2 default placeholder) still asked for
`mono(dpi, 8)` while every C3/C4 view (`BarChartView`, `HBarChartView`,
`LineChartView`, `SplineChartView`) asks for `mono(dpi, 9)`. The comment on
`BarChartView.cpp:18` already documents why: *"Real labels carry formatted
floats and benefit from a touch more weight."* That rationale applies to
the placeholder too — the placeholder is only exercised in tests today,
but keeping it consistent means any future regression that causes the
default paint to fall through doesn't silently downgrade to point 8.

**Fix**: bump the placeholder's font point from `8` to `9`.

### F3 — Plot frame / axis ticks / legend boxes all draw with `palette.border` [DOCUMENTED, NOT FIXED]

All four chart views draw the plot frame, axis tick marks, and legend
column border with `palette.border` (light theme = `RGB(219, 215, 204)`,
dark = `RGB(61, 60, 54)`). The brief asked whether the grid should use
`palette.border` or a hypothetical `palette.divider` — there is no
`divider` token in `ThemePalette` (`include/nfui/Theme.hpp:24-39`). Adding
one would be an API change and is out of scope for a polish pass.

The current `border` value is already a deliberately soft hue (the light
theme pairs it with `RGB(232, 229, 219)` surface), so it reads as a chart
outline, not a hard separator. No change.

### F4 — Axis tick labels use `palette.text_secondary`; legend series names use `palette.text` [DOCUMENTED, NOT FIXED]

Every tick label in all four chart views is drawn with
`palette.text_secondary`; the legend column's series-name label uses
`palette.text`. This is consistent with the rest of the framework's
typographic hierarchy: body chrome uses `text`, captions / secondary
metadata uses `text_secondary`. No change.

### F5 — Chart series colors are caller-supplied, not palette-derived [DOCUMENTED]

`ChartSeries::color` is set by the caller (the sample uses
`palette_.accent` / `palette_.success` / `palette_.warning`). The chart
module itself never picks a series color. This is the correct factoring —
a charting API must let the caller pin colors that don't rotate with the
palette (a single-series chart pinned to brand-accent should not flip
when the dark theme swaps success for something else). The sample's
choice of three palette accents is also defensible: `accent` reads as the
"primary" series, `success` and `warning` as secondary / tertiary. No
change.

### F6 — No hover / selected state on chart data points [DOCUMENTED, OUT OF SCOPE]

The chart module does not implement `on_mouse_move` hover tracking on
individual data points (markers / bars / line vertices). The brief asked
whether such a state should exist; `docs/CHARTS.md` confirms it is a V2
feature. Implementing it now would require: mouse hit-testing against
`normalize_points` output, a small hover overlay rect, and a
`WM_MOUSEMOVE` / `WM_MOUSELEAVE` handler on `ChartView`. That is a feature
change, not a polish change, so it is left for V2.

`ChartView` does implement `WM_ERASEBKGND` suppression (returns 1) and
`WM_PRINTCLIENT` passthrough — the R6 polish infrastructure that lets the
self-paint path stay clean.

### F7 — No disabled state [DOCUMENTED, NOT APPLICABLE]

Charts are read-only views. There is no `enabled` / `disabled` state in
the V1 surface and none of the consumers need one. Not applicable.

### F8 — AA rendering quality is correctly tuned [DOCUMENTED]

`src/charts/ChartsPaint.cpp` uses `Gdiplus::SmoothingModeAntiAlias` plus
`PixelOffsetModeHalf` for the AA primitives. `PixelOffsetModeHalf` nudges
strokes half a pixel so integer-aligned lines do not ghost between two
rows of device pixels at integer scales — this is the documented
Microsoft guidance for crisp AA output at typical DPI. No text goes
through the AA path (all `draw_text` calls use GDI's `DrawTextW`,
consistent with the rest of the framework). The bar / hbar renderers
remain pure GDI; the comment in `docs/CHARTS.md:79` documents why: their
axis-aligned rects and rounded corners are already crisp.

### F9 — Theme transition: `set_palette()` exists, re-read on every paint [DOCUMENTED]

`ChartView::set_palette(const ThemePalette*)` exists
(`include/nfui/Charts.hpp:117`) and `inject_theme(palette, fonts)`
(`include/nfui/Charts.hpp:123`) is a one-call setter pair. The paint
path dereferences the `palette_` pointer on every `on_paint` invocation
(no color caching), so a theme transition that swaps the pointed-at
palette takes effect on the next `InvalidateRect`.

The V1 sample does **not** exercise a theme toggle — its
`nfui::ThemePalette palette_` member is constructed once from
`theme_palette(ThemeMode::light)` at startup and never replaced. That is
intentional: the Charts sample focuses on multi-chart layout, not theme
switching (Showcase is the theme-toggle sample). The setter infrastructure
is in place for any future theme-aware consumer.

### F10 — Tick mark length is 3 raw pixels, not DPI-scaled [DOCUMENTED, NOT FIXED]

The axis tick marks (`pb.left - 3` to `pb.left`, and `pb.bottom` to
`pb.bottom + 3`) are drawn at a hardcoded 3-px length in
`BarChartView.cpp:53`, `HBarChartView.cpp:55`, `LineChartView.cpp:53`,
`LineChartView.cpp:77`, `SplineChartView.cpp:55`, `SplineChartView.cpp:78`.
At 96 DPI this is fine; at 200% DPI (192) it is still 3 px and reads as a
slightly shorter stub relative to the DPI-scaled axis label height.

Fixing this would require plumbing a `DpiScale` through every internal
tick helper (currently they take `dpi` for font sizing only) and
multiplying the 3 by the scale factor. The change is mechanical but
touches every chart view's `on_paint` signature for an inner-only
constant. Considered too invasive for a polish pass; logged here for a
future V2 polish iteration that batches all DPI-rescaling of internal
constants together.

### F11 — Sample (`samples/NativeFrameUICharts`) audit [CLEAN]

`samples/NativeFrameUICharts/NativeFrameUICharts.cpp` uses
`palette_.accent` / `palette_.success` / `palette_.warning` for the chart
series colors and `fonts_.regular(dpi_value, 9)` for chrome text. No
hardcoded RGB literals (P6.1 audit already covered this). The 2x2 chart
grid layout is consistent with the other gallery samples. Clean.

## Files changed

| File | Change |
| --- | --- |
| `src/charts/internal/ChartsPaint.cpp` | F1: legend series-name font switched from `mono` to `regular` via a `kLegendUseMonoFont` toggle. |
| `src/charts/ChartView.cpp` | F2: placeholder tick font point bumped from `8` to `9` to match C3/C4. |

## Verification

```
cmake --build --preset x64-debug --target nfui_charts nfui_charts_aa NativeFrameUICharts
cmake --build --preset x64-debug --target NativeFrameUISmokeTest
NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure
```

All three chart targets compile clean (no new warnings); both CTest
entries pass (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).

## Lesson

The chart module is already a clean palette-driven surface; the polish
gap was a **font-choice** inconsistency (legend used mono where every
other chrome uses regular) and a **font-size** drift between the C2
placeholder and its C3/C4 overrides. Both are textbook polish items:
they only show up when the demo runs and a human looks at it. A future
"audit-by-visual-diff" loop would catch them at PR time.

Theme transition, hover state, and DPI-scaled tick lengths are real
debt items but require feature work (theme toggle) or invasive plumbing
(DPI through internal helpers). Neither belongs in a polish pass.
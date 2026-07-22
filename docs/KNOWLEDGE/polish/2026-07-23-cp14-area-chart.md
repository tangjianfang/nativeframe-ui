---
title: CP14 AreaChartView new chart kind
date: 2026-07-23
tags: [charts, new-feature, polish, cp14]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-22-charts-split-recommendation.md
  - 2026-07-23-nfui-charts-revisit-decision.md
---

# CP14 — AreaChartView new chart kind

Branch: `polish/CP14-area-chart-kind`.

Adds a fifth chart kind, `ChartKind::area`, to `nfui_charts` alongside the
existing `bar_vertical` / `bar_horizontal` / `line` / `spline` kinds.

## What it does

`AreaChartView` paints a filled polygon spanning the normalized line of
each series down to the plot baseline (`y == y.min`). The data shape is
identical to `LineChartView` (same `ChartSeries` + `ChartAxisRange` API),
so consumers can swap a line view for an area view by changing only the
subclass.

| Public method | Behaviour |
|---|---|
| `set_outline(bool)` | When true (default), draw a 1-px stroke in the original series color along the upper edge of the fill. Set false for pure-fill area (useful when stacking many series). |
| `set_fill_alpha(double)` | Blend factor in [0, 1]; clamped. 1.0 returns the original series color; 0.5 yields a 50%-toward-surface blend. |

The fill color tracks `palette.surface` so dark / light / HC themes all
render with the same tonal weight. Stacked area charts draw back-to-front
so the upper series wins visually (matches the `nfui::alpha_blend` convention
used by `Button::on_paint` pressed/disabled states).

## What did NOT change

Per the `2026-07-22-charts-split-recommendation.md` decision (DO-NOT-SPLIT
ratified by the layered-architecture spec §2.1 line 129), the `nfui_charts`
library boundary stays monolithic. The new kind joins the four existing
kinds in the same `.lib`; the per-chart split remains a V2 move.

Per the `2026-07-23-nfui-charts-revisit-decision.md` doc, V1 stays at the
"five chart kinds max" budget. Adding `area` keeps that promise — six or
more kinds would justify re-opening the split question.

The shared geometry helpers (`compute_chart_layout`, `normalize_points`,
`format_axis_tick`) needed no change: the area renderer reads `axis_x` and
`axis_y` exactly like `LineChartView`. `compute_chart_layout`'s single
`bar_horizontal` branch is the only kind-aware path, and `area` falls into
the default "non-horizontal-bar" case.

The three chart-axis tick helpers (`draw_value_axis_ticks_v`,
`draw_index_axis_ticks_v`, `draw_plot_frame`) are duplicated into
`AreaChartView.cpp`. The duplication is the minimum honest cost: lifting
them into `internal/ChartsPaint.cpp` would create a fifth identical
copy once a sixth chart kind lands. The decision to lift them is
deferred to the same V2 trigger that would justify a per-chart split.

## Files changed

- `include/nfui/Charts.hpp` — added `ChartKind::area` enum value; added
  `AreaChartView` class declaration with `set_outline` / `set_fill_alpha`
  public methods.
- `src/charts/AreaChartView.cpp` — new (~190 LOC). Patterned after
  `LineChartView` with the polygon-fill augmentation.
- `cmake/nfui_charts.cmake` — added `AreaChartView.cpp` to the
  `nfui_charts` source list (alphabetical order alongside the other
  five kinds).
- `tests/nativeframeui_smoke.cpp` — added an `AreaChartView` block
  mirroring the `LineChartView` block (~25 LOC). Exercises both
  `set_outline(true) + set_fill_alpha(1.0)` and
  `set_outline(false) + set_fill_alpha(0.5)` paint cycles to validate the
  alpha-blend branch.
- `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` — added
  `nfui::AreaChartView area_` member; added a "Monthly active users"
  series; expanded the 2×2 grid to 3×2 to accommodate the new cell;
  bumped the window width 1000→1200 so the third column has room.

## Verification

- `cmake --build --preset x64-debug` — clean. All twelve sample
  executables + smoke test link.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure` —
  **3/3** tests pass (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`, `NativeFrameUIArchitectureCheck`). The
  new `AreaChartView` block in the smoke test exercises both the
  opaque-fill+outline and the alpha-blend+no-outline paths.
- Smoke launch: `./out/build/x64-debug/Debug/NativeFrameUICharts.exe`
  runs to a stable event loop for at least 3 s; the 3×2 grid lays out
  correctly and the area chart paints alongside the four existing
  kinds.

## Lessons

- **One new chart kind is a ~190-LOC + 25-LOC smoke addition** when the
  shared helpers and `ChartLayout` dispatch are already in place. That
  confirms the DO-NOT-SPLIT recommendation from the chart-split review:
  adding kinds is cheap because the foundation is shared.
- **`alpha_blend(series.color, palette.surface, t)` is the right fill
  recipe** for a palette-aware area chart. Pre-tinting the series color
  in `set_series` would force consumers to recompute on every
  `set_palette`; computing at paint time keeps the tint in lockstep with
  the active palette (DarkStudio can show the same data series with a
  different fill weight without re-binding anything).
- **Stacked area draws back-to-front** so the upper series occludes the
  lower one. Without the ordering, the first series in the vector always
  wins regardless of which has the higher y range, producing visually
  inverted stacks.
- **The boundary check still passes** with zero modifications because the
  new file is in the same `src/charts/` directory; the architecture check
  policy maps `nfui_charts` to its allowed `nfui_core` / `nfui_theme` /
  `nfui_window` deps.
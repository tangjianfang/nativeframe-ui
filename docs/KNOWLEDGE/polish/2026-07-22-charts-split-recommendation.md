---
title: nfui_charts per-component split recommendation
date: 2026-07-22
tags: [charts, architecture, review]
severity: cosmetic
effort: large
status: open
---

## Current state

`nfui_charts` exposes **four chart kinds** in V1: `bar_vertical` (BarChartView),
`bar_horizontal` (HBarChartView), `line` (LineChartView), and `spline`
(SplineChartView). The chart-kind switch is exposed both as a `ChartKind` enum
and as four concrete `ChartView` subclasses, each with its own `on_paint`
override. No `Area`, `Pie`, `Candle`, `Heatmap`, or other V2 kinds are present.

| Metric | Value |
| --- | --- |
| Public classes | 5 (`ChartView` + 4 subclasses) |
| Internal classes | 1 (`charts_internal::GdiplusContext`) |
| Header files | 1 public (`include/nfui/Charts.hpp`, 251 LOC), 2 internal |
| Source files | 7 in `src/charts/`, 2 in `src/charts/internal/` |
| Total LOC (header + sources) | **1814** |
| `nfui_charts` library LOC | ~1330 (8 TUs) |
| `nfui_charts_aa` library LOC | ~229 (ChartsPaint.cpp + GdiplusContext.cpp/hpp, 167 + 57 + 53 + 47 = 324; minus the `nfui_charts` copy of ChartsPaint.cpp) |
| Test coverage | `tests/nativeframeui_smoke.cpp` exercises all 4 subclasses (lines 539-629) |

### Per-class source breakdown

| Class | File | LOC | AA path |
| --- | --- | --- | --- |
| `ChartView` (base) | `src/charts/ChartView.cpp` | 160 | pure GDI |
| `BarChartView` | `src/charts/BarChartView.cpp` | 232 | pure GDI |
| `HBarChartView` | `src/charts/HBarChartView.cpp` | 246 | pure GDI |
| `LineChartView` | `src/charts/LineChartView.cpp` | 151 | GDI+ when AA active |
| `SplineChartView` | `src/charts/SplineChartView.cpp` | 150 | GDI+ when AA active |
| Pure helpers | `src/charts/Charts.cpp` | 224 | n/a |
| AA primitives + public AA init | `src/charts/ChartsPaint.cpp` | 167 | GDI+ with GDI fallback |
| Legend column (shared) | `src/charts/internal/ChartsPaint.cpp` | 76 | n/a |
| GDI+ context | `src/charts/internal/GdiplusContext.cpp` | 57 | n/a |

### Cross-class dependencies

The four `ChartView` subclasses share several helpers that would have to be
re-hosted by a per-chart split:

| Helper | Defined in | Used by |
| --- | --- | --- |
| `compute_chart_layout(RECT, ChartKind, size_t)` | `Charts.cpp` | **all 4** subclasses |
| `format_axis_tick(double, wstring_view)` | `Charts.cpp` | **all 4** subclasses |
| `normalize_points(...)` | `Charts.cpp` | `LineChartView`, `SplineChartView` |
| `compute_bar_geometry(...)` | `Charts.cpp` | `BarChartView`, `HBarChartView` |
| `catmull_rom_to_bezier(...)` | `Charts.cpp` | `SplineChartView` |
| `charts_internal::draw_legend_column(...)` | `internal/ChartsPaint.cpp` | **all 4** subclasses |
| `charts_internal::draw_polyline_aa(...)` | `ChartsPaint.cpp` | `LineChartView` |
| `charts_internal::draw_beziers_aa(...)` | `ChartsPaint.cpp` | `SplineChartView` |
| `charts_internal::fill_circles_aa(...)` | `ChartsPaint.cpp` | `LineChartView` |

Every chart kind reaches into the shared `Charts.cpp` helpers and the shared
`internal/ChartsPaint.cpp` legend-column helper. The `ChartView` base class
itself is the common ancestor of all four kinds; `ChartKind`, `ChartSeries`,
`ChartPoint`, `ChartAxisRange`, and `ChartLayout` are public types consumed by
every kind.

There are also **duplicated** helpers inside the four `.cpp` files that have
not yet been lifted into a shared internal header:

| Helper | Bar | HBar | Line | Spline |
| --- | --- | --- | --- | --- |
| `draw_plot_frame` | yes | yes | yes | yes |
| `draw_value_axis_ticks_v` | yes | — | yes | yes |
| `draw_value_axis_ticks_h` | — | yes | — | — |
| `draw_category_axis_ticks_v` | yes | — | — | — |
| `draw_category_axis_ticks_h` | — | yes | — | — |
| `draw_index_axis_ticks_v` | — | — | yes | yes |
| `kTickFontPt`, `kTickCount`, `kAxisLabelGutter` constants | yes | yes | yes | yes |

This duplication is a real piece of polish debt — but it argues for **lifting
into a shared helper module**, not for splitting the library. (See
`docs/KNOWLEDGE/polish/INDEX.md` open items.)

### `nfui_charts_aa` relationship

`nfui_charts_aa` is the **opt-in antialiasing sibling** of `nfui_charts`:

- Compiles `src/charts/ChartsPaint.cpp` (the AA-aware polyline / bezier /
  circles primitives + `nfui::initialize_chart_aa` / `nfui::shutdown_chart_aa`)
  plus `src/charts/internal/GdiplusContext.cpp`.
- `PUBLIC` links `NativeFrameUI::nfui_charts` so the `nfui::ChartView` symbols
  resolve; `PRIVATE` links `gdiplus` so the GDI+ runtime stays scoped.
- Declarations for `initialize_chart_aa` / `shutdown_chart_aa` still live in
  `include/nfui/Charts.hpp` (the public consumer-facing header); only the
  definitions + the GDI+ context live in `nfui_charts_aa`.
- Without `nfui_charts_aa`, line/spline renderers silently fall back to GDI
  `Polyline` / `PolyBezier`; bar/hbar are pure GDI and unaffected.

This is the **one** boundary the layered-architecture spec chose to draw inside
the chart module: GDI+ consumers vs. GDI-only consumers. There is no second
boundary that the current consumer surface benefits from.

### Per-sample usage

| Sample | Uses |
| --- | --- |
| `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` | All four kinds in a 2x2 grid (`BarChartView`, `HBarChartView`, `LineChartView`, `SplineChartView`) |
| `tests/nativeframeui_smoke.cpp` | All four kinds (one hidden-window create/paint block per kind) |
| `samples/NativeFrameUIWorkbench`, `Showcase`, `DarkStudio`, `SettingsDemo`, `ResourceGallery`, `Controls` | None — they don't link `nfui_charts` |

Only **one** product sample uses charts, and it uses **all four kinds**. There
is no "bars-only" or "line-only" consumer to motivate a per-kind lib.

## Per-component split evaluation

### Arguments FOR splitting

- **Per-kind TUs already exist.** Each chart kind is already its own
  `src/charts/<Kind>ChartView.cpp`. The split would only re-home them into
  separate CMake targets and (optionally) separate headers. Mechanical work,
  not architectural surgery.
- **Polish surface is per-kind.** Bar polish (rounded corners, gap-ratio
  tuning, stacked-vs-grouped geometry) is independent from line polish (point
  markers, AA pen width) and spline polish (tension clamp). A library
  boundary would let one kind's iteration invalidate only its own compile
  cycle.
- **Failure isolation would be precise.** A regression in
  `SplineChartView::on_paint` would not touch `BarChartView`'s compile unit
  (today the monolithic `nfui_charts` target rebuilds on any kind change).
- **Aligns with the rest of the layered split.** `nfui_button`, `nfui_text`,
  `nfui_listbox`, etc. are per-component. Per-chart libraries would extend the
  same pattern to the chart module.
- **Consumers could pull only the kinds they need.** In V2, if a consumer
  ships only a settings form (which uses bars) and not a sensor dashboard
  (which uses splines), the line+spline code wouldn't be linked. Today
  everything comes along.

### Arguments AGAINST splitting

- **Shared geometry helpers are intrinsic, not accidental.** `compute_chart_layout`,
  `normalize_points`, `compute_bar_geometry`, `catmull_rom_to_bezier`, and
  `format_axis_tick` live in `src/charts/Charts.cpp` and are called by
  every kind. A per-kind split would require one of:
  1. **A shared geometry lib** (`nfui_chart_geom` or similar) that re-creates
     the "one shared module" pattern at one level of indirection — same
     problem, more files.
  2. **Duplicating the helpers into each kind** — strictly worse.
  3. **Header-only helpers** with `inline` linkage — works for small pure
     helpers but inflates per-TU compile time and removes any meaningful
     boundary. Net negative on the isolation axis.
- **`ChartView` is the common ancestor of all four kinds.** Each subclass
  inherits `set_kind`, `set_series`, `set_axis_x`, `set_axis_y`,
  `set_palette`, `set_font_cache`, and the WM_PAINT / WM_PRINTCLIENT plumbing.
  Splitting forces consumers to either include the base header alongside the
  subclass header (defeating the boundary) or to repeat the setters in each
  subclass (duplicating state).
- **The shared internal helpers are already factored.** `internal/ChartsPaint.cpp`
  (`draw_legend_column`) and `internal/GdiplusContext.cpp` are exactly the
  kind of cross-kind helpers a per-kind split would need; they already exist.
  Splitting without first lifting the *non*-internalized duplicates
  (`draw_plot_frame`, `draw_value_axis_ticks_v`, etc.) would either leave
  them duplicated 4x in 4 libs (worse) or require a "ChartPaint base" lib
  that recreates the same structure again.
- **Header surface stays monolithic.** `include/nfui/Charts.hpp` is one
  header today. Splitting the *library* doesn't shrink the header — every
  subclass needs `ChartView`, `ChartKind`, `ChartSeries`, `ChartPoint`,
  `ChartAxisRange`, `ChartLayout`, and the pure helpers to compile. A
  per-kind header split (`Charts/BarChartView.hpp`, etc.) would be a
  separate (and orthogonal) refactor.
- **No consumer needs fewer kinds.** The only product sample that links
  `nfui_charts` is `NativeFrameUICharts`, which uses all four. No internal
  consumer links a subset. The "only link what you use" benefit is
  theoretical until a V2 product ships.
- **nfui_charts_aa already draws the load-bearing boundary.** The GDI+ vs
  pure-GDI distinction is the only consumer-visible choice that affects link
  cost. Bars are pure GDI and never need `gdiplus`; line + spline use GDI+
  when AA is active. `nfui_charts_aa` already separates these; a per-kind
  split would duplicate that work.
- **Per-lib LOC is small.** Each kind's TU is 150-246 LOC; the geometry
  helpers total 224 LOC; AA primitives + legend column total 243 LOC. Four
  per-kind libs would land at ~250-500 LOC each — well below the threshold
  where a library boundary pays for itself in build-system noise terms.
- **The layered-architecture spec explicitly ratified this decision.** Section
  2.1 (line 129): *"nfui_charts is kept as one lib for now (rather than
  per-chart-kind) because all four chart kinds share one geometry helper
  library (compute_chart_layout, catmull_rom_to_bezier) and one paint helper
  (draw_legend_column). Per-chart-kind split is a V2 move if individual
  chart polish demands it."*
- **CMake boilerplate cost is real.** Each per-component library today has
  ~20 LOC of CMake plus an `include(...)` line in the top-level. Adding 4
  per-kind libs (plus 1 shared-geometry lib, if we don't want to duplicate)
  adds ~120 LOC of CMake for ~1330 LOC of C++ — a 9% overhead in build
  system code for a 0% change in feature surface.
- **Existing duplication argues for extraction, not division.** The four-way
  duplication of `draw_plot_frame`, `draw_value_axis_ticks_v`, etc. is real
  polish debt, but the *fix* is to lift them into `internal/ChartsPaint.cpp`
  (alongside `draw_legend_column`), not to give each kind its own library
  and live with the duplication there.

### Recommendation

**Decision: DO-NOT-SPLIT**

Rationale: the geometry + legend + base-class helpers that all four kinds
share are intrinsic to the chart family, not accidental coupling. Splitting
without first extracting them would either duplicate them or re-create the
same shared module at one indirection level deeper. Splitting *after*
extracting them would re-introduce the same shared module under a different
name (`nfui_chart_base` or similar). The single lib plus the
already-extracted `internal/ChartsPaint.cpp` is the right factoring.

The **nfui_charts + nfui_charts_aa** boundary that the layered spec
introduced is the only consumer-visible choice that affects link cost
(GDI+ opt-in), and it's already in place. A per-kind split would not add a
second axis of choice — only a second axis of CMake files.

The layered-architecture spec ratifies this explicitly (§2.1 line 129):
"Per-chart-kind split is a V2 move if individual chart polish demands it."
Today no individual chart kind's polish demands a separate library.

### Implementation sketch (not pursued)

If a future V2 polish pass finds a per-chart library boundary useful (e.g.
because `BarChartView` and `SplineChartView` diverge enough that their tick
renderers need to evolve independently), the sketch would be:

```text
cmake/
  nfui_charts.cmake             ← umbrella (unchanged name; static-INTERFACE)
  nfui_chart_base.cmake         ← ChartView, ChartKind, ChartSeries,
                                  ChartAxisRange, ChartLayout, pure helpers
  nfui_chart_bar.cmake          ← BarChartView, HBarChartView
  nfui_chart_line.cmake         ← LineChartView, SplineChartView (AA-aware)
  nfui_charts_aa.cmake          ← GDI+ context (unchanged)

src/charts/
  Charts.cpp                    ← → src/chart/base/Charts.cpp
  ChartView.cpp                 ← → src/chart/base/ChartView.cpp
  BarChartView.cpp              ← → src/chart/bar/BarChartView.cpp
  HBarChartView.cpp             ← → src/chart/bar/HBarChartView.cpp
  LineChartView.cpp             ← → src/chart/line/LineChartView.cpp
  SplineChartView.cpp           ← → src/chart/line/SplineChartView.cpp
  ChartsPaint.cpp               ← → src/chart/base/ChartsPaint.cpp
  internal/                     ← → src/chart/base/internal/ (or src/chart/internal/)
    ChartsPaint.cpp             ← draw_legend_column (stays shared)
    ChartsPaint.hpp
    GdiplusContext.hpp
    GdiplusContext.cpp
```

Pre-conditions before any split:

1. **Lift `draw_plot_frame`, `draw_value_axis_ticks_v`,
   `draw_index_axis_ticks_v`, `kTickFontPt`, `kTickCount`, `kAxisLabelGutter`
   into `internal/ChartsPaint.cpp/.hpp`.** Currently duplicated 3-4x; a split
   without this step makes the duplication strictly worse.
2. **Decide whether bars and line/spline should be 2 libs or 3-4 libs.** 2
   libs share more (only the AA dispatch differs), 3-4 libs align with the
   rest of the layered split but multiply the CMake boilerplate.
3. **Update `samples/NativeFrameUICharts` link list** to pull the new per-kind
   libs.
4. **Update `tests/nativeframeui_smoke.cpp`** to link each new lib in the
   block that exercises its kind.

This work is **not pursued in P2.5** because the preconditions (extracting
the duplicated tick helpers) are themselves a polish pass that has not yet
been authorized, and the layered spec explicitly defers the per-chart split
to V2.

## Effort estimate

| Path | Effort |
| --- | --- |
| **DO-NOT-SPLIT (recommended)** | 0 LOC, 0 commits. No work needed. |
| **SPLIT-INTO-2-GROUPS (bars / line+spline)** | ~250 LOC moved + 3 new CMake files (umbrella, bars, lines) + 1 shared geometry internal lib (or pre-lift to `nfui_core`) + sample + smoke test link-list updates. ~1.5 days. |
| **SPLIT-INTO-4-PER-CHART-LIBS** | ~600 LOC moved + 5 new CMake files (umbrella, base, bar, hbar, line, spline) + sample + smoke test updates + internal helpers must be lifted first. ~2 days. |

The recommended path is free; both split paths are non-trivial and yield no
functional or build-isolation benefit that the existing `nfui_charts` +
`nfui_charts_aa` boundary doesn't already provide.

## Risk

- **Test coverage is unchanged either way.** The smoke test already exercises
  all four kinds (lines 539-629). Splitting would add ~4 link-line changes
  to the smoke test, no new assertions.
- **Sample fragmentation risk is low.** Only `NativeFrameUICharts` links
  `nfui_charts`, and it uses all four kinds. A split would require updating
  one link list, not many.
- **Downstream consumers:** there are **no documented external consumers**
  of `nfui_charts` in V1.x — `docs/INTEGRATION.md` lists only the in-tree
  samples and smoke test. The split, if pursued, is internal.
- **Polish debt (duplicated tick helpers)** is real but is orthogonal to
  the library-boundary question. It is tracked separately as a polish
  backlog item rather than as an argument for or against splitting.
- **Code review surface** for a split would touch every chart subclass file
  plus every `target_link_libraries` line for every sample. The diff would
  be mechanical but voluminous.

## Decision log

- 2026-07-22: initial review. Recommends **DO-NOT-SPLIT** because the shared
  geometry + base-class + legend-column helpers are intrinsic to the chart
  family, the per-chart split would re-create the same shared module at one
  indirection level deeper, no consumer needs a per-kind lib today, and the
  layered-architecture spec explicitly defers the per-chart split to V2.
  Baseline build + test confirmed green before review.
---
title: nfui_charts per-component split revisited
date: 2026-07-23
tags: [architecture, charts, decision]
status: same
refs: [polish/2026-07-22-charts-split-recommendation.md (P2.5)]
---

## Re-survey (2026-07-23)

Chart types present (unchanged from P2.5):
- BarChartView (vertical bars) — `src/charts/BarChartView.cpp`, 232 LOC
- HBarChartView (horizontal bars) — `src/charts/HBarChartView.cpp`, 246 LOC
- LineChartView (lines + filled circle markers) — `src/charts/LineChartView.cpp`, 151 LOC
- SplineChartView (cubic Catmull-Rom splines) — `src/charts/SplineChartView.cpp`, 150 LOC

No new chart kinds have been added since 2026-07-22. There are still
exactly four `ChartKind` enum values and exactly four subclasses of
`ChartView`.

### Shared vs chart-specific code

Shared code (~70% by LOC, unchanged from P2.5):
- `ChartView` base class — `src/charts/ChartView.cpp` (160 LOC)
- Pure geometry / format helpers — `src/charts/Charts.cpp` (224 LOC)
- AA-aware paint primitives (`draw_polyline_aa`, `draw_beziers_aa`,
  `fill_circles_aa`) — `src/charts/ChartsPaint.cpp` (167 LOC)
- `internal/ChartsPaint.cpp` legend column helper (76 LOC)
- `internal/GdiplusContext.cpp` GDI+ lifetime helper (57 LOC)

Chart-specific code (~30%): the four `*ChartView.cpp` files (779 LOC
total), each containing a `void on_paint(HDC, const RECT&)` override plus
kind-specific tick and bar geometry.

The pure helpers in `Charts.cpp` are called by all four subclasses
(`compute_chart_layout`, `format_axis_tick`); `normalize_points` is used
by Line + Spline; `compute_bar_geometry` is used by Bar + HBar;
`catmull_rom_to_bezier` is used only by Spline. The legend-column helper
(`draw_legend_column`) is used by all four. The duplicated tick-renderer
helpers (`draw_plot_frame`, `draw_value_axis_ticks_v`, the `kTick*`
constants) noted in P2.5 have **not** been lifted into a shared
`internal/ChartsPaint.cpp` module — the polish debt is still open.

### Library structure

- `nfui_charts` — monolithic, all 4 chart kinds plus shared geometry
  and paint helpers. `cmake/nfui_charts.cmake` lists 7 TUs; alias
  `NativeFrameUI::nfui_charts`.
- `nfui_charts_aa` — GDI+ AA renderer, separate. PUBLIC links
  `NativeFrameUI::nfui_charts`; PRIVATE links `gdiplus`. Alias
  `NativeFrameUI::nfui_charts_aa`. Two TUs.

This split is unchanged from P2.5 and remains the layered-architecture
spec's only library boundary inside the chart module.

### Consumers

- `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` — uses ALL FOUR
  chart kinds in a 2x2 grid (Bar, HBar, Line, Spline). This is the
  only sample that links `nfui_charts`.
- `tests/nativeframeui_smoke.cpp` — exercises all four kinds (one
  hidden-window create/paint block per kind) when
  `NativeFrameUI::nfui_charts_aa` is linked. Test consumer only.
- `samples/NativeFrameUIWorkbench`, `Showcase`, `DarkStudio`,
  `SettingsDemo`, `ResourceGallery`, `Controls`, `Minimal` — none of
  these link `nfui_charts` (verified by repo-wide grep for chart API
  symbols; only the Charts sample and the smoke test show up).
- No third-party or external consumers identified; `docs/INTEGRATION.md`
  enumerates only the in-tree samples and tests.

**Per-chart linking benefit: still theoretical.** The only consumer that
links `nfui_charts` uses all four kinds, so a per-kind split would not
let it pull a smaller subset. No second consumer has appeared since
P2.5 that wants only a subset.

## Decision

**No change from P2.5**: keep `nfui_charts` monolithic.

### Rationale

1. **The 4 chart kinds share ~70% code.** `compute_chart_layout`,
   `format_axis_tick`, `draw_legend_column`, and the AA primitives in
   `ChartsPaint.cpp` are reached by every subclass. Splitting would force
   consumers who want more than one kind to link multiple per-kind libs
   and still pull the shared helpers transitively — re-creating the
   "one shared module" pattern at one level of indirection (or
   duplicating the helpers, which is strictly worse).

2. **Only one consumer uses charts.** `NativeFrameUICharts` is the sole
   product sample that links `nfui_charts`, and it uses all four kinds.
   The per-component linking benefit (link what you use) is not realized
   today and cannot be realized until either a second consumer appears
   with a different subset, or charts gain substantially more kinds.

3. **`nfui_charts_aa` already provides a natural split boundary.** The
   load-bearing consumer-visible choice is GDI+ AA on vs. off. That
   boundary is already drawn: line + spline renderers fall back to
   pure GDI when `nfui_charts_aa` is not linked. Bars and HBar are pure
   GDI regardless. A per-kind split would add no second axis of choice —
   only a second axis of CMake files.

4. **Layered-architecture spec ratifies.** Section 2.1 of
   `docs/superpowers/specs/2026-07-22-layered-library-architecture-design.md`
   explicitly keeps `nfui_charts` monolithic and defers a per-chart-kind
   split to V2: *"Per-chart-kind split is a V2 move if individual chart
   polish demands it."* No such demand has materialized.

5. **Build cost is unchanged.** `nfui_charts.lib` build time is ~2 s on
   the current toolchain — well below the threshold where a split pays
   for itself in build-system noise terms.

### What stayed the same in the 30 days since P2.5

- Same 4 chart kinds, same LOC totals, same internal file layout.
- Same shared-helper profile (`Charts.cpp` + `internal/ChartsPaint.cpp`
  + `internal/GdiplusContext.cpp`).
- Same monolithic `nfui_charts` + opt-in `nfui_charts_aa` library
  structure.
- Same single consumer (`NativeFrameUICharts`) using all 4 kinds.
- The tick-helper polish debt noted in P2.5 (duplicated
  `draw_plot_frame`, `draw_value_axis_ticks_v`, `kTick*` constants)
  has not been lifted into `internal/ChartsPaint.cpp`. This remains an
  open polish item independent of the library-boundary question.

### What changed (none material)

- One additional smoke-test assertion exercising all four kinds via
  hidden-window create/destroy — pure test work, no API changes.
- A handful of doc-comment edits inside `include/nfui/Charts.hpp`
  (clarifications, no signature or class-set changes).
- No new chart kinds, no new consumers, no new CMake targets
  introducing a second boundary.

## Re-evaluation triggers

Re-evaluate the per-component split decision if any of the following
becomes true:

- **New self-contained chart kind added.** A kind that does not share
  the existing geometry or paint helpers (heatmap, scatter,
  candlestick, treemap, sankey) would tip the shared-code ratio below
  ~50% and motivate per-kind libraries.
- **Second consumer appears with a different subset.** A product sample
  or external consumer that links only Bar / only Line would let us
  realize the "link what you use" benefit.
- **`nfui_charts.lib` exceeds ~5 s incremental build time.** Today
  ~2 s; ~5 s is the threshold at which monolithic rebuild cost becomes
  a developer-friction complaint rather than background noise.
- **Current LOC exceeds 5 000.** Today ~1 814 (header + sources); 5 000
  is the threshold at which per-kind TUs would meaningfully shorten
  rebuilds.
- **`Charts.cpp` shared helpers grow past ~600 LOC.** Currently 224;
  crossing 600 would mean the "shared module" pattern itself wants its
  own library boundary, making the per-chart split cleaner to
  implement.

## Open questions

- **Should `nfui_charts` be split if/when charts exceed 4 kinds?** Maybe.
  A 6-kind scenario with low shared-helper overlap (e.g., heatmap +
  candlestick) could justify per-kind libs. The decision should be
  re-surveyed at that point.
- **Should `nfui_charts_aa` be promoted to default-on in V2.0?** It is
  currently opt-in via link target. AA is the user-visible improvement
  for line + spline; making it default would require either folding
  `ChartsPaint.cpp` into `nfui_charts` (and pulling `gdiplus` into
  the PUBLIC link list for every consumer) or moving
  `initialize_chart_aa` startup into the ApplicationContext.
- **Should the open tick-helper polish debt be paid before V2?** The
  `draw_plot_frame`, `draw_value_axis_ticks_v`, `draw_index_axis_ticks_v`
  duplication is real. Lifting them into `internal/ChartsPaint.cpp`
  would not change the library boundary, but it would make a future
  per-kind split a strictly cheaper refactor (no deduplication work
  needed before the cut).

## Effort reminder (from P2.5, still accurate)

| Path | Effort |
| --- | --- |
| **NO-SPLIT (this decision)** | 0 LOC, 0 commits. No work needed. |
| SPLIT-INTO-2-GROUPS (bars / line+spline) | ~250 LOC moved + 3 new CMake files + 1 shared-geometry internal lib + sample + smoke-test link-list updates. ~1.5 days. |
| SPLIT-INTO-4-PER-CHART-LIBS | ~600 LOC moved + 5 new CMake files (umbrella, base, bar, hbar, line, spline) + sample + smoke-test updates + tick helpers must be lifted first. ~2 days. |

## Decision log

- 2026-07-22: P2.5 initial review. Recommends **DO-NOT-SPLIT**.
  Rationale: shared geometry + base-class + legend-column helpers are
  intrinsic to the chart family; per-chart split would re-create the
  same shared module at one indirection level deeper; no consumer
  needs a per-kind lib; layered-architecture spec explicitly defers
  per-chart split to V2.
- 2026-07-23: P7.3 revisit (this document). Re-survey finds **no
  material change** since P2.5: same 4 chart kinds, same shared-helper
  profile, same monolithic structure, same single consumer using all
  four kinds. Decision **unchanged**: `nfui_charts` stays monolithic.
  Re-evaluation triggers and open questions recorded for the next
  per-chart-kind survey.

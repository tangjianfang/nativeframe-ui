# NativeFrame UI Known Limitations

## V1.2 charts module

V1.2 introduces the `nfui_charts` module plus the `NativeFrameUICharts` sample gallery (2x2 grid: vertical bar / horizontal bar / line / spline). Renderer behaviour, lifecycle, and smoke-test coverage are documented in [Charts Guide](CHARTS.md).

- Four chart kinds ship in V1.2: `bar_vertical` (`BarChartView`), `bar_horizontal` (`HBarChartView`), `line` (`LineChartView`), and `spline` (`SplineChartView`). Area / filled-stack / scatter are V2.
- Renderers are split into pure HWND-free geometry helpers (`compute_chart_layout`, `normalize_points`, `compute_bar_geometry`, `catmull_rom_to_bezier`) plus thin `on_paint` overrides on the four `ChartView` subclasses. The pure helpers are exercised by the smoke test without ever touching a window, so the geometry math is unit-testable in isolation from paint lifecycle.
- `PolyBezier` point count invariant: `catmull_rom_to_bezier(n)` returns `4*(n-1)` `POINT`s (one cubic Bezier segment per input gap, four control points each). `PolyBezier` consumes `3*(n-1)+1` control points per polyline, which is `4*(n-1)` — they match exactly because each cubic segment shares its endpoint with the next.
- `SplineChartView` is smooth-only: point markers are intentionally omitted because they fight the Catmull-Rom curve and produce a muddy result. `LineChartView` keeps markers via `set_point_radius(0)` to disable.
- `set_stacked(true)` on `BarChartView` / `HBarChartView` is a V2 TODO (currently a no-op); V1.2 ships un-stacked / grouped bars only. The setter exists so consumer code can declare intent and stay binary-compatible when stacking lands.

## V1.1 refresh (Claude Code UI restyle)

V1.1 is a visual and paint-lifecycle refresh; the public `nfui` API surface and V1 scope are unchanged.

- All five product-growth demos (`Showcase`, `DarkStudio`, `SettingsDemo`, `ResourceGallery`) plus the `NativeFrameUIControls` gallery now consume the shared `nfui::ThemePalette` (warm cream `#FAF9F5` light / warm ink `#1F1E1D` dark / coral `#D97757` accent) and `nfui::FontCache` (serif headings via `serif()`, mono digits via `mono()`). No demo hand-rolls its own palette or font.
- `MemoryDC` scope-before-`EndPaint` invariant: the buffer is created at the top of `WM_PAINT`, painted into, flushed via `BitBlt`, and torn down by the destructor before `EndPaint` returns. This is the only correct lifecycle for flicker-free owner-draw / custom-draw paint. See commits `dd4768f`, `15f25e6`, `380a330`, `84038c1`, `fc8df43` for the SettingsDemo / Workbench / Controls gallery migrations and follow-ups.
- `NativeFrameUIControls` ListBox re-applies its font on DPI change (R9 follow-up `84038c1`) so the cached `HFONT` is refreshed alongside the rest of the typography.
- DPI handlers **must** re-apply `WM_SETFONT` to every native child control (ListBox / Edit / ComboBox / StatusBar). Creation-time font binding alone is not sufficient — the child's HFONT is frozen at the DPI present when `create_child` ran, and a DPI move leaves the control visually out of step. Owner-draw controls re-query `dpi_of(hwnd())` on every paint and therefore do not need an explicit `WM_SETFONT` re-apply.

## Current V1 baseline limitations

- No Ribbon.
- No full docking or floating panes.
- No visual designer.
- No complete Property Grid.
- No complete Data Grid.
- No plugin system.
- No MFC, ATL/WTL, BCG, or BCG-compatible API surface.
- Dark theme support is a full `ThemePalette` (background, surface, surface hover, border, text, secondary text, accent, accent hover, accent text, selection, selection text, danger, success, warning) consumed by owner-draw and custom-draw controls. Per-row hover highlighting and listview column-header theming remain deferred.
- Showcase and demo visuals are sample-local evaluation surfaces that consume the shared theme/paint/font primitives (`theme_palette`, `fill_rounded_rect`, `draw_text`, `FontCache`); stable framework guarantees remain the documented `nfui` APIs, not sample pixel output.
- `FontCache` retains one regular and one semibold `HFONT` per face (serif, sans, mono), rebuilt when queried at a new DPI/point size. Per-paint consumers are unaffected; a long-lived `HFONT` obtained from the cache (e.g. stored via `WM_SETFONT`) can be invalidated when the cache is next queried at a different DPI. All demos now re-apply `WM_SETFONT` from their DPI handler to stay in sync.
- DarkStudio currently favors mouse-driven visual review over deeper keyboard command coverage.
- SettingsDemo exposes save-state intent but does not persist settings to disk yet.
- ResourceGallery reuses the shared framework asset file instead of demo-specific resources.
- UI automation coverage is documented for future expansion, not yet implemented.
- Manual compatibility matrix entries remain pending until run on the target OS/display combinations.


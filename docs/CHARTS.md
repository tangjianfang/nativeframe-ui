# Charts Module

> Status: **C2 delivered** (control + paint cycle). C3 (bar renderer) and C4 (line/spline renderer) follow.

The charts module is a self-painted HWND-based chart control built on top of `nfui::Window`. C2 wires the `nfui::ChartView` control, the paint cycle (MemoryDC scope-before-EndPaint), and the future `OCM_DRAWITEM` integration point.

## File Map

| Path | Role |
| --- | --- |
| `include/nfui/Charts.hpp` | Public API: `ChartKind`, `ChartPoint`, `ChartSeries`, `ChartAxisRange`, `ChartLayout`, pure helpers (`compute_chart_layout`, `normalize_points`, `compute_bar_geometry`, `catmull_rom_to_bezier`), and `ChartView`. |
| `src/charts/Charts.cpp` | Pure helper implementations (HWND-free, unit-testable). |
| `src/charts/ChartView.cpp` | `ChartView` control: create, message dispatch, paint cycle, default placeholder paint. |
| `cmake/nfui_charts.cmake` | Module CMake target; links `nfui_core` + `nfui_theme`. |

## `ChartView` Lifecycle

1. **Construct.** Caller declares a `ChartView` instance (typically as a member of the owner `Window`).
2. **Create.** `ChartView::create(WindowCreateParams)` registers the private `NativeFrameUIChartView` window class (because `Window::register_window_class` rejects the system `STATIC` class whose proc does not match `Window::window_proc`) and creates the HWND. A real subclass registration lands in a later task.
3. **Inject dependencies.** `set_palette(&palette)` and `set_font_cache(&fonts)` install borrowed pointers the paint path will read. The lifetime of the underlying `ThemePalette` / `FontCache` must outlive the chart window.
4. **Push data.** `set_kind`, `set_series`, `set_axis_x`, `set_axis_y` copy the inputs into the storage vectors. `std::wstring_view` payloads (e.g. `ChartSeries::name`) remain borrowed — the caller must keep the underlying string valid until the next paint.
5. **Paint.** The window receives `WM_PAINT` (or `WM_PRINTCLIENT` for owner-draw snapshots), enters the R6 nested-scope `MemoryDC` block, calls the virtual `on_paint(HDC, RECT)` to render the chart, then `EndPaint`. The destructor of `MemoryDC` flushes the buffer back into the BeginPaint DC before `EndPaint` invalidates it.
6. **Destroy.** `Window::destroy` (driven by `WM_NCDESTROY` or an explicit call) tears down the HWND.

## Render Pipeline

```
WM_PAINT
  -> BeginPaint
  -> GetClientRect
  -> {                         # nested scope: MemoryDC destructor flushes before EndPaint
       MemoryDC mem(hdc, client)
       target = mem.valid() ? mem.dc() : hdc
       on_paint(target, client)        # virtual; default = placeholder; C3/C4 override per kind
     }
  -> EndPaint
```

`on_paint` is `virtual` so subclasses (C3 bar, C4 line/spline) can replace the default placeholder with a call into the appropriate renderer (`draw_bar_chart`, `draw_line_chart`).

The default placeholder paints:
- `palette.background` over the full client area.
- A bordered plot frame inset by the axis gutter (32 px) and a legend strip (24 px).
- A legend box at the top-left of the plot.
- Mono `0..4` tick labels along x and y (mono font from the injected `FontCache`, falls back to `nullptr` when no cache is set).

## MemoryDC Scope-Before-EndPaint Invariant

`nfui::MemoryDC`'s constructor snapshots the target rect via `BitBlt`; the destructor BitBlts the modified buffer back into the target. If the destructor runs **after** `EndPaint`, the target DC is no longer valid and the flush silently fails (or, worse, accesses a released DC). The R6 fix (commits `dd4768f`, `15f25e6`, `380a330`, `fc8df43`) binds the `MemoryDC` lifetime to a nested scope that closes **before** `EndPaint`. `ChartView` mirrors the canonical pattern from `samples/NativeFrameUIControls/NativeFrameUIControls.cpp:114-138`.

## V1 Chart Kinds

`nfui::ChartKind` enumerates four kinds in V1:

| Kind | Renderer | Class | Notes |
| --- | --- | --- | --- |
| `bar_vertical` | grouped vertical bars | `BarChartView` | C3. `set_stacked(true)` is a V2 TODO; V1 ships un-stacked/grouped. |
| `bar_horizontal` | grouped horizontal bars | `HBarChartView` | C3. Plot width/height swapped via `compute_chart_layout(bar_horizontal)`. |
| `line` | polyline + markers | `LineChartView` | C4. `set_point_radius(logical_px)` controls marker radius; 0 disables markers. |
| `spline` | Catmull-Rom -> cubic Bezier | `SplineChartView` | C4. `set_tension(double)` in [0, 1], clamped. Markers intentionally omitted (see Renderers). |

**Area charts are V2.** The `compute_chart_layout` helper already reserves a legend column when more than one series is present, but the area-fill path is intentionally not yet implemented.

## Renderers

Each chart kind is its own `nfui::ChartView` subclass with a thin `on_paint` override that dispatches into the shared pure-geometry helpers. The renderer classes share the placeholder paint frame (legend box, axis gutter, mono tick labels) so multi-series charts read consistently when stacked side by side.

| Class | Header | Description | Key setters |
| --- | --- | --- | --- |
| `nfui::BarChartView` | `include/nfui/Charts.hpp` | Grouped vertical bars; sub-bars per x slot are placed side-by-side. The bars grow upward from the plot baseline toward the y maximum. | `set_series(std::vector<ChartSeries>)`, `set_axis_x(ChartAxisRange)`, `set_axis_y(ChartAxisRange)`, `set_stacked(bool)` (V2 TODO, currently a no-op) |
| `nfui::HBarChartView` | `include/nfui/Charts.hpp` | Grouped horizontal bars; sub-bars per y slot are stacked vertically within the slot and grow rightward from the plot baseline. | `set_series(std::vector<ChartSeries>)`, `set_axis_y(ChartAxisRange)` (carries the value range), `set_stacked(bool)` (V2 TODO) |
| `nfui::LineChartView` | `include/nfui/Charts.hpp` | One `Polyline` per series, optionally punctuated by filled circle markers. Multi-series line charts share the legend-column layout so they read identically to bar charts side-by-side. | `set_series(std::vector<ChartSeries>)`, `set_point_radius(int logical_px)`, `set_axis_x(ChartAxisRange)`, `set_axis_y(ChartAxisRange)` |
| `nfui::SplineChartView` | `include/nfui/Charts.hpp` | Each polyline is converted to cubic Bezier control points via `catmull_rom_to_bezier` and drawn with GDI `PolyBezier`. Markers are intentionally omitted — fixed point markers and a smooth curve fight each other and produce a muddy result. | `set_series(std::vector<ChartSeries>)`, `set_tension(double t)` (clamped to [0, 1]), `set_axis_x(ChartAxisRange)`, `set_axis_y(ChartAxisRange)` |

All four subclasses inherit `set_palette(const ThemePalette*)` and `set_font_cache(FontCache*)` from `nfui::ChartView`. The framework's `set_kind(ChartKind)` is retained as a no-op-style override so the sample gallery can swap chart kinds at runtime if a future cross-kind view lands; in V1 each subclass hard-wires its own kind in `on_paint`.

## Custom-Draw `OCM_DRAWITEM` Path

`ChartView` is a top-level `nfui::Window` rather than an `nfui::Control` subclass, so it does **not** receive `OCM_DRAWITEM` reflections today. C2 wires `WM_PRINTCLIENT` so a future `OCM_DRAWITEM` integration (where the parent reflects owner-draw notifications to the chart, or where `PrintWindow` snapshots the chart) can reuse the same `on_paint` target without touching the paint code. V1 keeps `WM_PAINT` as the only paint entrypoint; the `WM_PRINTCLIENT` handler is a no-op for the default placeholder but is wired so C3/C4 renderer subclasses get the path for free.

## Threading and Lifetime

- All members are touched only on the UI thread that owns the chart HWND. The setters do not post cross-thread messages.
- Setters copy into the storage vectors. `std::wstring_view` payloads (e.g. `ChartSeries::name`) are stored as borrowed views — the caller keeps ownership of the underlying string until the next paint (or destruction of the chart, whichever comes first).
- Exceptions are stopped at `handle_message`'s `try/catch` boundary (`Window::window_proc`), so paint code can rely on `noexcept` semantics even though the renderer paths are not decorated `noexcept` end-to-end.

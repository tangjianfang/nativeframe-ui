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

| Kind | Renderer (planned) | Notes |
| --- | --- | --- |
| `bar_vertical` | `draw_bar_chart(..., horizontal=false)` | C3. |
| `bar_horizontal` | `draw_bar_chart(..., horizontal=true)` | C3. |
| `line` | `draw_line_chart(..., ChartType::line)` | C4. Polyline + markers. |
| `spline` | `draw_line_chart(..., ChartType::spline)` | C4. Catmull-Rom -> cubic Bezier (`catmull_rom_to_bezier` helper already in place). |

**Area charts are V2.** The `compute_chart_layout` helper already reserves a legend column when more than one series is present, but the area-fill path is intentionally not yet implemented.

## Custom-Draw `OCM_DRAWITEM` Path

`ChartView` is a top-level `nfui::Window` rather than an `nfui::Control` subclass, so it does **not** receive `OCM_DRAWITEM` reflections today. C2 wires `WM_PRINTCLIENT` so a future `OCM_DRAWITEM` integration (where the parent reflects owner-draw notifications to the chart, or where `PrintWindow` snapshots the chart) can reuse the same `on_paint` target without touching the paint code. V1 keeps `WM_PAINT` as the only paint entrypoint; the `WM_PRINTCLIENT` handler is a no-op for the default placeholder but is wired so C3/C4 renderer subclasses get the path for free.

## Threading and Lifetime

- All members are touched only on the UI thread that owns the chart HWND. The setters do not post cross-thread messages.
- Setters copy into the storage vectors. `std::wstring_view` payloads (e.g. `ChartSeries::name`) are stored as borrowed views — the caller keeps ownership of the underlying string until the next paint (or destruction of the chart, whichever comes first).
- Exceptions are stopped at `handle_message`'s `try/catch` boundary (`Window::window_proc`), so paint code can rely on `noexcept` semantics even though the renderer paths are not decorated `noexcept` end-to-end.

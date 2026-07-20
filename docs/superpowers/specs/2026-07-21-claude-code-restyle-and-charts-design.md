# Claude Code Restyle + Charts Module — Design

**Date:** 2026-07-21
**Status:** Approved (user authorized autonomous execution; no further consultation)

## Context

The 2026-07-20 visual-controls plan is largely landed: a shared visual core (`theme_palette`, `FontCache`, `Paint`, `Icon`) and five self-painting controls (`Button`, `StaticText`, `ListBox`, `ListView`, `IconView`) exist, proven by `NativeFrameUIControls` and `ShowcaseView`. Two problems remain:

1. **The palette is generic Win32 blue** (`#006ED7` light / `#60A5FA` dark) with no brand identity — this is the "visuals look poor" feedback.
2. **Four samples still use sample-local hardcoded RGB painting** (`Workbench`, `DarkStudio`, `SettingsDemo`, `ResourceGallery`) with their own duplicated `blend`/`fill_rect_color`/`draw_round_panel`/`create_font` helpers — never migrated to shared primitives.
3. **There is zero chart/plot/axis code anywhere** — charts are entirely greenfield.

`BufferedPaintContext` was sketched in the prior plan but never landed; rendering is direct GDI and flickers.

## Goal (two halves)

1. **Claude Code UI restyle** — swap the global `theme_palette()` to Anthropic's warm palette (cream + ink + coral), extend `FontCache` with serif/mono faces and add `ThemeMetrics`, then migrate all six samples to the shared primitives so the whole suite reads as one branded system.
2. **Charts module** — a reusable, testable chart module (Approach C: pure renderer + thin control) covering V1 chart types: bar/column (+ horizontal bar) and line/spline (multi-series). Ship a `NativeFrameUICharts` gallery demo.

## Non-goals (V1)

- Area charts, pie/donut, scatter, stacked/histogram (deferred to V2).
- Direct2D/DirectWrite (per architecture V1 constraints) — charts use pure GDI (`Polygon`, `Polyline`, `Pie`, `PolyBezier`).
- Owner-drawing the still-native controls in `Workbench` (`TreeView`, `Edit`, `TabControl`, `ProgressBar`, `StatusBar`) — only the self-painting subset + window background are restyled there; native chrome stays. Documented as a limitation.

---

## Part 1 — Claude Code visual identity

### 1.1 Palette (replaces values in `src/theme/Theme.cpp`)

Maps to the existing 14 `ThemePalette` fields. Warm, no cool blue.

**Light**
| field | value | role |
|---|---|---|
| `background` | `#FAF9F5` | window chrome (cream) |
| `surface` | `#F0EEE6` | cards/panels (warm sand) |
| `surface_hover` | `#E8E5DB` | hover |
| `border` | `#DBD7CC` | warm tan hairline |
| `text` | `#1F1E1D` | ink body |
| `text_secondary` | `#6B6862` | warm gray subtext |
| `accent` | `#D97757` | **coral** (Anthropic signature) |
| `accent_hover` | `#C15F3F` | deep coral |
| `accent_text` | `#FFFFFF` | text on coral |
| `selection` | `#F2D6C8` | coral-tint selection |
| `selection_text` | `#1F1E1D` | |
| `danger` | `#C75450` | |
| `success` | `#4D7C5F` | warm green |
| `warning` | `#B8821F` | warm amber |

**Dark** (warm ink — the key differentiator from generic cool dark themes)
| field | value |
|---|---|
| `background` | `#1F1E1D` |
| `surface` | `#2A2927` |
| `surface_hover` | `#35342F` |
| `border` | `#3D3C36` |
| `text` | `#EDEDEB` |
| `text_secondary` | `#A8A39A` |
| `accent` | `#D97757` |
| `accent_hover` | `#E89478` |
| `accent_text` | `#FFFFFF` |
| `selection` | `#3A2E26` |
| `selection_text` | `#EDEDEB` |
| `danger` | `#E5736F` |
| `success` | `#6FAA82` |
| `warning` | `#D9A23A` |

**High contrast** stays black/white/yellow/red (unchanged).

### 1.2 Fonts (extend `FontCache`)

Add two faces to `FontCache` so the suite reads as "editorial Claude", not generic Segoe:

- `serif(dpi, point_size)` → **Georgia** (warm serif; brand/heading text). Windows-bundled.
- `mono(dpi, point_size)` → **Cascadia Code**, fallback **Consolas** (chart numerics, code-ish labels, KPI digits).
- `regular` / `semibold` stay Segoe UI for body text.

Cached per (dpi, weight, family) like today; `~FontCache` deletes all cached `HFONT`s.

### 1.3 Metrics (new `ThemeMetrics` in `Theme.hpp`)

Kills the sample-local magic numbers (`6`, `8`, `18`...).

```cpp
struct ThemeMetrics {
    int corner_radius_control{6};   // buttons, inputs
    int corner_radius_card{10};     // panels, cards
    int border_width{1};
    int spacing{8};                 // base spacing unit
    int row_height{28};             // list row baseline
};
[[nodiscard]] ThemeMetrics theme_metrics() noexcept;
```

### 1.4 Paint helpers (extend `Paint.hpp`)

Add the shared GDI primitives the migrated samples and charts need, plus the deferred flicker-free buffer:

- `fill_rect(HDC, RECT, Color)` — solid fill (replaces sample-local `fill_rect_color`).
- `draw_line(HDC, POINT a, POINT b, Color, int width)`.
- `fill_polygon(HDC, const POINT* pts, int count, Color fill, Color border)`.
- `draw_polyline(HDC, const POINT* pts, int count, Color, int width)`.
- `class MemoryDC` — RAII offscreen buffer: construct with `(HDC target, RECT bounds)`, paint to `dc()`, `BitBlt`s back to target on destruct. Works for any HDC (including owner-draw DCs with no HWND), so it also fixes the deferred flicker on buttons/labels.

---

## Part 2 — Charts module

### 2.1 Architecture (Approach C: pure renderer + thin control)

```
charts -> core, theme, dpi, paint, font
```

New module `src/charts/Charts.cpp` + `include/nfui/Charts.hpp`. No new forbidden dependencies; `core` still depends on nothing below it.

**Data model**
```cpp
struct ChartSeries {
    std::wstring label;
    std::vector<double> values;
    Color color;                 // zero-alpha => auto-assign from palette accent family
};
struct ChartData {
    std::vector<ChartSeries> series;
    std::vector<std::wstring> categories;   // x-axis labels; if empty, indices used
    double y_min{0};             // 0 => auto from data
    double y_max{0};              // 0 => auto (nice-rounded)
};
enum class ChartType { bar, line, spline };
```

**Pure helpers (unit-tested, no HWND)**
- `scale_value_to_pixel(double v, double vmin, double vmax, int pixel_span) noexcept -> int` — linear map, clamps when `vmax==vmin`.
- `nice_num(double range) noexcept -> double` — axis tick rounding (1/2/5 × 10^k).
- `nice_axis_range(double lo, double hi) noexcept -> {min,max}` — padded, rounded axis domain.
- `bar_geometry(rect, series_index, series_count, category_index, category_count, metrics) -> RECT` — exact bar rect (vertical or horizontal).

**Renderer (free functions, take an HDC + already-mapped geometry)**
- `draw_bar_chart(HDC, const ChartData&, const RECT& bounds, const ThemePalette&, const ThemeMetrics&, bool horizontal) noexcept`
- `draw_line_chart(HDC, const ChartData&, const RECT& bounds, const ThemePalette&, const ThemeMetrics&, ChartType) noexcept` — straight (`Polyline`) or smooth (`PolyBezier` through the points), multi-series, with axis, gridlines, ticks, legend, value labels.

**Control**
```cpp
class ChartView : public Control {
public:
    bool create(const ControlCreateParams&) noexcept;
    void set_data(ChartData data) noexcept;
    void set_type(ChartType t) noexcept;
    void set_horizontal(bool h) noexcept;   // bar only
protected:
    void on_paint(HDC, const PaintState&) noexcept override;
private:
    ChartData data_;
    ChartType type_{ChartType::bar};
    bool horizontal_{false};
};
```
`on_paint` builds a `MemoryDC` over the control rect, calls the renderer, and the subclass flushes. Mutators `InvalidateRect`. UI-thread-only mutation per architecture rules.

### 2.2 `NativeFrameUICharts` gallery demo (new, 7th sample)

Claude-Code-styled window, light/dark toggle, DPI-aware, showing four `ChartView`s:
1. Vertical bar — 3 series × 6 categories (e.g. "Adoption / Builds / Reviews" across Q1..Q6).
2. Horizontal bar — single series ranking.
3. Line — multi-series straight.
4. Spline — multi-series smooth curve.

Sidebar nav (like Showcase) + KPI tiles using `mono` font for digits + `serif` for the brand title. `WM_DPICHANGED` re-layout. `nfui_add_resources` per the executable rule.

---

## Part 3 — Sample migration

All six samples end at the shared primitives + Claude Code palette.

| Sample | Current state | Migration |
|---|---|---|
| `NativeFrameUIControls` | already themed | Use `ThemeMetrics` radii; add a light/dark toggle to showcase the palette switch. |
| `NativeFrameUIShowcase` | already shared-primitives | Auto palette upgrade; swap brand/KPI fonts to `serif`/`mono` for editorial feel. |
| `NativeFrameUIDarkStudio` | sample-local RGB + local helpers | Replace `draw_round_panel`/`create_font`/`blend` with `fill_rounded_rect`/`FontCache`/`alpha_blend`; feed `theme_palette(ThemeMode::dark)`. Full migration. |
| `NativeFrameUISettingsDemo` | local banner paint + native form | Migrate banner paint to shared primitives; inject `set_palette`/`set_font_cache` into `Button` (self-paints) and set Segoe UI on native `Edit`/`ComboBox`/`CheckBox`; paint window bg with `background`. |
| `NativeFrameUIResourceGallery` | local round panels | Replace local `draw_round_panel`/fonts with shared `fill_rounded_rect`/`FontCache`; use `theme_palette`. Full migration. |
| `NativeFrameUIWorkbench` | fully native unstyled | Inject `set_palette`/`set_font_cache` into the self-painting subset (`ListView`, `ListBox`, `Button`, `StaticText`); set Segoe UI on native controls; paint window background with `background`. Native chrome (TreeView/Edit/TabControl/ProgressBar/StatusBar) stays — documented limitation. |

---

## Part 4 — Tests (extend `tests/nativeframeui_smoke.cpp`)

Pure-logic assertions (no window needed for the math):
- `theme_metrics()` field defaults.
- New palette spot check: `theme_palette(ThemeMode::dark).accent == RGB(217,119,87)` and `theme_palette(ThemeMode::light).accent == RGB(217,119,87)` (coral `#D97757` is the same in both modes).
- `FontCache::serif`/`mono` return non-null `HFONT` (CreateFont works in console).
- `scale_value_to_pixel(0, 0,100, 200) == 0`; `(100, 0,100, 200) == 200`; `(50, 0,100, 200) == 100`; clamp when `vmax==vmin`.
- `nice_num`/`nice_axis_range` monotonic and rounded.
- `bar_geometry` rects are within bounds and non-overlapping across series in a category.
- `ChartView` create + `hwnd()` + resource-count stability (hidden window, like existing control tests).
- `NativeFrameUICharts.exe` emit check.

Visuals verified manually via screenshots (light + dark, 100/150% DPI).

---

## Part 5 — Docs

- `README.md`: add `NativeFrameUICharts.exe` to demo table; note Claude Code palette + charts module.
- `docs/KNOWN_LIMITATIONS.md`: update palette/visual-status lines; add the Workbench-native-chrome limitation and the "V1 charts = bar/line/spline only" scope.
- `docs/screenshots/`: capture `charts-light.png`, `charts-dark.png`, `controls-claude.png`, `showcase-claude.png`, `darkstudio-claude.png`, `settings-claude.png`, `resourcegallery-claude.png`.

---

## Dependency direction (respects `docs/ARCHITECTURE.md`)

```
theme   -> core, dpi, paint (Color alias only)
font    -> core, dpi
paint   -> core, Win32 GDI
icon    -> core, resource, dpi
charts  -> core, theme, dpi, paint, font
controls -> core, resource, theme, dpi, layout, paint, font, icon
sample  -> public modules
```

`core` depends on nothing below it. No new forbidden markers (boundary check stays green).

## Invariants preserved

- Stable object address until `WM_NCDESTROY`; no exceptions across any callback/WindowProc/paint; UI-thread-only mutation; logical-vs-device-pixel distinction in DPI code; `LONG_PTR` for pointer data; explicit handle ownership; no global singletons.
- Charts never call `GetLastError`/`GetProcAddress` directly (centralized). Renderer functions are `noexcept` and never throw across the paint callback.

## Exit criteria (loop until all true)

- [ ] Smoke test passes in Debug **and** Release with all new assertions.
- [ ] Boundary check green (no new forbidden markers).
- [ ] All six samples launch with the Claude Code warm palette at 100/150% DPI; light + dark where applicable.
- [ ] `NativeFrameUICharts.exe` renders bar (vertical + horizontal), line, and spline charts, multi-series, flicker-free.
- [ ] Pure chart math assertions pass (`scale_value_to_pixel`, `nice_axis_range`, `bar_geometry`).
- [ ] Screenshots captured; README + KNOWN_LIMITATIONS updated.
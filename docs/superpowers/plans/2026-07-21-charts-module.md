# Charts Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. After every task, run the Loop Verification block at the bottom; re-enter via `/loop` to repeat the cycle until the Charts exit criteria pass.

**Goal:** Add a reusable, testable charts module (`nfui::charts`) covering V1 chart types — bar/column (+ horizontal bar) and line/spline (multi-series) — plus a `NativeFrameUICharts` gallery demo in the Claude Code style.

**Architecture:** Approach C (pure renderer + thin control). `ChartRenderer` is a set of free functions that take an HDC + already-mapped geometry and draw; the value→pixel, axis-range, and bar-geometry math are pure `noexcept` helpers unit-tested in the smoke test without a window. `ChartView` is a thin `Control` subclass that holds a `ChartData` model and calls the renderer inside a `MemoryDC` on `WM_PAINT`. Charts depend on `core, theme, dpi, paint, font` — a new module below `controls`, never reached by `core`.

**Tech Stack:** C++20, Win32, GDI (`Polygon`, `Polyline`, `PolyBezier`, `Rectangle`, `Pie`-style fills), Common Controls, CMake Presets, CTest. No Direct2D/GDI+ (V1 constraint).

**Spec:** `docs/superpowers/specs/2026-07-21-claude-code-restyle-and-charts-design.md` (Part 2).

**Prerequisite:** The Restyle plan (`docs/superpowers/plans/2026-07-21-claude-code-restyle.md`) must be complete — charts consume `theme_metrics()`, `MemoryDC`, and the Claude Code palette.

---

## File Structure

**Created:**
- `include/nfui/Charts.hpp` — `ChartSeries`, `ChartData`, `ChartType`, pure helpers (`scale_value_to_pixel`, `nice_num`, `nice_axis_range`, `bar_geometry`), `ChartRenderer` free functions, `ChartView` control.
- `src/charts/Charts.cpp` — implementations.
- `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` — gallery demo.

**Modified:**
- `CMakeLists.txt` — add `src/charts/Charts.cpp` to the library; add `NativeFrameUICharts` executable target.
- `tests/nativeframeui_smoke.cpp` — append pure-logic + `ChartView` + emit assertions.
- `README.md`, `docs/KNOWN_LIMITATIONS.md` — note the charts module + V1 scope.

**Dependency direction (respects `docs/ARCHITECTURE.md`):**
```text
charts  -> core, theme, dpi, paint, font
sample  -> public modules
```
`core` still depends on nothing below it. No new forbidden markers (boundary check stays green).

---

## Task 1: Charts module skeleton + pure helpers

**Files:**
- Create: `include/nfui/Charts.hpp`
- Create: `src/charts/Charts.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** In `tests/nativeframeui_smoke.cpp`, immediately before the final `return ok ? 0 : 1;`, append:

```cpp
    {
        using namespace nfui;
        // scale_value_to_pixel: linear map over [vmin, vmax] into [0, pixel_span].
        ok = expect(scale_value_to_pixel(0,   0, 100, 200) == 0,   L"scale 0 -> 0") && ok;
        ok = expect(scale_value_to_pixel(100, 0, 100, 200) == 200, L"scale 100 -> 200") && ok;
        ok = expect(scale_value_to_pixel(50,  0, 100, 200) == 100, L"scale 50 -> 100 (midpoint)") && ok;
        ok = expect(scale_value_to_pixel(25,  0, 100, 200) == 50,  L"scale 25 -> 50") && ok;
        // degenerate range: vmin == vmax -> clamp to 0 (no divide-by-zero).
        ok = expect(scale_value_to_pixel(7,   5, 5, 200) == 0, L"degenerate range yields 0") && ok;
        // nice_num rounds to 1/2/5 x 10^k.
        ok = expect(nice_num(0.0) == 0.0, L"nice_num(0) == 0") && ok;
        ok = expect(nice_num(73.0) >= 70.0 && nice_num(73.0) <= 100.0, L"nice_num(73) in [70,100]") && ok;
        // nice_axis_range returns {min,max} with max > min and max >= data hi.
        auto r = nice_axis_range(0.0, 73.0);
        ok = expect(r.second > r.first, L"nice_axis_range max > min") && ok;
        ok = expect(r.second >= 73.0, L"nice_axis_range max >= data hi") && ok;
        ok = expect(r.first <= 0.0, L"nice_axis_range min <= data lo") && ok;
        // bar_geometry: 2 series x 3 categories, vertical, fits inside bounds, non-overlapping per category.
        RECT bounds{0, 0, 300, 200};
        RECT b0 = bar_geometry(bounds, /*series=*/0, /*series_count=*/2, /*cat=*/0, /*cat_count=*/3, theme_metrics(), /*horizontal=*/false);
        RECT b1 = bar_geometry(bounds, /*series=*/1, /*series_count=*/2, /*cat=*/0, /*cat_count=*/3, theme_metrics(), /*horizontal=*/false);
        ok = expect(b0.right > b0.left && b0.bottom > b0.top, L"bar 0 has positive area") && ok;
        ok = expect(b1.right > b1.left && b1.bottom > b1.top, L"bar 1 has positive area") && ok;
        // bars of different series in the same category are side-by-side (no x-overlap).
        ok = expect(b0.right <= b1.left || b1.right <= b0.left, L"series bars do not overlap in x") && ok;
        // both bars inside bounds.
        ok = expect(b0.left >= bounds.left && b0.right <= bounds.right, L"bar 0 within bounds x") && ok;
        ok = expect(b1.left >= bounds.left && b1.right <= bounds.right, L"bar 1 within bounds x") && ok;
    }
```

- [ ] **Step 2: Run test to verify it fails.** `cmake --build --preset x64-debug` → compile error: `scale_value_to_pixel`/`nice_num`/`nice_axis_range`/`bar_geometry` not declared.

- [ ] **Step 3: Write minimal implementation.** Create `include/nfui/Charts.hpp`:

```cpp
#pragma once

#include <nfui/Theme.hpp>
#include <nfui/Controls.hpp>   // Control base for ChartView

#include <string>
#include <vector>
#include <windows.h>

namespace nfui {

struct ChartSeries {
    std::wstring label;
    std::vector<double> values;
    Color color{};              // rgb == 0 => auto-assign by renderer
};

struct ChartData {
    std::vector<ChartSeries> series;
    std::vector<std::wstring> categories;   // x labels; empty => "0","1",...
    double y_min{0};            // 0 => auto from data
    double y_max{0};            // 0 => auto (nice-rounded)
};

enum class ChartType { bar, line, spline };

// Pure helpers (no HWND) — unit-tested.
[[nodiscard]] int scale_value_to_pixel(double v, double vmin, double vmax, int pixel_span) noexcept;
[[nodiscard]] double nice_num(double range) noexcept;                         // round to 1/2/5 x 10^k
struct AxisRange { double lo; double hi; };
[[nodiscard]] AxisRange nice_axis_range(double lo, double hi) noexcept;       // padded + rounded
[[nodiscard]] RECT bar_geometry(const RECT& bounds, int series_index, int series_count,
                                int category_index, int category_count,
                                const ThemeMetrics& metrics, bool horizontal) noexcept;

// Renderer (free functions). noexcept; never throw across the paint callback.
void draw_bar_chart(HDC dc, const ChartData& data, const RECT& bounds,
                    const ThemePalette& palette, const ThemeMetrics& metrics,
                    bool horizontal) noexcept;
void draw_line_chart(HDC dc, const ChartData& data, const RECT& bounds,
                     const ThemePalette& palette, const ThemeMetrics& metrics,
                     ChartType type) noexcept;

class ChartView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_data(ChartData data) noexcept;
    void set_type(ChartType t) noexcept;
    void set_horizontal(bool h) noexcept;
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    ChartData data_{};
    ChartType type_{ChartType::bar};
    bool horizontal_{false};
};

} // namespace nfui
```

Create `src/charts/Charts.cpp` with the pure helpers only (renderer + control come in later tasks):

```cpp
#include <nfui/Charts.hpp>
#include <nfui/Paint.hpp>

#include <algorithm>
#include <cmath>

namespace nfui {

int scale_value_to_pixel(double v, double vmin, double vmax, int pixel_span) noexcept {
    if (pixel_span <= 0) return 0;
    if (vmax <= vmin) return 0;                 // degenerate range -> clamp to origin
    double t = (v - vmin) / (vmax - vmin);
    if (t <= 0.0) return 0;
    if (t >= 1.0) return pixel_span;
    return static_cast<int>(t * pixel_span + 0.5);
}

double nice_num(double range) noexcept {
    if (!(range > 0.0)) return 0.0;
    double exp = std::floor(std::log10(range));
    double f = range / std::pow(10.0, exp);    // f in [1, 10)
    double nf;
    if (f < 1.5)      nf = 1;
    else if (f < 3.0) nf = 2;
    else if (f < 7.0) nf = 5;
    else             nf = 10;
    return nf * std::pow(10.0, exp);
}

AxisRange nice_axis_range(double lo, double hi) noexcept {
    if (hi <= lo) { hi = lo + 1; }
    double range = nice_num(hi - lo);
    double step  = range / 5.0;                 // ~5 ticks
    double nlo = std::floor(lo / step) * step;
    double nhi = std::ceil(hi / step) * step;
    return AxisRange{nlo, nhi};
}

RECT bar_geometry(const RECT& bounds, int series_index, int series_count,
                  int category_index, int category_count,
                  const ThemeMetrics& metrics, bool horizontal) noexcept {
    RECT r{};
    if (series_count <= 0 || category_count <= 0) return r;
    const int gap = std::max(2, metrics.spacing / 2);
    if (!horizontal) {
        // Vertical bars: category bands along x, series clustered side-by-side within a band.
        int band_w = (bounds.right - bounds.left) / category_count;
        int band_x = bounds.left + band_w * category_index;
        int cluster_w = band_w - gap * 2;
        int bar_w = cluster_w / series_count;
        int x = band_x + gap + bar_w * series_index;
        // Height filled by caller via scale_value_to_pixel; return full-height slot here.
        r.left   = x;
        r.right   = x + bar_w;
        r.top     = bounds.top;
        r.bottom  = bounds.bottom;
    } else {
        // Horizontal bars: category bands along y.
        int band_h = (bounds.bottom - bounds.top) / category_count;
        int band_y = bounds.top + band_h * category_index;
        int cluster_h = band_h - gap * 2;
        int bar_h = cluster_h / series_count;
        int y = band_y + gap + bar_h * series_index;
        r.top    = y;
        r.bottom = y + bar_h;
        r.left   = bounds.left;
        r.right  = bounds.right;
    }
    return r;
}

} // namespace nfui
```

Add `src/charts/Charts.cpp` to the `NativeFrameUI` `add_library` list in `CMakeLists.txt` (alphabetic, after `src/controls/Controls.cpp`). The file already compiles with the two renderer functions and `ChartView` declared but not yet defined — so provide stubs in `Charts.cpp` now to keep the build green:

```cpp
void draw_bar_chart(HDC, const ChartData&, const RECT&, const ThemePalette&, const ThemeMetrics&, bool) noexcept {}
void draw_line_chart(HDC, const ChartData&, const RECT&, const ThemePalette&, const ThemeMetrics&, ChartType) noexcept {}
bool ChartView::create(const ControlCreateParams& params) noexcept {
    return create_native(L"STATIC", params, SS_OWNERDRAW);   // painted via OCM_DRAWITEM like IconView
}
void ChartView::set_data(ChartData data) noexcept { data_ = std::move(data); InvalidateRect(hwnd(), nullptr, FALSE); }
void ChartView::set_type(ChartType t) noexcept { type_ = t; InvalidateRect(hwnd(), nullptr, FALSE); }
void ChartView::set_horizontal(bool h) noexcept { horizontal_ = h; InvalidateRect(hwnd(), nullptr, FALSE); }
void ChartView::on_paint(HDC dc, const PaintState&) noexcept { (void)dc; }
```

> `ChartView::create` uses `SS_OWNERDRAW` so the existing `Control` `OCM_DRAWITEM` dispatch routes `ODT_STATIC` to `on_paint` (already wired for `IconView`). Confirm `subclass_proc`'s `OCM_DRAWITEM` calls `control->on_paint` for `ODT_STATIC` — it does (the 2026-07-20 plan made `ODT_STATIC` call `on_paint`). If it currently only does `ODT_BUTTON`, extend it.

- [ ] **Step 4: Run test to verify it passes.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (pure-helper assertions green; `ChartView` stub compiles).

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Charts.hpp src/charts/Charts.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(charts): add module skeleton with pure scale/axis/bar-geometry helpers"
```

---

## Task 2: ChartView paint cycle test + OCM_DRAWITEM wiring

**Files:**
- Modify: `src/charts/Charts.cpp` (ChartView::on_paint real body — but still no renderer; just clear bg)
- Modify: `src/controls/Controls.cpp` (ensure `ODT_STATIC` → `on_paint`)
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append before the final `return`:

```cpp
    {
        using namespace nfui;
        ChartView cv;
        ControlCreateParams p{};
        p.instance = GetModuleHandleW(nullptr); p.parent = controls_parent.hwnd();
        p.control_id = 9101; p.x = 0; p.y = 0; p.width = 200; p.height = 120;
        ok = expect(cv.create(p), L"ChartView::create") && ok;
        ok = expect(cv.valid(), L"ChartView::valid") && ok;
        ok = expect((GetWindowLongPtrW(cv.hwnd(), GWL_STYLE) & SS_OWNERDRAW) != 0, L"ChartView is SS_OWNERDRAW") && ok;
        ChartData d;
        d.categories = {L"Q1", L"Q2", L"Q3"};
        ChartSeries s; s.label = L"Adoption"; s.values = {10, 20, 30};
        d.series.push_back(s);
        cv.set_data(std::move(d));
        cv.set_type(ChartType::bar);
        ThemePalette pal = theme_palette(ThemeMode::light);
        FontCache fonts;
        cv.set_palette(&pal); cv.set_font_cache(&fonts);
        RedrawWindow(cv.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        ok = expect(true, L"ChartView bar paint cycle no crash") && ok;
        cv.set_type(ChartType::line);
        RedrawWindow(cv.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        ok = expect(true, L"ChartView line paint cycle no crash") && ok;
        cv.set_type(ChartType::spline);
        RedrawWindow(cv.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        ok = expect(true, L"ChartView spline paint cycle no crash") && ok;
        // resource-count stability: chart destroyed with parent.
        controls_parent.destroy();   // NOTE: this line already exists later in the file —
        // REMOVE the duplicate; instead, ensure cv is destroyed before the existing
        // controls_parent.destroy() call. See Step 3 note.
    }
```

> **Placement:** this block must run BEFORE the existing `controls_parent.destroy();` near the end of the controls-parent scope. Insert it inside that scope (the one opened at line ~325 `if (controls_parent.hwnd() != nullptr) {`), after the IconView block and before the `controls_parent.destroy();` line. Do not call `controls_parent.destroy()` inside this block — let the existing one handle it. Drop the last three comment lines from the test code.

- [ ] **Step 2: Run test to verify it fails.** Build + run → crashes or `SS_OWNERDRAW` assertion fails if `OCM_DRAWITEM` doesn't route `ODT_STATIC` to `on_paint`.

- [ ] **Step 3: Write minimal implementation.**
  - In `src/controls/Controls.cpp`, confirm the `OCM_DRAWITEM` case dispatches `ODT_STATIC` to `control->on_paint(di->hDC, st)` (same path as `ODT_BUTTON`). If it only handles `ODT_BUTTON`, add `|| di->CtlType == ODT_STATIC` to that branch. Build the `PaintState` from `di->rcItem` + `di->itemState` exactly as for the button.
  - In `src/charts/Charts.cpp`, give `ChartView::on_paint` a real body that draws into a `MemoryDC` but still only clears the background (renderer comes in Tasks 3–4):

```cpp
void ChartView::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette& p = palette() ? *palette() : theme_palette(ThemeMode::light);
    MemoryDC mem(dc, state.bounds);
    HDC target = mem.valid() ? mem.dc() : dc;
    fill_rect(target, state.bounds, p.background);
    // renderer wired in Tasks 3-4
    (void)target;
}
```

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (all three ChartView paint cycles complete without crash; `SS_OWNERDRAW` assertion green).

- [ ] **Step 5: Commit.**
```bash
git add src/charts/Charts.cpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(charts): wire ChartView owner-draw paint cycle"
```

---

## Task 3: Bar chart renderer

**Files:**
- Modify: `src/charts/Charts.cpp`
- Test: `tests/nativeframeui_smoke.cpp` (paint-cycle already covers it; add a geometry regression assertion)

- [ ] **Step 1: Write the failing test.** Append before the final `return`:

```cpp
    {
        using namespace nfui;
        // horizontal bar geometry sanity: clusters along y, within bounds.
        RECT bounds{0,0,300,200};
        RECT h0 = bar_geometry(bounds, 0, 2, 0, 3, theme_metrics(), true);
        RECT h1 = bar_geometry(bounds, 1, 2, 0, 3, theme_metrics(), true);
        ok = expect(h0.bottom > h0.top && h1.bottom > h1.top, L"horizontal bars have height") && ok;
        ok = expect(h0.bottom <= h1.top || h1.bottom <= h0.top, L"horizontal series bars do not overlap in y") && ok;
    }
```

- [ ] **Step 2: Run test to verify it fails.** It may pass already (geometry is from Task 1) — the assertion guards regression. Build + run.

- [ ] **Step 3: Write minimal implementation.** In `src/charts/Charts.cpp`, implement `draw_bar_chart` and wire it into `ChartView::on_paint` when `type_ == ChartType::bar`. The renderer:

```cpp
namespace {
const Color& series_color(const ChartData& data, const ThemePalette& pal, size_t i) {
    static const Color coral   = Color{RGB(217,119,87)};
    static const Color teal    = Color{RGB(77,124,95)};
    static const Color amber   = Color{RGB(184,130,31)};
    static const Color slate   = Color{RGB(107,104,98)};
    if (i < data.series.size() && data.series[i].color.rgb != 0) return data.series[i].color;
    switch (i % 4) {
    case 0: return coral;     // palette.accent family
    case 1: return teal;      // success family
    case 2: return amber;     // warning family
    default:return slate;     // text_secondary family
    }
}
}

void draw_bar_chart(HDC dc, const ChartData& data, const RECT& bounds,
                    const ThemePalette& palette, const ThemeMetrics& metrics, bool horizontal) noexcept {
    if (dc == nullptr || data.series.empty()) return;
    // Compute axis range.
    double lo = data.y_min, hi = data.y_max;
    bool auto_lo = (lo == 0 && hi == 0) ? true : (lo == 0);
    // gather data bounds
    double dmin = 1e30, dmax = -1e30;
    for (const auto& s : data.series)
        for (double v : s.values) { dmin = (std::min)(dmin, v); dmax = (std::max)(dmax, v); }
    if (dmax <= dmin) { dmax = dmin + 1; dmin = dmin - 1; }
    if (data.y_min == 0 && data.y_max == 0) { lo = dmin; hi = dmax; }
    else { if (data.y_min == 0) lo = dmin; else lo = data.y_min; if (data.y_max == 0) hi = dmax; else hi = data.y_max; }
    AxisRange ar = nice_axis_range(lo, hi);

    // Plot area (inset for axis + labels).
    const int axis_pad = 28;
    RECT plot = bounds;
    if (!horizontal) { plot.left += axis_pad; plot.right -= 6; plot.top += 6; plot.bottom -= axis_pad; }
    else            { plot.left += axis_pad; plot.right -= 6; plot.top += 6; plot.bottom -= axis_pad; }

    const int span = (std::max)(1, (!horizontal ? (plot.bottom - plot.top) : (plot.right - plot.left)));

    // Gridlines + baseline.
    for (int i = 0; i <= 4; ++i) {
        double v = ar.lo + (ar.hi - ar.lo) * i / 4.0;
        int px = scale_value_to_pixel(v, ar.lo, ar.hi, span);
        POINT a, b;
        if (!horizontal) { a = {plot.left, plot.bottom - px}; b = {plot.right, plot.bottom - px}; }
        else            { a = {plot.left + px, plot.top};      b = {plot.left + px, plot.bottom}; }
        draw_line(dc, a, b, palette.border, 1);
    }
    // Baseline at v=0 if within range.
    if (ar.lo <= 0.0 && 0.0 <= ar.hi) {
        int z = scale_value_to_pixel(0.0, ar.lo, ar.hi, span);
        POINT a, b;
        if (!horizontal) { a = {plot.left, plot.bottom - z}; b = {plot.right, plot.bottom - z}; }
        else            { a = {plot.left + z, plot.top};      b = {plot.left + z, plot.bottom}; }
        draw_line(dc, a, b, palette.text_secondary, 1);
    }

    // Bars.
    const size_t cat_count = data.categories.empty()
        ? (data.series.empty() ? 0 : data.series[0].values.size())
        : data.categories.size();
    if (cat_count == 0) return;
    for (size_t si = 0; si < data.series.size(); ++si) {
        const auto& s = data.series[si];
        const Color col = series_color(data, palette, si);
        for (size_t ci = 0; ci < s.values.size() && ci < cat_count; ++ci) {
            double v = s.values[ci];
            int px = scale_value_to_pixel(v, ar.lo, ar.hi, span);
            RECT slot = bar_geometry(plot, (int)si, (int)data.series.size(), (int)ci, (int)cat_count, metrics, horizontal);
            if (!horizontal) {
                RECT bar = slot;
                bar.top    = slot.bottom - px;
                bar.bottom = slot.bottom;
                fill_rect(dc, bar, col);
            } else {
                RECT bar = slot;
                int base = scale_value_to_pixel(0.0, ar.lo, ar.hi, span);
                int bx = plot.left + base;
                int vx = plot.left + px;
                bar.left   = (std::min)(bx, vx);
                bar.right  = (std::max)(bx, vx);
                bar.top    = slot.top;
                bar.bottom = slot.bottom;
                fill_rect(dc, bar, col);
            }
        }
    }

    // Category labels.
    HFONT font = nullptr; // ChartView injects fonts via Control::fonts(); renderer uses a passed-in font in the real call.
    // (The renderer signature in the header takes no font; labels drawn by ChartView::on_paint after the renderer.)
}
```

> The renderer draws bars + gridlines only. Category labels + legend are drawn by `ChartView::on_paint` after the renderer call, using `Control::fonts()`. Update `ChartView::on_paint`:

```cpp
void ChartView::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette& p = palette() ? *palette() : theme_palette(ThemeMode::light);
    const ThemeMetrics m = theme_metrics();
    MemoryDC mem(dc, state.bounds);
    HDC target = mem.valid() ? mem.dc() : dc;
    fill_rect(target, state.bounds, p.background);
    RECT plot = state.bounds;
    if (type_ == ChartType::bar) {
        draw_bar_chart(target, data_, plot, p, m, horizontal_);
    } else {
        draw_line_chart(target, data_, plot, p, m, type_);
    }
    // category labels along the bottom (bar) / left (horizontal) using fonts()
    HFONT font = fonts() ? fonts()->regular(96, 8) : nullptr;
    // ... (draw categories + series legend; implementation detail left to the executor
    //      following the existing ShowcaseView label-drawing pattern)
}
```

> Complete the category-label + legend drawing in `on_paint` using `nfui::draw_text` with `p.text_secondary` / `p.text`. Keep it minimal: category labels under each band; a one-line legend row of colored squares + series labels along the top.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (geometry regression + paint cycles green). **Manual:** temporarily point the gallery demo (Task 5) at a bar chart — but since the demo isn't built yet, verify by adding a `ChartView` to the Controls gallery temporarily, OR defer manual check to Task 5.

- [ ] **Step 5: Commit.**
```bash
git add src/charts/Charts.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(charts): bar chart renderer (vertical + horizontal) + labels"
```

---

## Task 4: Line + spline chart renderer

**Files:**
- Modify: `src/charts/Charts.cpp`
- Test: `tests/nativeframeui_smoke.cpp` (paint-cycle already covers it)

- [ ] **Step 1: Write the failing test.** No new pure-logic assertion (line/spline geometry is GDI-path-driven, verified visually). The existing `ChartView` line + spline paint-cycle assertions (Task 2) gate compilation/no-crash.

- [ ] **Step 2: Run test to verify current baseline.** Build + smoke → PASS (line/spline currently fall through to the empty `draw_line_chart` stub).

- [ ] **Step 3: Write minimal implementation.** In `src/charts/Charts.cpp`, implement `draw_line_chart`. For each series: compute the points (x evenly spaced across plot width, y = `plot.bottom - scale_value_to_pixel(v, ar.lo, ar.hi, plot_height)`), then:
  - `ChartType::line` → `nfui::draw_polyline(dc, pts, count, col, 2)`.
  - `ChartType::spline` → build a `POINT` array and call `PolyBezier(dc, pts, count)` with control points generated by a Catmull-Rom-to-Bezier conversion (compute the 4-point segments). Provide a small static helper `catmull_rom_to_bezier(const std::vector<POINT>&, std::vector<POINT>&)` inside the anonymous namespace.
  - Draw small filled circle markers at each data point (`fill_polygon` of a tiny 3px square, or `Ellipse`).
  - Gridlines + zero baseline identical to the bar renderer (factor the axis-draw block into a shared `static void draw_axis(HDC, plot, ar, palette, horizontal)` helper used by both renderers).

```cpp
namespace {
void draw_axis(HDC dc, const RECT& plot, const AxisRange& ar, const ThemePalette& palette, bool horizontal) noexcept {
    const int span = (std::max)(1, horizontal ? (plot.right - plot.left) : (plot.bottom - plot.top));
    for (int i = 0; i <= 4; ++i) {
        double v = ar.lo + (ar.hi - ar.lo) * i / 4.0;
        int px = scale_value_to_pixel(v, ar.lo, ar.hi, span);
        POINT a, b;
        if (!horizontal) { a = {plot.left, plot.bottom - px}; b = {plot.right, plot.bottom - px}; }
        else            { a = {plot.left + px, plot.top};      b = {plot.left + px, plot.bottom}; }
        draw_line(dc, a, b, palette.border, 1);
    }
    if (ar.lo <= 0.0 && 0.0 <= ar.hi) {
        int z = scale_value_to_pixel(0.0, ar.lo, ar.hi, span);
        POINT a, b;
        if (!horizontal) { a = {plot.left, plot.bottom - z}; b = {plot.right, plot.bottom - z}; }
        else            { a = {plot.left + z, plot.top};      b = {plot.left + z, plot.bottom}; }
        draw_line(dc, a, b, palette.text_secondary, 1);
    }
}

void catmull_rom_to_bezier(const std::vector<POINT>& p, std::vector<POINT>& bez) noexcept {
    // Emit PolyBezier control points: for n points, produces (n-1) cubic segments = 3*(n-1)+1 points.
    if (p.size() < 2) return;
    bez.clear();
    bez.reserve(3 * (p.size() - 1) + 1);
    bez.push_back(p[0]);
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        POINT p0 = (i == 0) ? p[0] : p[i - 1];
        POINT p1 = p[i];
        POINT p2 = p[i + 1];
        POINT p3 = (i + 2 < p.size()) ? p[i + 2] : p[i + 1];
        POINT c1{ p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6 };
        POINT c2{ p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6 };
        bez.push_back(c1); bez.push_back(c2); bez.push_back(p2);
    }
}
}

void draw_line_chart(HDC dc, const ChartData& data, const RECT& bounds,
                     const ThemePalette& palette, const ThemeMetrics& metrics, ChartType type) noexcept {
    if (dc == nullptr || data.series.empty()) return;
    double lo = data.y_min, hi = data.y_max;
    double dmin = 1e30, dmax = -1e30;
    for (const auto& s : data.series) for (double v : s.values) { dmin = (std::min)(dmin, v); dmax = (std::max)(dmax, v); }
    if (dmax <= dmin) { dmax = dmin + 1; dmin = dmin - 1; }
    if (data.y_min == 0 && data.y_max == 0) { lo = dmin; hi = dmax; }
    else { lo = (data.y_min != 0) ? data.y_min : dmin; hi = (data.y_max != 0) ? data.y_max : dmax; }
    AxisRange ar = nice_axis_range(lo, hi);

    const int axis_pad = 28;
    RECT plot = bounds; plot.left += axis_pad; plot.right -= 6; plot.top += 6; plot.bottom -= axis_pad;
    const int plot_h = (std::max)(1, plot.bottom - plot.top);

    draw_axis(dc, plot, ar, palette, /*horizontal=*/false);

    for (size_t si = 0; si < data.series.size(); ++si) {
        const auto& s = data.series[si];
        if (s.values.empty()) continue;
        const Color col = series_color(data, palette, si);
        const int n = (int)s.values.size();
        std::vector<POINT> pts(n);
        for (int i = 0; i < n; ++i) {
            int x = plot.left + (n == 1 ? 0 : (plot.right - plot.left) * i / (n - 1));
            int y = plot.bottom - scale_value_to_pixel(s.values[i], ar.lo, ar.hi, plot_h);
            pts[i] = POINT{x, y};
        }
        if (type == ChartType::spline && n >= 2) {
            std::vector<POINT> bez;
            catmull_rom_to_bezier(pts, bez);
            if (bez.size() >= 4) {
                HPEN pen = CreatePen(PS_SOLID, 2, col.rgb);
                HGDIOBJ old = SelectObject(dc, pen);
                PolyBezier(dc, bez.data(), (DWORD)bez.size());
                if (old && old != HGDI_ERROR) SelectObject(dc, old);
                DeleteObject(pen);
            }
        } else {
            draw_polyline(dc, pts.data(), n, col, 2);
        }
        // markers
        for (int i = 0; i < n; ++i) {
            RECT mk{pts[i].x - 3, pts[i].y - 3, pts[i].x + 3, pts[i].y + 3};
            fill_rect(dc, mk, col);
        }
    }
    (void)metrics;
}
```

Refactor `draw_bar_chart` to call the shared `draw_axis` helper (remove its inline axis block).

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS. **Manual deferred to Task 5** (gallery demo).

- [ ] **Step 5: Commit.**
```bash
git add src/charts/Charts.cpp
git commit -m "feat(charts): line + Catmull-Rom spline renderer with markers"
```

---

## Task 5: NativeFrameUICharts gallery demo

**Files:**
- Create: `samples/NativeFrameUICharts/NativeFrameUICharts.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append before the final `return`:

```cpp
    ok = expect(target_registered(L"NativeFrameUICharts"),
                L"NativeFrameUICharts target is registered in generated build artifacts") && ok;
```

- [ ] **Step 2: Run test to verify it fails.** Build → fails (no `NativeFrameUICharts` target).

- [ ] **Step 3: Write minimal implementation.** Add to `CMakeLists.txt` after the `NativeFrameUIControls` block:

```cmake
add_executable(NativeFrameUICharts WIN32
    samples/NativeFrameUICharts/NativeFrameUICharts.cpp
)
target_link_libraries(NativeFrameUICharts PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUICharts)
nfui_add_resources(NativeFrameUICharts)
```

Create `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` — a Claude-Code-styled Win32 app mirroring the Workbench/Controls structure:
- `Application::initialize_process_dpi()` + `initialize_common_controls()`.
- 1100×720 `WS_CLIPCHILDREN` window titled `NativeFrame UI Charts`.
- Members: `ThemePalette palette` (light or dark, toggle), `FontCache fonts`, `ThemeMode mode`, four `ChartView` controls (`bar`, `hbar`, `line`, `spline`), a `Button` toggle.
- `WM_CREATE`: build the four `ChartData` models (e.g. bar: 3 series × 6 categories "Q1..Q6" with Adoption/Build Health/Reviews; hbar: single series ranking 5 items; line: 2 series straight; spline: 2 series smooth). Create each `ChartView` via `ControlCreateParams`, call `set_palette(&palette)` + `set_font_cache(&fonts)`, then `set_data`, `set_type`, `set_horizontal`. Inject palette/fonts into the toggle `Button`.
- `WM_PAINT`: fill the client area with `palette.background` (via `MemoryDC`); draw a serif brand title "NativeFrame UI Charts" + a mono caption.
- `WM_COMMAND` (toggle button id): flip `mode`, rebuild `palette = theme_palette(mode)`, re-inject into every `ChartView` + `Button`, invalidate.
- `WM_DPICHANGED`: reposition via `SetWindowPos` with the suggested rect; `ChartView`s auto-scale via their injected `FontCache` (rebuilt lazily).
- `WM_DESTROY` → `PostQuitMessage`.
- Message loop via `Application({instance, SW_SHOWNORMAL}).run()`-style (mirror an existing sample).

Layout (manual pixel positions, DPI-scaled via `DpiScale`):
- Sidebar 220px: serif title + 4 nav labels (Bar/Horizontal/Line/Spline) drawn as rounded pills (like Showcase), mono caption.
- Top-right: toggle button "Switch to dark".
- 2×2 grid of `ChartView` cards in the remaining area, each on a `palette.surface` rounded card.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (target registered). **Manual launch:** `./out/build/x64-debug/Debug/NativeFrameUICharts.exe` → verify 4 charts (vertical bar, horizontal bar, line, smooth spline), multi-series, light/dark toggle, flicker-free. Capture `docs/screenshots/charts-light.png` + `charts-dark.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUICharts/NativeFrameUICharts.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(samples): add NativeFrameUICharts gallery demo"
```

---

## Task 6: Validation, screenshots, docs

**Files:**
- Modify: `docs/KNOWN_LIMITATIONS.md`
- Modify: `README.md`
- Create: `docs/screenshots/charts-light.png`, `charts-dark.png`

- [ ] **Step 1: Run the full validation matrix (both presets).**
```powershell
cmake --preset x64-debug; cmake --build --preset x64-debug; ctest --preset x64-debug
cmake --preset x64-release; cmake --build --preset x64-release; ctest --preset x64-release
```
Expected: all CTest PASS, boundary check PASS (no forbidden markers in the new charts files).

- [ ] **Step 2: Manual visual validation.** Launch `NativeFrameUICharts.exe` at 100% and 150% DPI, light + dark; verify bar (vertical + horizontal), line, spline render correctly with gridlines, zero baseline, markers, legend, category labels. Capture the two screenshots.

- [ ] **Step 3: Update docs.**
  - `README.md`: add `NativeFrameUICharts.exe` to the demo table; add a Charts section noting V1 types (bar/line/spline) and the `nfui::ChartView` / `ChartRenderer` API.
  - `docs/KNOWN_LIMITATIONS.md`: add "V1 charts cover bar (vertical + horizontal) and line/spline; area, pie/donut, scatter, stacked, and histograms are deferred to V2."

- [ ] **Step 4: Commit.**
```bash
git add docs/KNOWN_LIMITATIONS.md README.md docs/screenshots/charts-light.png docs/screenshots/charts-dark.png
git commit -m "docs: add charts module + gallery demo and V1 chart scope"
```

---

## Loop Verification (run after every task)

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```
- If any test fails: stop, fix the current task, re-run. Do not advance.
- If boundary check fails: a forbidden marker slipped in — remove it before continuing.
- If all green: proceed to the next unchecked task.

## Charts exit criteria (loop until all true)

- [ ] Smoke test passes in Debug **and** Release with all new chart assertions (`scale_value_to_pixel`, `nice_num`/`nice_axis_range`, `bar_geometry`, `ChartView` paint cycles, target-registered).
- [ ] Boundary check green (no new forbidden markers; `charts` module respects the dependency direction).
- [ ] `NativeFrameUICharts.exe` renders vertical bar, horizontal bar, line, and spline charts, multi-series, flicker-free, at 100/150% DPI in light + dark.
- [ ] Pure chart math assertions pass.
- [ ] Screenshots captured; README + KNOWN_LIMITATIONS updated with the charts module + V1 scope.

When all boxes are checked, both the Restyle and Charts plans are complete.
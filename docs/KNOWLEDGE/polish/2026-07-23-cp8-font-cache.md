---
title: CP8 FontCache audit + polish
date: 2026-07-23
tags: [font, fontcache, dpi, wm-setfont, tokens]
severity: minor
effort: small
status: resolved
---

# Audit findings

Audited `include/nfui/Font.hpp`, `src/font/Font.cpp`, every caller of
`FontCache` (Button, StaticText, ComboBox, ListBox, ListView,
Controls, all four chart views, ChartsPaint, samples) and the
`WM_SETFONT` / `WM_DPICHANGED` paths in `src/controls/Controls.cpp`
and the workbench sample.

## 1. Cache strategy — correct, no change required

Each slot is keyed on `(dpi, point_size)`. `rebuild()` early-outs when
the request matches the cached key; otherwise it deletes the old
HFONT, calls `make()`, and updates the bookkeeping. This is the right
strategy for a UI-thread cache because:

- `DeleteObject` on a still-in-use HFONT can race with the OS drawing
  loop, so we only delete when we have a replacement in hand. (We
  update bookkeeping before the second `make()` even when it returns
  null, so a failing cascade still terminates without re-attempting.)
- Callers query `cache->regular(dpi_of(hwnd()), pt)` at paint time.
  After a `WM_DPICHANGED` the next paint automatically rebuilds the
  slot, so the cache is DPI-self-correcting without an explicit
  invalidate step.

No leak risk: `~FontCache` calls `DeleteObject` on each non-null
slot. The null-on-failure case is the same bookkeeping as the
success case, so callers see consistent behaviour.

## 2. Point-size coverage — magic `9` was scattered

Before CP8, the dominant UI size (`9` pt) was a literal in eight
places: `Button.cpp`, `StaticText.cpp`, `ComboBox.cpp` (×3),
`ListBox.cpp` (×2), `ListView.cpp`, `ChartView.cpp`,
`BarChartView.cpp`, `HBarChartView.cpp`, `LineChartView.cpp`,
`SplineChartView.cpp`, plus `kLegendFontPt` in
`internal/ChartsPaint.cpp`. The chart views further wrapped `9` in a
local `kTickFontPt = 9` constant that existed only to rename the
magic number.

Centralised into `font_pt::ui`, `font_pt::chart_tick`,
`font_pt::chart_legend` in `include/nfui/Font.hpp`. Every caller now
uses the named token, so a future bump to the design-system size
shifts every text surface in one edit.

## 3. Five-font coverage — added missing `bold()`

The header declared only four face accessors (`regular`, `semibold`,
`serif`, `mono`). The audit called for five including a full-weight
`bold`. Added `bold(int dpi, int point_size)` returning FW_BOLD (700)
Segoe UI — distinct from `semibold()` (FW_SEMIBOLD, 600). The slot
follows the same `(dpi, pt)` keying as the others; `~FontCache`
deletes it alongside the rest.

Use `bold` only where the visual hierarchy needs full-weight
emphasis (titles, KPI numerals, large headings). Body text and
buttons stay on `semibold` to avoid an uneven chrome.

## 4. Fallback strategy — `mono()` now has a three-stage chain

Only `mono()` had any fallback (Cascadia Code → Consolas). Added a
third stage — Lucida Console — as a last-resort pre-Vista family.
The chain only re-cascades when the previous family returns null,
and `rebuild()` updates the bookkeeping regardless, so the failing
cascade terminates without repeating itself on the next call. The
`make()` path also short-circuits to null when the requested
pixel height is non-positive, which keeps both `regular()` and
`semibold()` (no fallback) from returning invalid handles on a
defective `point_size` argument.

## 5. `WM_SETFONT` on DPI change — already correct

The subclass proc in `src/controls/Controls.cpp` chains
`DefSubclassProc` then `InvalidateRect` on `WM_SETFONT`, so the OS
re-broadcast during a DPI change triggers an immediate repaint.
Self-painting controls (Button, StaticText) read the cache via
`fonts()->regular(dpi_of(hwnd()), pt)` at paint time, so the
DPI-keyed slot is rebuilt before the invalidation completes. Sample
hosts (Workbench, DarkStudio, SettingsDemo, etc.) additionally
forward `WM_DPICHANGED` to an `apply_native_fonts()` helper that
re-sends `WM_SETFONT` to native-chrome children — keeping native
controls (Tab, ListView, TreeView) in lockstep.

No code change needed here; the existing path is correct.

# Files changed

- `include/nfui/Font.hpp` — added `font_pt` token namespace,
  `bold()` accessor, header comments documenting the (dpi, pt)
  keying.
- `src/font/Font.cpp` — `bold()` body, three-stage `mono()`
  fallback (Cascadia → Consolas → Lucida Console), destructor
  updated for the new slot.
- `src/controls/Button.cpp`, `src/controls/StaticText.cpp`,
  `src/controls/ComboBox.cpp`, `src/controls/ListBox.cpp`,
  `src/controls/ListView.cpp`, `src/controls/Controls.cpp` —
  replaced literal `9` with `font_pt::ui`.
- `src/charts/ChartView.cpp`,
  `src/charts/BarChartView.cpp`,
  `src/charts/HBarChartView.cpp`,
  `src/charts/LineChartView.cpp`,
  `src/charts/SplineChartView.cpp`,
  `src/charts/internal/ChartsPaint.cpp` — replaced `kTickFontPt`
  and `kLegendFontPt` with `font_pt::chart_tick` /
  `font_pt::chart_legend`.

# Verification

- `cmake --preset x64-debug` (configure)
- `cmake --build --preset x64-debug` (clean build, no warnings)
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` — 2/2 pass
  (smoke + boundary check)
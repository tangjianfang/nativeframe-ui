# Learning: FontCache Consumer DPI Audit

**Date:** 2026-07-21
**Triggered by:** Phase A2 polish audit
**Related commits:** `15f25e6` (R8), `380a330` (R9), `84038c1` (R9 follow-up)

## Background

`nfui::FontCache` builds `HFONT` handles keyed by `(weight, dpi)`. A DPI
change (`WM_DPICHANGED`) requires re-fetching the new DPI's font and re-
applying it via `WM_SETFONT` to **every** native child window that captured
the old DPI's font handle. Owner-draw controls (`Button`, `ChartView`
subclasses) re-query `dpi_of(hwnd())` inside their `on_paint` so they only
need their `FontCache*` re-bound, not an explicit `WM_SETFONT`.

## Symptom

If a demo's `WM_DPICHANGED` handler only invalidates children without
re-binding / re-sending fonts, native controls render in the **old DPI's
font** at the new DPI's layout scale → visual mismatch.

This was the original R8 spec gap (commit `15f25e6`) and the R9 ListBox
gap (commits `380a330` / `84038c1`).

## Audit Result (2026-07-21)

All 6 demos correctly handle DPI changes:

| Demo | Handler |
|---|---|
| `NativeFrameUICharts` | re-binds `fonts_` to all 4 chart views + `layout_charts()` + `InvalidateRect` |
| `NativeFrameUIControls` | `dpi_of(hwnd())`, `WM_SETFONT` to `view_`, `WM_SETFONT` + `LB_SETITEMHEIGHT` to `list_`, invalidate all controls + self |
| `NativeFrameUIDarkStudio` | `dpi_ = new DpiScale`, `refresh_layout()` + invalidate |
| `NativeFrameUIResourceGallery` | `apply_native_fonts()` + `layout_controls()` + invalidate |
| `NativeFrameUISettingsDemo` | `apply_native_fonts()` + `layout_controls()` + invalidate |
| `NativeFrameUIWorkbench` | `apply_native_fonts()` + `layout()` + invalidate |

The `apply_native_fonts()` helper used by Workbench / ResourceGallery /
SettingsDemo is the canonical "re-apply `WM_SETFONT` to every native
child" pattern. See `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp:239-253`.

## Applies to

Any future demo / consumer that:
1. Holds a `nfui::FontCache` member.
2. Owns native children (`Button`, `CheckBox`, `Edit`, `ListBox`,
   `ComboBox`, `StatusBar`, etc.) OR owner-draw ChartView subclasses.
3. Wants correct DPI scaling.

The DPI handler must:
- Re-bind the `FontCache*` pointer on every owner-draw child.
- Send `WM_SETFONT` to every native child that captured a DPI-keyed
  `HFONT` (use `apply_native_fonts()` or equivalent).
- Re-measure any DPI-dependent row heights (`LB_SETITEMHEIGHT`, etc.).
- `InvalidateRect` the parent + children with `FALSE` (no erase) so the
  next paint uses the new fonts.

## Prevention check

- Code review: any new `WM_DPICHANGED` handler must list each child
  control and the action taken (re-bind, `WM_SETFONT`, re-measure,
  invalidate).
- Mechanical: `grep -A30 "case WM_DPICHANGED" path/to/file.cpp` and verify
  every `fonts_.regular(...)` consumer has a corresponding `WM_SETFONT`
  path in the DPI handler.
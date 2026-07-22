# NativeFrameUIComponentGallery

The "kitchen sink" sample. Renders every control class in its expected
state, stacked vertically with section labels down the left margin. This
is the closest thing the framework has to a single-shot smoke test for
consumers — if any control fails to instantiate or paint, its row is
obviously empty or out of place.

## What it demonstrates

- **Header** — `NativeFrame UI ComponentGallery` (serif 16).
- **Vertical sections** (top to bottom), each labelled down the left
  margin:
  - `Button` — `Default / Disabled / Hover` (Disabled is greyed via
    `EnableWindow(... FALSE)`).
  - `CheckBox` — `Unchecked / Checked / Indeterminate`
    (`BM_SETCHECK` to drive state).
  - `RadioButton` — `Option A / B / C` with `A` pre-selected.
  - `Edit` — `editable sample`.
  - `StaticText` — three alignments using the CP8 `TextStyle::align_h`
    field so the labels match the rendered layout.
  - `ListBox` — five items, second selected (`LB_SETCURSEL = 1`).
  - `ComboBox` — five colours in the dropdown.
  - `ListView` — three columns (`A / B / C`) × four rows (`A1..C4`).
  - `TreeView` — `Root` expanded with three children.
  - `IconView` — bound to the scaled `IDI_NFUI_APP` icon at 32 px.
  - `StatusBar` — `Ready`.
  - `TabControl` — three tabs (`Tab 1 / Tab 2 / Tab 3`) with the CP8A
    `(12, 4)` padding.
  - `ProgressBar` — `PBM_SETRANGE(0, 100)` and `PBM_SETPOS(60, 0)`.
  - `Panel + Splitter` — V1.4 chrome with a 6-px splitter drag handle.
- **Footer** — single-line caption reminding the reviewer what they
  are looking at.

Window is sized `880 × 1320` and scrollable (`WS_VSCROLL`).

## Key controls

Every control class the framework exposes. The relevant wrappers, all
created through the documented `inject_theme` → `create` path:

- `nfui::Button`, `nfui::CheckBox`, `nfui::RadioButton`, `nfui::Edit`,
  `nfui::StaticText`, `nfui::ListBox`, `nfui::ComboBox`,
  `nfui::ListView`, `nfui::TreeView`, `nfui::IconView`,
  `nfui::StatusBar`, `nfui::TabControl`, `nfui::ProgressBar`,
  `nfui::Panel`, `nfui::Splitter`.

Plus:

- `nfui::Window` — host window.
- `nfui::ThemePalette` + `nfui::FontCache` — single shared palette /
  font cache injected into every wrapper before creation.
- `nfui::DpiScale` — `nfui::dpi_of(hwnd())` re-read on every size /
  DPI change.
- `nfui::MemoryDC` — flicker-free offscreen buffer for the section
  labels and header.

## Theme support

Single light palette. The demo is a control-state surface, not a
theme-comparison surface — switching themes would re-arrange every
section and bury the product story. For Light / Dark / HC comparison
in the same family, run `NativeFrameUIThemeDemo`.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIComponentGallery
./out/build/x64-debug/Debug/NativeFrameUIComponentGallery.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Single theme. Theme switching is not wired here; use
  `NativeFrameUIThemeDemo` for the Light / Dark / HC comparison.
- The window is tall (`1320` pixels at 100 % DPI) and depends on the
  vertical scroll bar for the bottom sections at smaller monitor
  sizes.
- All controls render in their default state except where the sample
  forces a non-default value via the documented message
  (`BM_SETCHECK`, `LB_SETCURSEL`, `CB_SETCURSEL`, `PBM_SETPOS`, ...).
  Interactivity (clicking a checkbox, etc.) is not exercised; this is
  a static visual smoke test.
- All controls share one `nfui::FontCache` and adopt Segoe UI; the
  Gallery does not exercise the Serif / Mono / Semibold faces beyond
  the section labels.

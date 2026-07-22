# NativeFrameUIThemeDemo

The theme-comparison harness. Renders every control class in its
expected state, then lets you flip the entire row between **Light /
Dark / High Contrast** in place.

## What it demonstrates

- **Persistent toggle bar** at the top — three `nfui::Button`s
  (`Light / Dark / High Contrast`). These stay alive across theme
  switches.
- **Demo row** below — 13 sections, each labelled down the left margin:
  `Button`, `CheckBox`, `RadioButton`, `Edit`, `StaticText`, `ListBox`,
  `ComboBox`, `ListView`, `TreeView`, `IconView`, `ProgressBar`,
  `TabControl`, `Panel + Splitter`. Every section renders the same
  control types you would see in `NativeFrameUIComponentGallery`, but
  rebuilt fresh against the active palette.
- **Active mode footer** in the title row — `Active mode: Light /
  Dark / High Contrast` so the screenshot reviewer never has to guess.
- **Status bar** at the bottom showing `ThemeDemo: ready`.

Switching themes tears down the demo row and rebuilds it against the new
`nfui::theme_palette(mode_)` so every native control adopts the new
colours. Self-painted controls (buttons, progress, etc.) re-paint
through their cached palette pointer.

## Key controls

- `nfui::Button` — three toggle buttons (persistent) plus two demo
  buttons (`OK` / `Cancel`).
- `nfui::CheckBox` — three states (Unchecked / Checked /
  Indeterminate).
- `nfui::RadioButton` — three options with the first pre-selected.
- `nfui::Edit` — single editable field pre-filled with `editable
  sample`.
- `nfui::StaticText` — three alignments (`Left / Center / Right`)
  using the new CP8 `TextStyle::align_h` field.
- `nfui::ListBox` — five items with `LB_SETCURSEL = 1`.
- `nfui::ComboBox` — five colours with `CB_SETCURSEL = 0`.
- `nfui::ListView` — three columns, four rows.
- `nfui::TreeView` — two levels, root expanded.
- `nfui::IconView` — bound to the scaled `IDI_NFUI_APP` icon at 32 px.
- `nfui::ProgressBar` — pre-positioned to 60 %.
- `nfui::StatusBar` — bottom strip with `ThemeDemo: ready`.
- `nfui::TabControl` — three tabs (`General / Layout / Output`) with
  the CP8A `(12, 4)` padding.
- `nfui::Panel` + `nfui::Splitter` — V1.4 chrome theming demonstration.

## Theme support

Three-way toggle. Click any of `Light / Dark / High Contrast` and:

1. `mode_` updates.
2. `palette_ = nfui::theme_palette(mode_)` rebuilds the shared palette.
3. The persistent toggle buttons refresh their palette via
   `set_palette(&palette_)`.
4. `build_demo()` tears down the demo row and re-creates every control
   against the new palette.
5. `layout_demo()` resizes everything to the current client rect.
6. `apply_native_fonts()` re-applies the Segoe UI font to every
   control whose chrome depends on it.
7. `InvalidateRect` schedules the next paint cycle.

The bottom-right footer reflects the active mode so the screenshot is
self-describing.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIThemeDemo
./out/build/x64-debug/Debug/NativeFrameUIThemeDemo.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Theme rebuild is destructive — the demo row is destroyed and
  recreated every switch. There is no per-control incremental
  re-paint; in practice the rebuild is fast enough that the
  flicker is invisible, but consumers wiring `set_palette` live
  should expect a brief teardown.
- The `High Contrast` button maps to `nfui::ThemeMode::high_contrast`
  which the framework resolves to a high-contrast palette; if the
  framework is built without HC support the row falls back to a
  light palette.
- All thirteen sections share the same row height, so very short
  controls (IconView at 32 px) leave white space below them.
- The demo row only re-renders once per click; there is no animated
  cross-fade.

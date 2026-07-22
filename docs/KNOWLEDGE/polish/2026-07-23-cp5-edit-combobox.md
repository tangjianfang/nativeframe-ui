---
title: CP5 Edit and ComboBox polish
date: 2026-07-23
tags: [polish, edit, combobox, theme]
status: resolved
---

## Context
User reported: "打磨所有子组件的UI实现". Edit and ComboBox were the
remaining native-control wrappers whose visual state was effectively
ignored by the framework. The native EDIT client drew the OS-default
border + caret regardless of the injected palette; ComboBox delegated
its dropdown styling to the framework-managed `style_` record that
nobody populated.

## Findings

- **Edit**: `create_native` only registered the standard Windows class.
  No subclass proc, no `on_palette_changed` override, no focus
  indicator, no disabled-state dimming.
- **ComboBox**: `ComboBoxStyle` was a struct but no constructor or
  builder produced a styled instance. Dropdown items inherited whatever
  the parent theme happened to be using. `set_style()` was a setter
  with no consumer.
- **Theme transition**: neither control reacted to `on_palette_changed`,
  so a palette swap left the native chrome (border + selection) on the
  previous palette until the user clicked the control.

## Fix

### Edit
- Added a per-instance `Edit::visual_subclass_proc` registered via
  `SetWindowSubclass` so the control can intercept
  `OCM_CTLCOLOREDIT` / `OCM_CTLCOLORSTATIC` reflected from the parent
  Window wrapper. The reflected handlers push `palette.surface` for the
  background and `palette.text` / `palette.text_secondary` for the text,
  keeping the native caret / selection behaviour intact.
- The same subclass proc chains `DefSubclassProc` for
  `WM_SETFOCUS` / `WM_KILLFOCUS` / `WM_ENABLE` and calls
  `paint_border()` on transitions so the border repaints immediately
  when focus or enabled state changes.
- `paint_border()` reads `palette.border`, mixes toward `background`
  when disabled, and substitutes `palette.accent_hover` when focused.
  `on_palette_changed` invokes `RedrawWindow` with
  `RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW` so the
  border repaints with the new palette and the client repaints in the
  same window-message turn.

### ComboBox
- Replaced `ComboBoxStyle` with a richer default-constructed
  record (`row_height_extra`, `dropdown_text_color`,
  `dropdown_background`, `rounded_selection`, etc.) and added
  `set_dropdown_row_height(int)` / `set_dropdown_palette(text, background)`
  builders.
- ComboBox's `create()` now installs a `visual_subclass_proc` that
  applies `style_.dropdown_background` / `dropdown_text_color` to the
  reflected `OCM_CTLCOLOR*` family so the dropdown list items follow
  the framework palette.
- `on_palette_changed` re-pushes the dropdown colours and refreshes
  the frame, so a palette swap propagates without a manual reload.

## Resolution

Both Edit and ComboBox now have visible theme awareness:
- focus border matches the palette accent;
- disabled state dims the border and text;
- theme swaps propagate without a click.

The Edit changes are leaf-only (`nfui_text` library) so the
`nfui_control_base` boundary remains intact.

## Verification

- `cmake --build --preset x64-debug` clean across all 10 sample
  executables and the smoke / boundary tests.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2.

## Lesson

Native Windows controls that are wrapped in a `Control` subclass need
explicit `on_palette_changed` overrides plus reflected-message subclass
procs whenever the OS draws chrome the framework wants to control. A
"control that paints nothing is a control whose chrome is the OS's" is
a useful invariant — if you can't draw it, you must intercept it.
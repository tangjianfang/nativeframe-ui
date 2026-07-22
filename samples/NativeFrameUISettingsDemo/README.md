# NativeFrameUISettingsDemo

Realistic settings surface — a category list on the left, a form of
native edit / combo / check controls on the right, and a status bar at
the bottom. Demonstrates how saved-state feedback stays obvious when
every field is a native Win32 control.

## What it demonstrates

- **Left column** — `nfui::ListBox` with three categories (`General`,
  `Workspace`, `Release`). The current selection swaps the description
  band on the right via the documented `LBN_SELCHANGE` path.
- **Right form** — three labelled rows: `Profile name` (Edit, pre-filled
  `NativeFrame UI`), `Workspace root` (Edit, pre-filled
  `C:\nativeframeui\workspace`), `Theme preference` (ComboBox with
  `Follow system / Light / Dark`), plus two checkboxes (`Enable
  optional startup telemetry`, `Restore the previous workspace on
  launch`) both pre-checked.
- **Top bar** — `Saved state` label and `Save snapshot` button. The
  label updates between `Saved state: pending changes` and
  `Saved state: synchronized` based on whether any field has been
  edited, and the same text is mirrored to the status bar.
- **Description band** — a word-wrapped `nfui::StaticText` (CP8 added
  word-wrap support so the longer `Workspace` blurb no longer clips).
- **Status bar** — mirrors the saved-state label so the user sees the
  change without scanning for it.

## Key controls

| Control | Use | Notes |
|---------|-----|-------|
| `nfui::ListBox` | categories | `LBN_SELCHANGE` routes through `on_command`. |
| `nfui::Edit` | profile + workspace fields | Adopts Segoe UI via `WM_SETFONT`. |
| `nfui::ComboBox` | theme preference | `CBN_SELCHANGE` marks the form dirty. |
| `nfui::CheckBox` | telemetry + startup | `BN_CLICKED` marks the form dirty. |
| `nfui::Button` | save snapshot | Click clears the dirty state. |
| `nfui::StaticText` | saved-state label + description | CP8 `TextStyle` enables word wrap on description. |
| `nfui::StatusBar` | status mirror | Same text as the saved-state label. |
| `nfui::MemoryDC` | offscreen banner paint | R6 fix. |

## Theme support

Single palette, fixed light cream background with a coral hairline at
the banner's bottom edge. Theme switching is **not** wired here — the
demo's product story is "settings form + native controls", so
introducing a toggle would compete with the form interaction.

`NativeFrameUIThemeDemo` covers Light / Dark / HC comparison in the same
shell family.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUISettingsDemo
./out/build/x64-debug/Debug/NativeFrameUISettingsDemo.exe
```

Same recipe works for `x64-release`.

## Known limitations

- The theme combo does not actually switch the palette. Selection
  changes mark the form dirty but the visual theme stays light; the
  combo is wired so the form behaviour is correct, but the visual
  switch lives in `NativeFrameUIThemeDemo`.
- No persistence — saved-state is in-memory only; closing the window
  discards the changes.
- The `Save snapshot` button does not write anywhere; clicking it only
  flips the saved-state label.
- Only three categories are visible; the `LB_ADDSTRING` API supports
  more, but adding them would not change the demo's story.

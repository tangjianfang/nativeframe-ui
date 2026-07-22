# NativeFrameUIControls

Compact theme-toggle demonstration. The smallest of the regular samples
— a 480 × 360 window with a theme toggle button plus a focused set of
controls that prove both light/dark switching and the V1.4 chrome
theming hooks (`FrameStyle::surface_brush`, `accent`, `chrome_text`,
`chrome_bg`).

## What it demonstrates

- **Theme toggle button** — `Switch to dark` / `Switch to light`,
  re-uses the same `nfui::Button` with `set_palette` and
  `SetWindowTextW` between switches.
- **Standard button row** — `OK` / `Cancel`. Both are demo buttons;
  `OK` shows a tooltip on hover and `Cancel` mirrors `OK`.
- **IconView** — bound to the scaled `IDI_NFUI_APP` icon.
- **ListBox** — five fruit entries, with `LB_SETITEMHEIGHT` adjusted
  per DPI so the row height tracks the font.
- **ListView** — three rows (`Row 0 / Row 1 / Row 2`) with one column
  (`Item`).
- **Panel + Splitter** — a `Panel` underneath the OK button and a
  vertical `Splitter` drag handle. Both adopt `FrameStyle` overrides:
  the panel uses `palette_.surface_hover` for its surface, the splitter
  uses `palette_.accent`.
- **Tooltip** — attached to the OK button via `TTM_ADDTOOL`. Uses
  `FrameStyle::chrome_text = palette_.text` and `chrome_bg =
  palette_.surface` so the tooltip's text and background track the
  active palette.
- **Header** — `NativeFrame UI Controls` rendered through `nfui::draw_text`.

## Key controls

| Control | Use | Notes |
|---------|-----|-------|
| `nfui::Button` | theme toggle + OK + Cancel | Self-painted. |
| `nfui::ListBox` | fruit list | `LB_SETITEMHEIGHT` recalculated on `WM_DPICHANGED`. |
| `nfui::ListView` | rows table | Single column with three rows. |
| `nfui::IconView` | app icon | Bound to scaled `IDI_NFUI_APP`. |
| `nfui::Panel` | V1.4 chrome demo | `FrameStyle::surface_brush` override. |
| `nfui::Splitter` | vertical handle | `FrameStyle::surface_brush = accent`. |
| `nfui::Tooltip` | chrome text + bg | `FrameStyle::chrome_text/chrome_bg` consumed by `Tooltip::on_palette_changed`. |

## Theme support

In-window toggle. Clicking `Switch to dark` / `Switch to light`:

1. Toggles `mode_` between `light` and `dark`.
2. Replaces `palette_` via `nfui::theme_palette(mode_)`.
3. Calls `set_palette(&palette_)` on every wrapper.
4. Rebuilds the three `FrameStyle` overrides (panel / splitter /
   tooltip) so the chrome tracks the new palette without subclassing.
5. Updates the toggle button label and forces `InvalidateRect` on
   every wrapper plus the host window.

`Tooltip::on_palette_changed` (core, CP6) consumes the stored
`chrome_text` / `chrome_bg` and re-applies them to the live ComCtl32
tooltip window, so the sample does not need to call
`TTM_SETTIPTEXTCOLOR` / `TTM_SETTIPBKCOLOR` itself.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIControls
./out/build/x64-debug/Debug/NativeFrameUIControls.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Only Light / Dark — High Contrast is not surfaced. For HC use
  `NativeFrameUIThemeDemo`.
- The Panel + Splitter pair is decorative — there is no underlying
  child layout that resizes against the splitter drag.
- The tooltip is attached to the OK button only; Cancel has no
  tooltip.
- The window is small (480 × 360) so dense control rows may need the
  window resized manually for full visibility.

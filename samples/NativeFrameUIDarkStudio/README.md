# NativeFrameUIDarkStudio

Dark-mode-only product shell focused on a preview-heavy surface (think
designer / creative tools). Pairs with `NativeFrameUIShowcase` to show
that the framework does not force a single visual identity — both
samples use the same core APIs but land in very different looks.

## What it demonstrates

A single dark shell rendered through self-paint, with a native status
bar anchoring the bottom:

- **Sidebar** — `Surface / Canvas / Tokens / Publish` navigation that
  updates the status bar text (`Surface review active.` etc.) without
  rebuilding any pane.
- **Preview canvas** in the centre — a wide rounded rectangle with a
  live-preview strip and a caption explaining the sample's product
  scope.
- **Two metric cards** below the canvas (`Native shell 100%` and
  `DPI layout Per-monitor`) with mono-font values.
- **Native status bar** at the bottom — kept as a real `nfui::StatusBar`
  so the demo proves the dark chrome pairs with Win32 status surfaces
  rather than always owning them itself.

The window class only handles size / DPI / paint. All visual chrome is
painted in `paint_shell` against an offscreen `nfui::MemoryDC` covering
the content area above the status bar.

## Key controls

- `nfui::Window` — host window with `WM_SIZE` / `WM_LBUTTONDOWN` /
  `WM_DPICHANGED` handlers.
- `nfui::StatusBar` — native status bar at the bottom; receives
  `WM_SETFONT` against `fonts_.regular(dpi, 9)` so its text matches the
  self-painted chrome.
- `nfui::ThemePalette` — initialised with `nfui::theme_palette(dark)`
  and stays on dark for the lifetime of the process.
- `nfui::FontCache` — Segoe UI / Semibold / Serif / Mono fonts.
- `nfui::DpiScale` — every layout measurement is logical, scaled by
  `logical_to_pixels`.
- `nfui::MemoryDC` — flicker-free offscreen buffer for the content
  area. Buffer flush happens before `EndPaint` (R6 fix).
- `nfui::ResourceContext` — loads `IDI_NFUI_APP` for the window icon.

## Theme support

**Dark only by design.** This is a deliberate product surface for
dark-mode tools; there is no toggle inside the demo. For Light / Dark
comparison in the same shell, run `NativeFrameUIThemeDemo`.

The dark palette is consumed once at construction and reused for every
paint. DPI changes re-apply the Segoe UI font cache but do not switch
the palette.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIDarkStudio
./out/build/x64-debug/Debug/NativeFrameUIDarkStudio.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Single theme (dark). Light + High Contrast are not surfaced here.
- Navigation is purely a status-bar message switcher — no other panel
  changes state when the user clicks a nav item.
- The preview canvas is a static surface; there is no live drag, zoom,
  or animated preview.
- The sample uses `WM_LBUTTONDOWN` for nav selection but does not track
  hover state; that lives in `NativeFrameUIShowcase`.

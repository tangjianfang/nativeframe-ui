# NativeFrameUIShowcase

The flagship product-growth evaluation surface. Renders a single custom-
painted dashboard so reviewers can see what NativeFrame UI shells look
like without going near the legacy proprietary Win32 UI toolkit look.

## What it demonstrates

A directly-painted product shell (no nested child controls) showing:

- **Brand header** — serif "NativeFrame UI" + a one-sentence tagline in
  the sidebar.
- **Navigation rail** — `Overview / Pipelines / Reviews / Resources`
  with hover and selected states. Selection updates a tracker but no
  other panel; the demo focuses on the visual rhythm.
- **Command bar** — three KPI cards ("Adoption", "Build Health",
  "Resource Flow") with hover highlight and status badges
  (`Stable / Release Ready / Explicit`).
- **Inspector panel** on the right with three labelled rows
  (`Framework boundary`, `DPI path`, `Resource story`).
- **Theme toggle** pill button in the command bar that swaps Light ↔ Dark
  in place — no rebuild, the existing widget re-paints with the new
  palette.

All painting goes through the `ShowcaseView` helper
(`ShowcaseView.hpp` / `ShowcaseView.cpp`), which owns the layout,
hit-testing, and palette mapping. The window class itself only handles
mouse tracking and forwards paint to the view.

## Key controls

This sample intentionally avoids nested child controls so reviewers can
focus on visual rhythm. It still uses the framework surface:

- `nfui::Window` — host window with custom `WM_PAINT` / `WM_SIZE` /
  `WM_DPICHANGED` handling.
- `nfui::FontCache` — shared Segoe UI / Semibold / Serif / Mono fonts
  pulled by face at the active DPI.
- `nfui::DpiScale` — every layout measurement is in logical units;
  `logical_to_pixels` adapts to the active monitor.
- `nfui::MemoryDC` — flicker-free offscreen buffer over the full client
  area (R6 fix carried from SettingsDemo).
- `nfui::theme_palette` — palette resolver for Light / Dark.
- `nfui::ResourceContext` — loads `IDI_NFUI_APP` for the title bar.

## Theme support

In-window toggle. The command-bar pill says **Switch to dark** /
**Switch to light** depending on current state. Clicking it:

1. Updates `ShowcaseView::theme_mode_` to the opposite mode.
2. Triggers `InvalidateRect`; the next `WM_PAINT` rebuilds the palette
   via `palette_for(theme_mode_)` and repaints the same surface.
3. The shell keeps its current navigation selection and hovered card
   across the switch.

`high_contrast` and `system` modes collapse to light (the view has no
high-contrast surface; use `NativeFrameUIThemeDemo` for that).

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIShowcase
./out/build/x64-debug/Debug/NativeFrameUIShowcase.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Navigation is purely visual — clicking a nav item only updates the
  selection state; no other panel content changes. The product story is
  "what does the shell look like at 100%", not "drive every nav into a
  functional page".
- `high_contrast` and `system` modes are not surfaced in the toggle; use
  `NativeFrameUIThemeDemo` to evaluate them.
- The view paints all chrome itself; there is no `Panel` / `Splitter`
  surface, so visual chrome theming via `FrameStyle` is not exercised
  here.
- No persistence of the selected nav across launches.

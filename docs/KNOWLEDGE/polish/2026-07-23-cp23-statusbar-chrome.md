# CP23 — StatusBar chrome self-paint + sample residual defects

## Goal

Eliminate the remaining sample-visible chrome regressions found by the CP22
audit:

1. **StatusBar** — native ComCtl32 v6 SBARS_SIZEGRIP and part chrome read as a
   pale grey strip on top of the themed background in dark mode. CP22 chained
   `DefSubclassProc` after the themed background paint; CP23 owns the full paint
   pass.
2. **DarkStudio rail** — nav rows only spanned `nav_width - outer` of the rail
   width, leaving a 30 logical px overhang on the right where the rail fill
   showed but no nav row covered.
3. **ResourceGallery About modal** — `IDD_NFUI_ABOUT` used `IDC_STATIC` for a
   single line of text and rendered against `COLOR_BTNFACE` with the system
   font, reading as a 1995-era grey dialog next to the themed shell.

## StatusBar rewrite

### Subclass chain ordering

`SetWindowSubclass` dispatches in reverse install order — the LAST subclass
runs FIRST. `StatusBar::create` installs the chrome subclass AFTER the base
`Control::subclass_proc` installed by `create_native`. So:

- WM_PAINT: chrome proc runs first → `return 0` (suppress native paint).
- Other messages: chrome proc falls through to `DefSubclassProc`, which
  reaches the base proc and the rest of the chain.

For WM_PAINT we use `BeginPaint`/`EndPaint` to validate the update region.
The base's `subclass_proc` checks `wants_self_paint()` and would call
`on_paint` then chain `DefSubclassProc` (native paint). The chrome subclass
short-circuits this entirely, so the themed chrome is the only paint ever
reaches the screen for the StatusBar.

### paint_chrome

`paint_chrome(HDC)` paints the full bar chrome:

- **Background** — `palette.surface` (or `style.surface_brush` if injected),
  filled into an offscreen `MemoryDC` so flickers are eliminated on
  resize/repaint.
- **Top hairline** — 1 logical px `palette.border` strip so the bar reads as
  a distinct band against the window chrome above it.
- **Part text** — read live from the native control via
  `SB_GETTEXTLENGTHW` + `SB_GETTEXTW` (so callers that set text via
  `SB_SETTEXTW` keep seeing their content), drawn in `palette.text` with the
  cached semibold face at the bar's DPI.
- **Size grip** — three diagonal rows of `palette.border` dots in the
  bottom-right corner, replacing the native SBARS_SIZEGRIP chevrons that
  paint in `COLOR_BTNTEXT` and read as a pale patch in dark mode.

All layout offsets (text padding, hairline height, grip reservation, grip
dot cell/gap) are scaled by `nfui::DpiScale::logical_to_pixels` so the bar
holds constant in logical units across DPI. The prior version used raw
device-pixel constants that collapsed at high DPI.

`paint_chrome` is a `StatusBar` member method because `palette()` and
`fonts()` are `protected` on `Control`. The chrome subclass proc (a free
function) calls `sb->paint_chrome(dc)` after retrieving the `StatusBar*`
from `ref_data`.

### `set_style` invalidation

`set_style(FrameStyle)` now invalidates the HWND so the chrome repaints
with the new surface/foreground tokens. Matches the pattern in
`Control::set_palette`.

### Scope limitation

`paint_chrome` paints part 0 only. Multi-part status bars set up via
`SB_SETPARTS` + `SB_SETTEXTW(part, ...)` will only show the first part's
text in our paint; the native control would render all parts. Documented in
the header. All framework samples (Workbench, DarkStudio) use a single-part
model so this is acceptable for now; a multi-part paint path is a separate
round of work.

## DarkStudio rail fix

`navigation_rect(index)` returned
`make_rect(content.left + outer, top, nav_width - outer, nav_height)`,
which assumed a 210 logical px rail and shrank the nav row by `outer` on
only one side. The rail itself is `rail_width = 220` logical px wide,
inset by `outer` on both sides, so the nav row should be
`rail_width - outer * 2` to match.

Now the nav rows fully cover the rail fill.

## ResourceGallery About modal theming

`IDD_NFUI_ABOUT` now declares three labelled statics:

- `IDC_NFUI_ABOUT_TITLE` — semibold, `palette.text`
- `IDC_NFUI_ABOUT_BODY` — regular, `palette.text_secondary`
- `IDC_NFUI_ABOUT_BUILD` — regular, `palette.text_secondary`

`gallery_dialog_proc` themes the dialog at `WM_INITDIALOG`:

- Sets the window class background brush to `palette.surface` so the dialog
  surface matches the host shell.
- Applies the framework's Segoe UI regular/semibold cached fonts to the
  statics and OK button.
- Stores the palette pointer in `DWLP_USER` so `WM_CTLCOLORSTATIC` can
  re-stamp the static-text colours without re-passing the palette through
  every paint.
- `WM_CTLCOLORSTATIC` returns the themed brush and re-stamps the
  per-control text colour on every paint (static controls reset their
  colour from the DC each paint).
- `WM_DESTROY` deletes the themed brush to avoid leaking it.

The host `on_command` passes `&palette_` as the dialog init lparam so the
proc can read the palette.

## Build / test

```
cmake --build --preset x64-debug
NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure --timeout 120
# 100% tests passed, 0 tests failed out of 3
```

## Files changed

- `include/nfui/Controls/StatusBar.hpp` — `set_style` out-of-line, header
  comments corrected (chain ordering, multi-part scope).
- `src/controls/StatusBar.cpp` — full rewrite: subclass proc, DPI-scaled
  paint, `set_style` invalidation, defensive `sb == nullptr` paths.
- `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp` — rail fill
  width fix.
- `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp`
  — themed About dialog via palette-aware `gallery_dialog_proc`.
- `resources/NativeFrameUI.rc` — expanded About dialog template with
  title/body/build labels.
- `resources/NativeFrameUIResource.h` — new IDC constants for the About
  dialog labels.
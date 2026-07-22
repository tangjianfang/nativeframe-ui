---
title: CP2 corner abruptness and theme transition polish
date: 2026-07-23
tags: [polish, theme, controls]
status: resolved
---

## Context
User reported: "切换主题后边角颜色出现突兀" — rounded controls visibly
flash a stale frame during a palette transition.

P6.1 + CP1 ruled out hardcoded RGB literals and fixed the StatusBar
self-paint chaining, but the abruptness still surfaced on Buttons,
ListBox rows, and the ListView empty area. CP2 closes the loop.

## Root cause

Three independent issues compounded to make the corner transition look
abrupt:

1. **Native chrome stayed on the previous palette.** ComCtl32 reads its
   colors once when the control is created. Re-injecting the framework
   palette (`Control::set_palette`) only repainted the *custom* path;
   the ListView empty area and the native button chrome kept the old
   frame.
2. **System theme notifications were swallowed.** `WM_THEMECHANGED`,
   `WM_SYSCOLORCHANGE`, and `WM_SETTINGCHANGE` never reached the
   subclass proc's switch, so Windows-driven repaints did not happen.
3. **RoundRect left the four outside corner pixels untouched.** When
   the previous frame's edge landed in those corners and the new fill
   rounded into them, a 1-2 pixel ring of the old palette stayed
   visible until the next full repaint.

## Fix

### A. System theme notifications now propagate
`Control::subclass_proc` (`src/controls/Controls.cpp`) now chains
`DefSubclassProc` for `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` /
`WM_SETTINGCHANGE`, then `InvalidateRect`s so the leaf repaints on the
same message-loop turn.

### B. Re-injecting a palette invalidates the HWND
`Control::set_palette` / `set_font_cache` (`include/nfui/Controls/Control.hpp`)
now call `on_palette_changed()` and `InvalidateRect` when an HWND is
attached. `on_palette_changed` is a new virtual extension point —
default is a no-op, so `nfui_control_base` stays free of leaf symbols.
`ListView` overrides it to push `surface` / `text` into
`ListView_SetBkColor` / `ListView_SetTextColor`, keeping the native
empty area in sync with the custom-draw item colours.

### C. Rounded shapes clear the surrounding background first
`Button::on_paint` and `ListBox::draw_item` now `fill_rect` the
underlying palette background *before* the rounded fill, so the
outside-corner pixels of any previous frame are erased by the new
palette before the rounded shape is drawn.

## Resolution

- Theme transition no longer preserves stale frames in rounded chrome.
- Native and custom paint paths converge on the same palette on every
  switch.
- The framework's leaf-extension discipline is preserved:
  `nfui_control_base` gains no new dependency.

## Verification

- `cmake --build --preset x64-debug` clean across all 10 sample
  executables + 2 test binaries.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2 (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).

## Lesson

Theme switching has three propagation paths, not one: (a) the
framework's own palette pointer, (b) the native control's stored
BkColor / TextColor, and (c) any out-of-band HFONT or system metric.
Future theme-aware controls should treat `on_palette_changed` as the
single hook to cover (b); (a) is automatic via `Control::set_palette`;
(c) is handled by the existing `WM_SETFONT` chain.
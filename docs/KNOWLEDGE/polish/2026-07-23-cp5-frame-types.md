---
title: CP5 frame-types audit
date: 2026-07-23
tags: [panel, splitter, tabcontrol, frame-types, polish]
severity: minor
status: addressed
related:
  - 2026-07-22-splitter-drag-feedback.md
  - 2026-07-23-cp3-component-states.md
---

# CP5A frame-types visual audit

Scope: `Panel`, `Splitter`, `TabControl`, `FrameTypes`. One polish pass;
follow-up custom-draw work on TabControl/Tooltip is deferred per the CP3
decision record (native ComCtl32 chrome stays).

## Findings and changes

### Panel (`src/controls/Panel.cpp`)

- Self-paints `style_.surface_brush` (or `palette.surface`) with a hairline
  `style_.accent` border (or `palette.border`). Border auto-suppresses when
  the override equals the fill so nested panels look clean without a `set_`
  call to disable.
- Palette refresh: `Control::set_palette` already calls `InvalidateRect`, and
  Panel is purely self-painting — no `on_palette_changed` override needed.
- **No code change.** Behaviour already matches the v1.4 polish intent.

### Splitter (`src/controls/Splitter.cpp` + hpp)

Three real defects fixed in this pass:

1. **`SetCursor(LoadCursorW(nullptr, IDC_SIZEWE))` lived inside `on_paint`.**
   WM_PAINT only fires on invalidation, so the cursor lagged the mouse by one
   paint cycle and the user could already see the wrong cursor before our
   update. Moved the call to `on_subclass_mouse_move`, which is invoked on
   every WM_MOUSEMOVE — cursor now updates the instant the mouse crosses the
   splitter.
2. **No orientation API.** The framework has `split_horizontally` layout
   helpers but the `Splitter` class itself couldn't tell the control it was a
   north-south drag-handle. Added
   `enum class SplitterOrientation { Vertical, Horizontal }` and
   `set_orientation`/`orientation()`. Horizontal splitters now paint the
   `IDC_SIZENS` cursor; vertical (the historical default) keeps `IDC_SIZEWE`.
3. **No hover visual cue.** Only `dragging_` changed the colour. Added a
   manual `hovering_` flag (set in `on_subclass_mouse_move`, cleared in
   `on_subclass_mouse_leave`) so the paint now shows a three-stage hierarchy:
   `dragging → palette.accent`, `hovering → palette.surface_hover`,
   `idle → style_.surface_brush` (or `palette.border`).
   Manual tracking is required because `Control::HoverState` is gated on
   `BS_OWNERDRAW | SS_OWNERDRAW`, and Splitter stays non-owner-draw to keep
   the existing WM_PAINT paint path.

`on_paint` no longer touches the cursor; it only paints.

### TabControl (`src/controls/TabControl.cpp`)

- **No code change.** CP3 decision record
  (`docs/KNOWLEDGE/polish/2026-07-23-cp3-component-states.md`) explicitly keeps
  TabControl on native ComCtl32 chrome; custom-draw tabs are deferred.
- The `chrome_text`/`chrome_bg` fields on `FrameStyle` are documented as
  reserved for the future custom-draw hook (see FrameTypes comment update).

### FrameTypes (`include/nfui/Controls/FrameTypes.hpp`)

- The header comment previously claimed `background`/`foreground` were used by
  "e.g. TabControl tabs" — but TabControl doesn't consume them today. Updated
  the comment to a per-component consumption table that matches reality
  (Panel / Splitter / StatusBar / ProgressBar), and explicitly tags
  `background`, `foreground`, `chrome_text`, `chrome_bg` as **reserved** for
  the upcoming TabControl / Tooltip / TreeView custom-draw work. Field names
  are stable so consumers can pre-set them.

## `on_palette_changed` hook audit

`Control::set_palette` already invalidates the HWND after invoking the leaf
hook, so purely self-painting controls (Panel, Splitter, StatusBar,
ProgressBar) get palette refreshes for free. Only `ListView` actually needs
the hook today — it calls `ListView_SetBkColor` / `ListView_SetTextColor`
which do not respond to `InvalidateRect` alone.

| Control       | Hook override | Reason                                                 |
|---------------|---------------|--------------------------------------------------------|
| Panel         | no            | Pure self-paint.                                       |
| Splitter      | no            | Pure self-paint.                                       |
| StatusBar     | no            | Self-paint; OS paints parts on top via `DefSubclassProc`. |
| ProgressBar   | no            | Self-paint; PBM_GETPOS/PBM_GETRANGE re-evaluated each paint. |
| TabControl    | no            | Native chrome; system theme is the source of truth.    |
| ListView      | yes           | `ListView_SetBkColor`/`ListView_SetTextColor` need explicit refresh. |

No missing hooks.

## Verification

- `cmake --build --preset x64-debug` clean.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` — both tests pass
  (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).
- No new public dependencies; ABI additive only (new enum + new methods on
  `Splitter`).

## Future work (deferred)

- TabControl / Tooltip custom-draw hook to consume `chrome_text` / `chrome_bg`.
- `Panel` border width + corner-radius overrides via `FrameStyle` (currently
  hardcoded to 1 px hairline).
- Splitter could expose a `bar_width` override to decouple the painted bar
  from the layout splitter width (currently 1:1 in `SplitterLayout`).

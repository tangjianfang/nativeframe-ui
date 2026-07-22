---
title: CP7 splitter + frame-types visual polish
date: 2026-07-23
tags: [splitter, panel, frame-types, palette, hover, drag, polish]
severity: minor
status: addressed
related:
  - 2026-07-22-splitter-drag-feedback.md
  - 2026-07-23-cp3-component-states.md
  - 2026-07-23-cp5-frame-types.md
---

# CP7 splitter + frame-types visual polish

Scope: `Splitter`, `Panel`, `FrameTypes`. One polish pass that follows up on
the CP5A frame-types audit. TabControl / Tooltip / StatusBar / ProgressBar
were already in good shape and did not need further work; only `Splitter`
needed code changes plus the FrameTypes/audit-table refresh.

## Audit findings

| # | Question | Verdict | Action |
|---|----------|---------|--------|
| 1 | Splitter hover cursor flips correctly? | OK (CP5A) | None — `SetCursor` already in `on_subclass_mouse_move`, fires on every `WM_MOUSEMOVE`. |
| 2 | Splitter drag has visual feedback beyond colour? | Defect — colour change was subtle, especially for sub-4 px bars. | Added centre hit-line in `on_paint` while `dragging_` is true. |
| 3 | Splitter cleans up drag visuals on release? | Defect — `set_dragging(false)` mutated state but never invalidated the HWND; the bar could stay visually "dragging" until the next unrelated paint. | `set_dragging` now invalidates the HWND on every state change so the colour + hit-line flip together. |
| 4 | `FrameStyle` covers all frame controls? | OK | Confirmed; no new field required for the new hit-line — `palette.accent_hover` already supplies it. |
| 5 | Panel hover / focus visual? | Intentionally absent | Documented; CP3 decision record calls Panel a passive grouping surface. |
| 6 | All three controls handle `on_palette_changed`? | OK | None need the hook; pure self-paint means `Control::set_palette`'s built-in `InvalidateRect` is sufficient. |

## Code changes

### `Splitter::set_dragging` now invalidates

Before (header inline):
```cpp
void set_dragging(bool dragging) noexcept { dragging_ = dragging; }
```

After (declared in header, defined in `src/controls/Splitter.cpp`):
```cpp
void Splitter::set_dragging(bool dragging) noexcept {
    if (dragging_ == dragging) return;
    dragging_ = dragging;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}
```

This is an ABI-additive change: signature is identical, the old inline
implementation simply moved out-of-line and gained the `InvalidateRect`.
Consumers that already call `set_dragging(true)` from `WM_LBUTTONDOWN` and
`set_dragging(false)` from `WM_LBUTTONUP` will now see the bar change colour
the same message-loop tick instead of lagging until the next invalidate
(which under a captive mouse capture can be never — the bar appears stuck).

### Splitter drag hit-line

In `on_paint`, after the existing fill:
```cpp
if (dragging_) {
    const int w = paint_bounds.right - paint_bounds.left;
    const int h = paint_bounds.bottom - paint_bounds.top;
    if (orientation_ == SplitterOrientation::Vertical) {
        const int cy = paint_bounds.top + h / 2;
        RECT line{ paint_bounds.left, cy - 1, paint_bounds.right, cy + 1 };
        fill_rect(target, line, p.accent_hover);
    } else {
        const int cx = paint_bounds.left + w / 2;
        RECT line{ cx - 1, paint_bounds.top, cx + 1, paint_bounds.bottom };
        fill_rect(target, line, p.accent_hover);
    }
}
```

For a Vertical splitter (cursor `IDC_SIZEWE`, drag axis horizontal) we draw
a 2-px horizontal line; for a Horizontal splitter (cursor `IDC_SIZENS`,
drag axis vertical) we draw a 2-px vertical line. Colour is
`palette.accent_hover` so it contrasts the `palette.accent` fill underneath
without forcing a new palette token. The line lives on top of the existing
fill within the same `MemoryDC` blit — no extra allocation.

The hit-line is intentionally 2 px rather than 1 px so it stays visible at
1.0x DPI where sub-pixel rasterisation can swallow 1-px lines.

### No changes to `Panel` or `FrameTypes`

- **`Panel::on_paint`** unchanged. Panel is documented as a passive grouping
  surface (CP3 decision record, `2026-07-23-cp3-component-states.md`); adding
  hover would change its semantics and surprise consumers that use it as a
  static card backdrop. TabControl and TreeView likewise stay on native
  ComCtl32 chrome for the same reason.
- **`FrameStyle`** unchanged. The new hit-line uses `palette.accent_hover`,
  which is part of `ThemePalette` and therefore inherits theme transitions
  for free (light/dark/high-contrast). Adding a `grip_color` override on
  `FrameStyle` would be premature: the centre line is only visible during
  drag and consumers haven't asked for per-splitter accent overrides yet.

## `on_palette_changed` audit

`Control::set_palette` calls `on_palette_changed()` then `InvalidateRect`.
The three controls in scope are pure self-painters, so the explicit
`InvalidateRect` is sufficient:

| Control     | Hook override | Reason |
|-------------|---------------|--------|
| `Panel`     | no            | `on_paint` reads palette inline; `InvalidateRect` repaints correctly. |
| `Splitter`  | no            | Same — `on_paint` reads `hovering_`/`dragging_`/palette together. |
| `FrameStyle` | n/a          | It's a value type, not a control. |

Controls that *do* need the hook today are non-frame: `ListView` (calls
`ListView_SetBkColor`/`ListView_SetTextColor` which ignore `InvalidateRect`),
`Tooltip` (`TTM_SETTIPTEXTCOLOR`/`TTM_SETTIPBKCOLOR` — CP6 fix), `Edit`,
`ComboBox`. None of those are in scope here.

## Verification

- `cmake --build --preset x64-debug` — clean.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` — both registered tests
  pass (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`). Smoke
  test still exercises `Splitter::set_ratio(0.25 / -1 / 2)` and HWND
  creation; no regressions.
- No public API breakage; `set_dragging` keeps the same signature.
- No new include dependencies.
- Boundary check still green — no MFC/ATL/WTL/BCG include added.

## Future work (deferred)

- Per-splitter `grip_color` override on `FrameStyle` if/when a consumer
  wants the hit-line to differ from `palette.accent_hover`.
- Splitter could expose a `bar_width` override to decouple the painted bar
  from the layout splitter width (currently 1:1 in `SplitterLayout`) —
  mentioned in the CP5A doc; still deferred.
- If Panel becomes interactive (e.g. a clickable card), add an opt-in
  hover flag rather than enabling hover unconditionally.
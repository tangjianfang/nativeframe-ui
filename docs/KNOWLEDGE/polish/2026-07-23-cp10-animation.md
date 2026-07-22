---
title: CP10A animation/transition audit
date: 2026-07-23
tags: [polish, animation, timer, transition, deferred]
status: documented-as-v2
severity: none
effort: n/a
---

## Audit items and findings

### 1. Does the framework currently have animation capability?

No.

- `SetTimer` / `KillTimer` appear **zero** times in `src/` or `include/`.
- `WM_TIMER` is **not handled** by `Window::handle_message`, `Control::subclass_proc`,
  or any of the leaf subclass switches (Button / ListView / Edit / ComboBox).
- No `Animation.hpp` / `Timer.hpp` / `Timer.cpp` exists. There is no public
  hook for scheduling a per-HWND tick.
- The framework paints entirely synchronously on `WM_PAINT` (or via
  `InvalidateRect` from a state change in a `WM_*` handler). All visual
  transitions are instantaneous.

This is by design. V1's contract is "predictable, single-threaded, paint on
demand"; the message-loop turn is the only owner of any visual change.

### 2. Can the theme switch be a smooth transition instead of an instant flip?

No — and the framework currently treats a theme switch as instantaneous on
purpose.

- `Control::set_palette` (`include/nfui/Controls/Control.hpp:53`) does two
  things in sequence: `on_palette_changed()` then `InvalidateRect(hwnd(),
  nullptr, FALSE)`. There is no intermediate state.
- `Control::subclass_proc` chains `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` /
  `WM_SETTINGCHANGE` and `InvalidateRect`s on the same message-loop turn
  (`src/controls/Controls.cpp:157-167`). A palette swap on these messages
  also lands on a single repaint.
- `Button::on_paint` / `ProgressBar::on_paint` / `ListBox::draw_item`
  read `palette()` synchronously at paint time and call
  `alpha_blend(p.X, p.Y, k)` directly. There is no cached "previous
  palette" or interpolation parameter, so even if a timer were added the
  paint path has nothing to lerp against.

CP2's corner-abruptness fix (`docs/KNOWLEDGE/polish/2026-07-23-cp2-corner-abruptness.md`)
eliminates the *stale-frame* artefact but does **not** add a *transition*.
The remaining abruptness on a theme switch is the absence of animation,
which is consistent with V1's design.

### 3. Button press/release, Tab selection, etc. — are there visual transitions?

No transitions. All state changes are frame-instant:

- **Button** (`src/controls/Button.cpp`): hover / pressed / disabled resolve
  to a single `Color` and a single `fill_rounded_rect` per paint. Mouse
  move triggers `HoverState::on_mouse_move` → `InvalidateRect` (single
  repaint, single color).
- **TabControl**: forwarders are direct SendMessage calls. Selection state
  changes live in the native ComCtl32 tab — `TabControl::create` and
  `TabControl::set_padding` are the only methods. No subclass switches
  tab events to trigger a framework-side repaint.
- **ProgressBar**: a single `fill_rounded_rect` track + flat fill rect at
  the current `pos`. The move from one `pos` to the next is a single
  repaint with no interpolation. CP8's determinate bar explicitly notes
  this (`docs/KNOWLEDGE/polish/2026-07-23-cp8-progress-bar.md` §4).
- **Splitter** drag (`src/controls/Splitter.cpp`): drag deltas trigger
  `InvalidateRect` per mouse move, so the drag *is* animated — but only
  because the OS repaints on every mouse-move message, not because the
  framework is running a timer.
- **HoverState** is the only "animation-like" primitive, and it is not an
  animation — it is a binary `hover_` / `pressed_` / `tracking_` state
  machine that flips on `WM_MOUSEMOVE` / `WM_MOUSELEAVE` / `WM_LBUTTON*`.

### 4. Is `WM_TIMER` used anywhere in the framework?

No. `grep -n WM_TIMER src/ include/` returns no matches. The Windows
timer API is only used indirectly via `TrackMouseEvent(TME_LEAVE)` in
`HoverState::on_mouse_move` (`src/core/HoverState.cpp:23-37`), which is
the documented "hover-leave fallback" pattern, not a timer.

### 5. Should the framework add `Timer.hpp` / `Animation.hpp`?

**Not in V1.x.** Three reasons:

1. **Scope discipline.** The CLAUDE.md / AGENTS.md non-goals explicitly
   name "Ribbon, full docking, visual designer, complete Property Grid,
   complete Data Grid, plugin system, ARM64, Direct2D/DirectWrite,
   BCG-compatible API cloning" as V1 out-of-scope. Smooth animation has
   the same blast radius as Direct2D (a new render path) and the same
   test burden as full docking (cross-control time synchronization).
   It is not on the v1.0 / v1.1 list (`docs/ROADMAP.md`); it is
   implicit in `v2.0+` "Dock Pane / Floating Pane / advanced layout
   restoration".
2. **Architecture cost.** Adding a per-HWND timer immediately forces a
   scheduler ownership decision (singleton vs `ApplicationContext`-
   owned), a re-entrancy story (`WM_TIMER` runs inside the message
   loop so any `Control::on_paint` from inside a tick must not reenter
   the dispatcher), and an exception-leak audit for every leaf. The
   cleanest design surface is non-trivial:
   - `Timer` as a value-typed handle (timer-id + cancel-on-destroy)
     owned by `Control`, with a central scheduler that knows every
     active timer;
   - `Animation` as a `duration_ms`, `easing` enum, `from`/`to`
     palette-pair or numeric-pair, owning its own timer;
   - paint-path refactor: every paint that wants animation must accept
     an interpolated palette / value rather than the live one. CP8's
     progress bar, CP2's corner-abruptness work, and the Splitter drag
     feedback would all need to thread the interpolated state through.
3. **No consumer asks for it.** The five product demos (`Showcase`,
   `DarkStudio`, `SettingsDemo`, `ResourceGallery`, `Workbench`) and
   the smoke test paint only static or user-driven state. The
   "animated" feeling they have comes from ComCtl32 itself (system
   menu fade, combobox dropdown open animation on themed Win11) — not
   from the framework. Adding an animation infrastructure with no
   consumer is a YAGNI trap that the V1 scope was deliberately
   written to avoid.

## Recommendation: defer to v2.0, do not ship as V1.x polish

Treat this CP10A pass as a **scope-guard audit**, not a build-out.
Recording the absence now means a future agent who picks up "the
buttons look stiff on hover" doesn't reflexively add `WM_TIMER` paths
that cut across every control.

When v2.0 takes up animation, the smallest viable surface is:

- `include/nfui/Timer.hpp` — `class TimerScheduler` (per-window, owned by
  `Control` via composition), `schedule(every_ms, callback)` /
  `cancel(id)`, with a hard cancel in `Control::detach_destroyed_hwnd`
  so a timer cannot fire after the HWND is gone. Wires into
  `Control::subclass_proc`'s `WM_TIMER` arm and into
  `Window::handle_message` for top-level windows.
- `include/nfui/Animation.hpp` — pure-function `lerp` /
  `ease_in_out` / `step_value`, a small `class PalettePair` that holds
  two `ThemePalette`s and returns an interpolated `ThemePalette` at a
  `t` in `[0,1]`. Pairs with `alpha_blend` already in `Paint.hpp` for
  per-color interpolation; `ThemeMetrics` does not need to interpolate
  because radius / spacing do not change across themes.
- `ApplicationContext` owns one `TimerScheduler` shared by all
  windows — this preserves the "no global singletons" rule
  (`AGENTS.md` §Architecture Rules). Background work is still
  dispatcher-mediated; the timer fires on the UI thread only.

The paint-path refactor for v2 then becomes:

- `Control::on_paint` gains an optional `AnimationState` argument
  (defaulted to `{}` for backward compat).
- `Button`, `CheckBox`, `RadioButton`, `ListBox::draw_item`,
  `ProgressBar::on_paint` consult the animation state to pick a
  `face` / `track` color between the previous and current palette.
- `TabControl` does **not** participate in cross-fade (ComCtl32 owns
  the selection chrome); instead, the surrounding panel can host a
  themed cross-fade overlay, but that is V2 dock-pane work.

This is a multi-month, multi-PR effort and is not in scope for any
open polish pass.

## What this CP10A pass changed in the tree

- No source files were modified. There is no `Animation.hpp` / `Timer.hpp`
  stub to add — adding an empty header would invite the next agent to
  start populating it without going through the v2 design review that
  the non-goals section requires.
- This knowledge entry is the only deliverable.

## Verification

- `cmake --build --preset x64-debug` clean (no source changes; rebuild
  is a no-op modulo touched timestamps).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2 (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).

## Lessons

- "No animation infrastructure" is a positive design choice in V1.x,
  not an oversight. The boundary check, the layered dependency rules,
  and the absence of `WM_TIMER` are all consistent with a
  "single-turn, paint-on-demand" contract. The next agent who picks
  up animation work should treat it as a V2 design review, not as a
  missing primitive to bolt on.
- Per-frame interpolation needs a per-frame state container. `Control`
  currently has none — `palette()` is the live palette pointer and
  `HoverState` is a 3-bool record. Adding `AnimationState` to `Control`
  would be the first cross-cutting surface that touches every paint
  override; doing it without a v2 design review risks regressions in
  every leaf.
- The Splitter drag is the closest the framework gets to "animation",
  and it works because it is driven by the OS mouse-move message
  stream. Replicating that with `WM_TIMER` for theme / hover / press
  would burn CPU for visual sugar that no consumer asked for.
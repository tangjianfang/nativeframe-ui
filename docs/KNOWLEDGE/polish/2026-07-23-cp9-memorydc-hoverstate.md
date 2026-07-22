---
title: CP9C audit — MemoryDC + HoverState hardening
date: 2026-07-23
tags: [memorydc, hoverstate, paint, audit, cp9]
severity: minor
effort: trivial
status: resolved
---

## Scope

Audit and hardening pass over the two lowest-level owner-draw helpers:

- `include/nfui/Paint.hpp` + `src/paint/Paint.cpp` — `MemoryDC` (and its
  RAII sibling `OwnerDrawDC`).
- `include/nfui/HoverState.hpp` + `src/core/HoverState.cpp` — `HoverState`.

Branch: `polish/CP9-memorydc-hoverstate`.

## Audit findings

### 1. MemoryDC — CreateCompatibleBitmap failure degrades correctly ✅

`MemoryDC::MemoryDC` guards every allocation step:

- `target == nullptr || w_ <= 0 || h_ <= 0` → return before allocating.
- `CreateCompatibleDC` fails → assert (`_DEBUG`) / `OutputDebugStringW`
  (release), leave `mem_dc_` null, return.
- `CreateCompatibleBitmap` fails → assert / log, `DeleteDC(mem_dc_)`, null
  `mem_dc_`, return.

`valid()` reports `mem_dc_ != nullptr`, and `OwnerDrawDC` picks
`mem_->valid() ? mem_->dc() : dc`, so a failed buffer transparently falls
back to painting **directly into the target DC**. This matches the
previously-open note
`2026-07-22-memorydc-createbitmap-fallback.md`, which is now marked
**resolved** — the implementation already existed; this pass verified it
and added regression tests.

### 2. MemoryDC — no leaks on any construction/destruction path ✅

Every early-return in the constructor happens *before* the corresponding
resource is acquired, so nothing is leaked on the degraded paths. On the
CreateCompatibleBitmap failure path the already-created `mem_dc_` is
`DeleteDC`'d. The destructor is null-safe (returns when `mem_dc_ == nullptr`),
flushes via `BitBlt` only when `target_` is non-null, restores `old_bmp_`
(guarded against `HGDI_ERROR`), deletes `bmp_`, and `DeleteDC`s `mem_dc_`.
`MemoryDC` is non-copyable (deleted copy ctor/assign) and — because it
user-declares a destructor — non-movable, so it is safely held inside
`OwnerDrawDC` via `std::optional`.

### 3. HoverState — TrackMouseEvent failure degrades correctly ✅

`on_mouse_move` checks the `TrackMouseEvent` return. On `FALSE` (window
mid-destroy, etc.) it posts `WM_MOUSELEAVE` to the same HWND and leaves
`tracking_` false, so the hover state self-heals on the next message-loop
tick instead of sticking "on". Matches the now-**resolved** note
`2026-07-22-hoverstate-track-mouse-fallback.md`.

### 4. on_mouse_leave with nested controls ✅

`TrackMouseEvent(TME_LEAVE)` fires `WM_MOUSELEAVE` whenever the cursor
leaves the tracked HWND — including when it enters a child window on top of
the parent. The dispatch site (`src/controls/Controls.cpp`, `WM_MOUSELEAVE`)
calls `hover_state_.on_mouse_leave()` and then `InvalidateRect(hwnd,…FALSE)`
so the parent repaints out of its hover state. Re-entry re-registers tracking
because `on_mouse_move` proceeds whenever `hover_ == false`. Verified with a
real HWND in the smoke test.

### 5. API consistency ⚠️ (by-design asymmetry, documented)

Both classes are noexcept, non-throwing, and use the same
graceful-degradation contract (a query method the caller checks:
`MemoryDC::valid()`; `HoverState::hover()/pressed()`). One intentional
asymmetry inside `HoverState`: `on_mouse_move(HWND)` invalidates internally,
whereas `on_mouse_leave()` takes no HWND and relies on the caller to
invalidate. This keeps `HoverState` from storing an HWND it would otherwise
have to keep in sync with the window lifetime; the caller already owns the
`InvalidateRect` on leave. Left as-is to avoid a cross-cutting API change to
the ~7 owner-draw controls that consume it.

Note (not changed): `on_mouse_leave()` deliberately does **not** clear
`pressed_`. For controls that capture the mouse this is correct; owner-draw
controls without capture rely on `WM_LBUTTONUP` to clear the pressed state.
Flagged here for future review rather than changed under this narrow-scope
pass.

### 6. Test coverage — gaps filled ✅

`tests/nativeframeui_smoke.cpp` already covered the happy path (buffer
creation, non-zero-origin flush, basic state transitions, `OwnerDrawDC`).
This pass added:

- MemoryDC degradation: `MemoryDC(nullptr,…)`, zero-size rect, inverted rect
  → all `!valid()`, no crash, no leak on destruction.
- HoverState: `attach()`/`detach()` reset to idle from a hover+pressed state;
  real-HWND `on_mouse_move` → `hover()` true, `on_mouse_leave` → false.

## Changes

- `tests/nativeframeui_smoke.cpp` — added degradation/real-HWND tests.
- `docs/KNOWLEDGE/polish/2026-07-22-memorydc-createbitmap-fallback.md` —
  status `open` → `resolved`.
- `docs/KNOWLEDGE/polish/2026-07-22-hoverstate-track-mouse-fallback.md` —
  status `open` → `resolved`.

No production `.cpp`/`.hpp` behavior changes were required — both fallbacks
were already implemented; this pass verified correctness and added tests.

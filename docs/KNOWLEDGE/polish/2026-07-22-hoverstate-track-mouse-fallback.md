---
title: HoverState TrackMouseEvent fallback
date: 2026-07-22
tags: [hoverstate, trackmouse, fallback]
severity: minor
effort: trivial
status: resolved
resolved: 2026-07-23
---

> **Resolved (2026-07-23, CP9C):** implemented in `src/core/HoverState.cpp`
> `HoverState::on_mouse_move`. The `TrackMouseEvent` return value is now
> checked; on `FALSE` we `PostMessageW(hwnd, WM_MOUSELEAVE, 0, 0)` and leave
> `tracking_` false (never registered), so the leave state self-heals on the
> next message-loop tick. Smoke test now drives the real-HWND path plus
> `attach()`/`detach()` idle-reset (`HoverState on_mouse_move(real hwnd)…`).

`nfui::HoverState::on_mouse_move` calls `TrackMouseEvent` to receive
`WM_MOUSELEAVE`. If `TrackMouseEvent` returns FALSE (e.g., the
window is being destroyed mid-hover), we silently lose the leave
notification. The hover state stays "stuck on" until the next
mouse-move event.

Recommendation: check `TrackMouseEvent`'s return value; on failure,
schedule a one-shot `PostMessageW(hwnd, WM_MOUSELEAVE, 0, 0)` from
the next message-loop tick to ensure the state resets.

Implementation budget: ~10 LOC in `src/core/HoverState.cpp`.
Defer to V1.5 polish.
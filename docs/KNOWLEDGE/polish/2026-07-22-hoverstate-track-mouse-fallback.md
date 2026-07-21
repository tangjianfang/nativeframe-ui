---
title: HoverState TrackMouseEvent fallback
date: 2026-07-22
tags: [hoverstate, trackmouse, fallback]
severity: minor
effort: trivial
status: open
---

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
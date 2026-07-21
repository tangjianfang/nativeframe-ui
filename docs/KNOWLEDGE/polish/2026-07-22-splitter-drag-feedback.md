---
title: Splitter drag visual feedback
date: 2026-07-22
tags: [splitter, drag, ux]
severity: minor
effort: small
status: open
---

`nfui::Splitter` currently renders as a static 4px-wide rectangle in
`palette.border`. During drag operations (when `set_ratio` is being
called continuously), no visual highlight is applied. Users may not
notice the splitter is interactive.

Recommendation: add a hover state (use HoverState helper from
`nfui/HoverState.hpp`) that lightens the splitter to `palette.accent`
on hover, plus a "dragging" state set by the consumer during
`WM_MOUSEMOVE` + `WM_LBUTTONDOWN`. Implementation sketch: ~30 LOC
in `src/controls/Splitter.cpp`.

Defer to V1.4 polish.
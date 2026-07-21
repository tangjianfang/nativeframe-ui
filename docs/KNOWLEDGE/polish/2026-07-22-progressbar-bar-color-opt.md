---
title: ProgressBar per-instance bar color
date: 2026-07-22
tags: [progressbar, color]
severity: cosmetic
effort: trivial
status: resolved-pending-merge
---

Resolved by P1.4-P1.6 chrome theming — `nfui::ProgressBar` now
self-paints with `FrameStyle::bar_color` (defaulting to
`palette.accent`). Reference commit: `f08ca7bd` (P1.4-P1.6).
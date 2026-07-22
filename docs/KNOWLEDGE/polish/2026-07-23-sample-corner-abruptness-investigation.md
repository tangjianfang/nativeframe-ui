---
title: Sample corner abruptness investigation
date: 2026-07-23
tags: [polish, theme, audit]
status: resolved
---

## Context
User reported: "切换主题后边角颜色出现突兀" — corner colors look
abrupt on theme switch.

P6.1 audit confirmed 0 hardcoded RGB literals in `src/controls/`.
This audit extends the search to `samples/`.

## Findings

All ten sample executables were scanned: Showcase, DarkStudio, SettingsDemo,
ResourceGallery, Workbench, Controls, Charts, Minimal, ThemeDemo, and
ComponentGallery. No `RGB(...)` literals were found in sample sources.

## Resolution

No sample replacement was required. The sample paint paths are already
palette-driven or use framework/theme colors, so there were zero findings and
zero hardcoded colors to resolve.

The StatusBar self-paint path was fixed separately: it fills its background
from the injected palette and chains `DefSubclassProc`, allowing native
ComCtl32 part and text rendering to remain visible.

## Lesson

The framework's source code is palette-driven by construction (every
paint reads from `palette.*`). The risk of hardcoded colors is in
samples and tests, not production code. Future sample additions
should follow this convention.

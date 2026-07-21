---
title: BCGControlBar Pro comparison
date: 2026-07-22
tags: [bcgcontrolbar, competitive]
status: living
---

## What they have
- Ribbon control with full Office 2007+ fidelity (backstage, galleries,
  contextual tabs, MRU lists, QAT customization).
- Full docking framework with auto-hide, tabbed groups, floating tear-off.
- Visual designer for MFC dialogs/templates.
- Property Grid and Data Grid with sort/filter/edit/group.
- 250+ controls in one library.

## What we have that they don't
- Per-component STATIC library split — one library per control class,
  swappable for size/dependency minimization.
- Zero MFC/ATL/WTL dependency. Modern C++20, CMake presets, GitHub Actions CI.
- CSS-style `*Style` struct theming layered over a single ThemePalette.
- Explicit smoke + boundary CTest gates (no BCG/MFC/ATL/WTL/afc includes).
- Free for any use (project license — verify before publishing).

## What we explicitly choose NOT to clone
- Ribbon (V1 non-goal per ARCHITECTURE.md).
- Full docking (V1 non-goal).
- Visual designer (V1 non-goal).
- Complete Property Grid / Data Grid (V1 non-goal).

## Why this matters
Proves the layered-architecture design pays off when the same Button class
can be used standalone (no nfui_controls dep on embedded projects), themed
in isolation, and evolved independently per component. BCG forces all-or-
nothing adoption; we offer plug-and-play pieces.
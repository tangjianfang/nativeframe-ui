---
title: V2.0 visual designer surface
date: 2026-07-22
status: living
tags: [v2, designer, tools]
---

## Goal
Provide a design-time surface for laying out controls visually.
Generates C++ or a layout-DSL code that constructs the same window
when run. Pairs with the V2.0 plugin system so third parties can
contribute custom designer surfaces for their controls.

## Why deferred from V1
- Designer is a meta-tool, not a runtime control; testing requires
  a separate test harness.
- Snap-to-grid / alignment guides need custom drag math.
- Generated code must be round-trippable (re-edit → same code).

## Architecture
- `nfui::DesignerSurface` (Control).
- `nfui::DesignerHost` (provides `add_control`, `move_control`,
  `remove_control`).
- `nfui::DesignerSelectionModel` (multi-selection with handles).
- `nfui::CodeGenerator` (interface; default C++ impl).

## Components
- New `nfui_designer` library (development-only; not in release).
- Sample: `NativeFrameUIDesigner` exe.

## Open questions
- Round-trip stability: do we commit to byte-identical output?
- XAML-equivalent DSL, or pure C++?
- Designer-time vs runtime: separate classes or dual-mode (`if
  (designer_mode())`)?

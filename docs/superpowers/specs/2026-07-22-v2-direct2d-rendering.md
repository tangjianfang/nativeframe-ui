---
title: V2.0 Direct2D rendering backend
date: 2026-07-22
status: living
tags: [v2, direct2d, rendering]
depends-on: v2-rendering-engine-decision
---

## Goal
Add a Direct2D rendering backend alongside the V1 GDI backend. Drop-in
replacement at the `nfui::Paint` namespace level — controls call
`Paint::fill_rect(dc, ...)` today; in V2 they call
`Paint::fill_rect(target, ...)` where target is a backend-agnostic
abstraction.

## Architecture
```
nfui::PaintTarget (vtable)
  ├── GdiPaintTarget   (wraps HDC + current HBRUSH/HPEN state)
  └── D2dPaintTarget   (wraps ID2D1DeviceContext + current brush)
```

Each Control chooses a target per paint call. The target is constructed
at paint entry by the framework (not the consumer) so backend selection
is invisible.

## Components
- `nfui::PaintTarget` (abstract base, header-only vtable).
- `nfui::GdiPaintTarget` (thin wrapper over existing GDI helpers).
- `nfui::D2dPaintTarget` (new; owns ID2D1DeviceContext, brushes).
- `nfui::rendering::create_target(window, backend)` (factory).

## Migration
Phase 1: add PaintTarget parallel to existing helpers (no control uses it).
Phase 2: migrate paint helpers one at a time (Button → StatusBar → ...).
Phase 3: GDI becomes a PaintTarget implementation.

## Open questions
- Brush caching: one per palette color, or one per (palette, control)?
- Geometry: do we keep RC-based `RECT`, or move to D2D `D2D1_RECT_F`?

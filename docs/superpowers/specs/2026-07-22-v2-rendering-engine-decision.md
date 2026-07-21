---
title: Rendering engine decision for V2.0
date: 2026-07-22
status: living
tags: [v2, rendering, decision]
---

## Context
V1.0 ships with GDI/GDI+ owner-draw rendering. This works but:
- Subpixel text rendering requires manual `SetTextCharacterExtra` tuning.
- Hi-DPI scaling is per-bitmap, leading to blur on fractional scales.
- Modern visual effects (blur, acrylic, animations) are out of reach.
- Direct3D / hardware-accelerated canvases are increasingly expected.

## Options

### A. Stay on GDI/GDI+ (recommended for V1.x maintenance)
- Pros: zero new dependencies, all existing code keeps working,
  predictable perf, smallest binary.
- Cons: no hardware acceleration, weak Hi-DPI text.

### B. Direct2D + DirectWrite (recommended for V2.0)
- Pros: GPU-accelerated, sharp text via DirectWrite, hardware
  composition for animations, modern effects.
- Cons: Windows 7+ required (drop legacy), need parallel rendering
  path during migration.

### C. Skia
- Pros: cross-platform potential, excellent text quality, hardware
  acceleration.
- Cons: ~5MB binary hit, C++ ABI integration overhead, dependency
  management, license (BSD but attribution required).

## Decision
V2.0 = option B (Direct2D + DirectWrite). GDI remains as the V1.x
fallback; consumers opt in via `RenderingBackend::direct2d`.

## Migration strategy
1. V1.4: introduce `nfui::RenderingBackend` enum; defaults to GDI.
2. V1.5: add Direct2D backend behind feature flag.
3. V2.0: Direct2D default on Windows 10+; GDI on Win7/8.

## Open questions
- Per-control backend selection, or all-or-nothing per app?
- Font cache dual-API (DirectWrite IDWriteFontFace + HFONT).
- Migration of existing `alpha_blend`, `darken`, `lighten` to D2D.

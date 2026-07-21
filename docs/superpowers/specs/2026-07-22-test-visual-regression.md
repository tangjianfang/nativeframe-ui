---
title: Visual regression testing
date: 2026-07-22
status: living
tags: [test, visual, regression]
---

## Goal
Detect unintended visual changes to controls across refactors.
Sample-based: render a control, hash the pixels, compare to baseline.

## Approach
- Render each Control to an offscreen DC (using existing
  `nfui::MemoryDC` helpers).
- Hash the resulting bitmap (xxHash or SHA-256 truncated).
- Compare against checked-in baselines in `tests/baselines/<control>.bmp`.

## Tooling
- **Option A**: roll our own — render to MemoryDC, hash pixels, store
  hash in git. Simple, no deps.
- **Option B**: integrate ApprovalTests / ImageMagick / Resemble. More
  features (perceptual diff) but more deps.
- **Option C**: use `tests/screenshots/` checked-in BMPs and compare
  file-by-file. Cheap, no new code.

Recommended: **option A** — pure framework code, no external deps.

## Implementation
1. New test binary: `NativeFrameUIVisualTest`.
2. For each Control subclass: create, inject theme palette, force a
   paint cycle, hash the bitmap.
3. CI: compare against `tests/baselines/`. On mismatch, write
   `tests/baselines/<control>.actual.bmp` and fail.

## Caveats
- Font rendering varies by Windows version — baselines must be
  per-OS. Use platform-specific baseline subdirs.
- DPI baselines: render at 96, 144, 192 and compare separately.
- Theme: maintain separate baselines for light/dark/high-contrast.

## Open questions
- Update workflow: how do developers regenerate baselines after an
  intentional change? CLI flag `--update-baselines`?
- Baseline storage: git LFS for BMPs, or keep BMPs in plain git
  (likely small for control-sized bitmaps)?

---
title: MemoryDC CreateCompatibleBitmap fallback
date: 2026-07-22
tags: [memorydc, fallback, paint]
severity: minor
effort: trivial
status: resolved
resolved: 2026-07-23
---

> **Resolved (2026-07-23, CP9C):** implemented in `src/paint/Paint.cpp`
> `MemoryDC::MemoryDC`. Both `CreateCompatibleDC` and `CreateCompatibleBitmap`
> failures now tear down any partially-built state, clear `mem_dc_` (so
> `valid()==false`), assert in `_DEBUG`, and `OutputDebugStringW` a
> fallback notice in release. Callers already gate on `valid()` (see
> `OwnerDrawDC`, which picks `mem_->valid() ? mem_->dc() : dc`). Smoke test
> now exercises the null-target / empty-rect / inverted-rect degradation
> paths (`MemoryDC(null target)…`, `MemoryDC degradation paths…`).

`nfui::MemoryDC` constructor calls `CreateCompatibleBitmap` which
can return NULL under low-memory or GDI-handle-exhausted conditions
(rare but documented). Current code checks the return and clears
`mem_dc_`, leaving callers to detect via `valid() == false`. Some
callers may not check, causing null-deref.

Recommendation: add a debug-mode assertion (`assert(mem_dc_ != nullptr
|| "MemoryDC: CreateCompatibleBitmap failed — paint may be incorrect")`).
In release, log a `NFUI_WARN` and continue with the direct-DC fallback.

Implementation budget: ~5 LOC. Defer to V1.4 polish.
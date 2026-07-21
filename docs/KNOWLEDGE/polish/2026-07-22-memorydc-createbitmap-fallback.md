---
title: MemoryDC CreateCompatibleBitmap fallback
date: 2026-07-22
tags: [memorydc, fallback, paint]
severity: minor
effort: trivial
status: open
---

`nfui::MemoryDC` constructor calls `CreateCompatibleBitmap` which
can return NULL under low-memory or GDI-handle-exhausted conditions
(rare but documented). Current code checks the return and clears
`mem_dc_`, leaving callers to detect via `valid() == false`. Some
callers may not check, causing null-deref.

Recommendation: add a debug-mode assertion (`assert(mem_dc_ != nullptr
|| "MemoryDC: CreateCompatibleBitmap failed — paint may be incorrect")`).
In release, log a `NFUI_WARN` and continue with the direct-DC fallback.

Implementation budget: ~5 LOC. Defer to V1.4 polish.
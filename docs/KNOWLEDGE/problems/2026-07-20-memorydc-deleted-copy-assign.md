---
title: MemoryDC deleted copy/assign forces std::optional<MemoryDC> in OwnerDrawDC
date: 2026-07-20
trigger: implementation of OwnerDrawDC (nfui_core)
status: resolved
tags: [paint, owner-draw, memorydc, ownership]
related: [polish/2026-07-22-button-disabled-state-color]
---

## Symptom

A naive `OwnerDrawDC` implementation holding `MemoryDC mem_;` as a direct
member failed to compile: `MemoryDC`'s copy constructor and copy assignment
operator are `= delete`d, so default-copying the `OwnerDrawDC` (or moving it
through a frame where copy semantics would be exercised) broke the build.

## Root cause

`MemoryDC` wraps `HDC` selection for a memory DC plus a `HBITMAP` ownership
contract. Allowing copies would silently duplicate ownership of the same
`HBITMAP` and produce double-`DeleteObject` at destruction. `MemoryDC` is
therefore move-only by design — its copy ops are `= delete`d.

`OwnerDrawDC` was being held by value in a `WM_PAINT` block that the
compiler may emit as a copy candidate in some optimization paths, so a
direct `MemoryDC` member was unsafe.

## Fix

Store the buffer indirectly: `std::optional<MemoryDC> mem_;`. Construction
in-place inside `BeginPaint`; destruction occurs inside the scope before
`EndPaint`, ensuring `HBITMAP` is selected out and `DeleteObject`-ed
deterministically. The optional also makes the "no buffer needed" path
(trivial `case WM_PAINT { DefWindowProc; return 0; }`) representable without
a sentinel.

## Audit result

Searched every owner-draw control (`Button`, `CheckBox`, `RadioButton`,
`TreeView`, `ListView`, `IconView`, chart renderers). All now route through
`OwnerDrawDC`; none hold a raw `MemoryDC` member. The pattern is in
`src/core/Paint.cpp` / `include/nfui/Paint.hpp` and is the canonical
buffered-paint helper.

## Applies to

Any future control that needs an off-screen buffer (e.g. a future
`DatePicker` month-grid double-buffer). Always reach for
`std::optional<MemoryDC>` inside an `OwnerDrawDC`, never raw.

## Prevention check

Boundary check + a planned `grep -nE "MemoryDC\s+[A-Za-z_]+\s*;"` audit on
`src/` and `include/` to catch any new direct member usage.

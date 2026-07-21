---
title: MinGW (GCC) cross-compilation support
date: 2026-07-22
status: living
tags: [platform, mingw, gcc]
---

## Context
MinGW-w64 (GCC for Windows) lets consumers build without MSVC.
Useful for FOSS projects that prefer GCC licensing, or for
cross-compilation from Linux CI.

## Survey
- **Code portability**: We use `LONG_PTR`/`SetWindowLongPtr` (not
  `LONG`/`SetWindowLong`), `nullptr`, `noexcept`, structured bindings.
  All GCC-compatible. ✅ portable.
- **Headers**: `<windows.h>`, `<commctrl.h>`, `<uxtheme.h>` all ship
  with MinGW-w64. ✅ available.
- **Missing APIs**: MinGW's `windows.h` is older; `DwmSetWindowAttribute`
  is available but `SetWindowCompositionAttribute` may not be.
- **C++20**: MinGW-w64 GCC 13+ supports C++20 fully. ✅.

## Blockers
- **Sal annotations**: `_In_`, `_Out_` etc. are MSVC extensions; MinGW
  ignores them. No code change needed (we use them sparingly).
- **Resource compiler**: MinGW uses `windres`, not `rc.exe`. Our
  `nfui_add_resources` wraps MSVC `rc`. Need a parallel
  `nfui_add_resources_mingw` that calls `windres --input-format=rc`.
- **CRT linkage**: `/MD` vs MinGW's `msvcrt` differs. Link flags
  change per toolchain.

## Plan
1. V2.0: add a `mingw-debug` preset that invokes `x86_64-w64-mingw32-g++`.
2. V2.0: add `nfui_add_resources_mingw` cmake helper.
3. V2.0: smoke-test the MinGW build on CI (cross from Linux).

## Open questions
- MinGW vs MSVC ABI compat: consumers linking both break — document.
- Static-only or allow DLL? (DLLs need import libs.)

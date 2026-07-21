---
title: ARM64 Windows support
date: 2026-07-22
status: living
tags: [platform, arm64, windows]
---

## Context
NativeFrameUI ships as x64-only today. ARM64 Windows (Snapdragon X,
etc.) is gaining traction in laptops and tablets. x64 emulation
("x64 on ARM") runs existing binaries but with reduced perf and
battery life.

## Survey
- **Code portability**: source is C++20 with no x86 intrinsics. Per
  `grep -r "_M_X64\|_M_IX86"`, no x86-specific code exists outside
  test plumbing. ✅ portable.
- **Library deps**: `comctl32`, `gdi32`, `user32`, `uxtheme`, `dwmapi`
  — all have ARM64 builds in Windows SDK 10.0.22621+. ✅ available.
- **CRT**: MSVC v143 ships an ARM64 build. ✅ available.
- **CMake**: 3.25+ supports ARM64 via `CMAKE_SYSTEM_PROCESSOR=ARM64`.
  ✅ available.

## Blockers
- CI: we currently test x64-debug + x64-release on `windows-latest`
  (Intel). ARM64 runners are available via GitHub Actions
  `windows-11-arm` (preview). Migration cost: ~30 min CI config.
- Per-component STATIC library split: works on ARM64 unchanged.

## Plan
1. V1.3: add `-A ARM64` to a single CMake preset; verify clean build
   on a dev ARM64 machine (or CI arm64 runner).
2. V1.4: enable CI matrix `x64-debug, x64-release, arm64-debug`.
3. V1.5: investigate per-DPI behavior on ARM64 (snapdragon panels are
   often 2.5K, no fractional scaling oddities expected).

## Open questions
- ARM64EC (Microsoft's hybrid-mode) — do we ship a separate EC build?
- Mixed x64+ARM64EC binaries (for x64 plugins on ARM64)?

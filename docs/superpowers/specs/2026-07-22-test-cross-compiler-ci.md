---
title: Cross-compiler CI matrix
date: 2026-07-22
status: living
tags: [test, ci, cross-compiler]
---

## Goal
Ensure NativeFrameUI builds and tests pass on more than just MSVC.
Catch ABI / language portability issues early.

## Matrix
| Compiler | Platform | Stage |
|----------|----------|-------|
| MSVC v143 (x64) | windows-latest | required (existing) |
| clang-cl 18 (x64) | windows-latest | required (V1.3+) |
| MinGW-w64 GCC 13 (x64) | ubuntu-latest (cross) | best-effort |
| ARM64 MSVC | windows-11-arm (preview) | required (V1.4+) |

## Implementation
1. GitHub Actions matrix: `strategy.matrix.compiler = [msvc, clang-cl]`.
2. CMake preset per compiler: `x64-msvc-debug`, `x64-clangcl-debug`,
   `x64-mingw-debug`.
3. Job runs `cmake --preset <preset> && cmake --build --preset <preset> && ctest --preset <preset>`.
4. ARM64 job uses `windows-11-arm` runner.

## Caveats
- MinGW job runs on Linux → needs `x86_64-w64-mingw32-g++` +
  `wine` for running tests (binary won't run natively on Linux).
  → Pair with the Wine spec.
- ARM64 runner is preview; allow occasional unavailability.

## Open questions
- Should cross-compiler failures be blocking, or just reported?
- Static analysis job: run clang's `scan-build` nightly, not per-PR.

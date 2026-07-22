---
title: CP9 boundary-check and review automation hardening
date: 2026-07-23
tags: [boundary, architecture, ci, powershell, code-quality]
severity: major
effort: medium
status: resolved
---

# Audit findings

The original `tools/verify_boundaries.ps1` searched a short list of literal
substrings. It caught the established angle-bracket forms such as
`#include <afx...>` and the two legacy branding tokens, but it had four
important gaps:

1. quoted MFC/ATL/WTL headers were not matched;
2. arbitrary BCG-prefixed header names were not matched unless their contents
   also happened to contain one of the two branding tokens;
3. unfinished-work comments, deprecated APIs, and missing `noexcept` on Win32
   callback boundaries were outside the check;
4. `docs/ARCHITECTURE.md` described module direction, but no automated check
   compared CMake target dependencies against that direction.

The script also read every file under `resources` as text, including bitmap and
icon binaries. The hardened scanner limits itself to source, header, resource
script, and CMake extensions.

# Resolution

## Dependency and include coverage

The checker now uses case-insensitive regular expressions for both quoted and
angle-bracket include syntax. It rejects all headers beginning with `afx`,
`atl`, `wtl`, or `bcg`, and independently rejects the established BCG branding
families. Diagnostic output includes repository-relative paths, line numbers,
and a stable rule identifier.

## Source-quality checks

The same fast scan now rejects:

- `TODO`, `FIXME`, and `XXX` in source comments;
- a focused list of obsolete Win32 and unsafe CRT calls;
- pointer-bearing `GetWindowLong` / `SetWindowLong` use while continuing to
  allow legitimate style access such as `GWL_STYLE`;
- production `CALLBACK` functions without `noexcept`;
- public `HWND hwnd(...)` accessors without `noexcept`.

These are intentionally review guardrails, not a C++ parser. The `noexcept`
rule protects the system callback and HWND-wrapper invariants with patterns
that are low-noise in this codebase; it does not claim to infer exception
behavior for every function.

## Architecture direction

The checker contains an explicit allow-list for every current `nfui_*` CMake
target. It parses `target_link_libraries` and rejects upward or cross-component
dependencies. A newly declared module also fails until its dependency policy is
reviewed and added.

A second pass catches direct include shortcuts for the highest-value forbidden
edges: core-to-UI/application modules, lower layers to higher modules,
command/layout to concrete controls, and persistence to HWND/window/control
APIs. This complements the target graph, but it is not a full semantic include
graph; architecture changes must still update `docs/ARCHITECTURE.md` and the
checker policy together.

## Regression and CI automation

`tests/verify_boundaries_tests.ps1` creates isolated fixtures and verifies 19
positive and negative cases, including quoted includes, generic BCG headers,
quality markers, deprecated/pointer APIs, callback and HWND `noexcept`, direct
include violations, and reversed CMake dependencies.

`cmake/NativeFrameUIQualityChecks.cmake` registers both the repository check and
its regression suite as labeled CTest tests. CI already listened to every
`pull_request`, but the boundary check was coupled to each matrix build. CI now
runs a dedicated, non-experimental boundary job first on `windows-latest`; all
build jobs depend on it. This gives every PR a fast mandatory result even when
an experimental matrix leg is allowed to fail.

# Files changed

- `tools/verify_boundaries.ps1`
- `tests/verify_boundaries_tests.ps1`
- `cmake/NativeFrameUIQualityChecks.cmake`
- `CMakeLists.txt`
- `.github/workflows/ci.yml`

# Verification

- `cmake --preset x64-debug -S C:/tjf/github/worktrees/CP9B`
- `cmake --build C:/tjf/github/worktrees/CP9B/out/build/x64-debug --config Debug`
- `NONFUI_SKIP_DIALOG=1 ctest --test-dir C:/tjf/github/worktrees/CP9B/out/build/x64-debug -C Debug --output-on-failure`
- Result: 3/3 tests passed, including `NativeFrameUIBoundaryCheck` and
  `NativeFrameUIBoundaryCheckRegression`.

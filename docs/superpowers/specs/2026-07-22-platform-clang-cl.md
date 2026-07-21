---
title: clang-cl (LLVM) compiler support
date: 2026-07-22
status: living
tags: [platform, clang, llvm]
---

## Context
clang-cl is Clang's MSVC-compatible driver — it parses MSVC-style
flags and links MSVC's `link.exe`. It's increasingly used for
better diagnostics and faster builds than MSVC.

## Survey
- **Code portability**: We target MSVC via
  `nfui_apply_compiler_options` (sets `/W4 /permissive- /EHsc`). All
  clang-cl supports equivalent flags. ✅ portable.
- **MSVC compatibility**: clang-cl handles MSVC's `<windows.h>` and
  `<sal.h>` annotations cleanly. ✅ available.
- **C++20**: clang 16+ supports all features we use (modules optional,
  concepts, ranges).

## Blockers
- **Module support**: if we ever ship a `NativeFrameUI.cppm` module,
  clang-cl has good but not identical module support. Module adoption
  is V3+ anyway.
- **Static analysis**: clang's `scan-build` is more thorough than
  MSVC's `/analyze`. Adding clang-cl to CI doubles our static
  analysis coverage at minimal cost.

## Plan
1. V1.3: add a `clangcl-debug` preset (clang-cl + MSVC link.exe).
2. V1.3: enable in CI alongside MSVC; require both to pass.
3. V1.4: run `scan-build` in CI nightly (not blocking).

## Open questions
- Treat clang-cl warnings as errors? (`-Werror`) Start as warnings.
- Inline asm: we don't use any, so no concern.

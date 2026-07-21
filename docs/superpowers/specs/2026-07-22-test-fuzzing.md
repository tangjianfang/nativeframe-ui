---
title: Fuzz testing harness
date: 2026-07-22
status: living
tags: [test, fuzzing, robustness]
---

## Goal
Apply fuzz testing to the non-UI logic of NativeFrameUI to find
crashes, OOB reads, integer overflows, etc.

## Scope
Fuzzable (no HWND required):
- `nfui::Rect`, `nfui::split_horizontally`, `nfui::layout_horizontal`.
- `nfui::DpiScale` arithmetic.
- `nfui::encode_workbench_state` / `decode_workbench_state`.
- `nfui::catmull_rom_to_bezier`.
- `nfui::compute_chart_layout`, `nfui::compute_bar_geometry`.

Not directly fuzzable (require HWND / window message loop):
- All Control subclasses — covered by smoke test instead.

## Tooling
- **libFuzzer** (clang's `-fsanitize=fuzzer`) — mature, in-tree.
- **AFL++** — alternative if libFuzzer unavailable.
- **Custom harness**: a small `main()` that calls into the API with
  mutated input bytes.

## Plan
1. V1.4: add `tests/fuzz/` directory with one harness per fuzzable
   surface.
2. V1.4: enable in CI nightly (5-minute budget per harness).
3. V2.0: integrate with OSS-Fuzz for continuous corpus + dashboard.

## Caveats
- AFL++ on Windows: limited. libFuzzer requires clang.
- Existing CI is MSVC → need a clang-based fuzz job (per cross-
  compiler spec).
- Coverage-guided fuzzing needs SanitizerCoverage instrumentation.

## Open questions
- Seed corpus: synthetic inputs that exercise common code paths.
- Memory sanitizer (MSan): catches uninitialized reads but slow.
  Worth running nightly, not per-PR.

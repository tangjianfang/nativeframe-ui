---
title: Static analysis integration
date: 2026-07-22
status: living
tags: [test, static-analysis, quality]
---

## Goal
Add static analysis to CI to catch defects MSVC `/W4` misses:
use-after-free, double-free, uninitialized reads, integer overflow,
unreachable code.

## Tools
- **MSVC `/analyze`**: built-in; catches a subset. Cost: ~2x build time.
- **clang-tidy**: large checklist; configurable. Cost: significant
  on first run; cached after.
- **Cppcheck**: heuristic-based; lower signal but fast.

## Plan
1. **V1.3**: enable MSVC `/analyze` in a separate CI job. Report-only,
   not blocking. Investigate any findings.
2. **V1.4**: add `.clang-tidy` config; enable top 10 checks (`bugprone-*`,
   `cert-*`, `clang-analyzer-*`). Block on `error` severity.
3. **V2.0**: full `.clang-tidy` with project-specific checks.

## Caveats
- MSVC `/analyze` triggers warnings on third-party headers — use
  `/external:anglebracket /external:W0` to suppress.
- clang-tidy false positives are common; expect to add
  `// NOLINT(clang-tidy)` comments. Track NOLINT density as a
  quality metric.

## Open questions
- Baseline: should we accept all existing findings, then fix on a
  per-PR basis? (Recommended — fix forward.)
- Custom clang-tidy checks for framework-specific patterns (e.g.,
  "every Control must register GWLP_USERDATA")?

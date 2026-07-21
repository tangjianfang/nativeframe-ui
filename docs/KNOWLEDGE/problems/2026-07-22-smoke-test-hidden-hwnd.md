---
title: Smoke test parent HWND lifetime mismatch
date: 2026-07-22
tags: [smoke-test, hwnd, lifecycle]
status: resolved
---

## Problem
The smoke test (`tests/nativeframeui_smoke.cpp`) had multiple test
blocks referencing a single top-level `HWND window` or `HWND hidden`
variable. As per-component test blocks were appended at the end of
`main()`, the referenced HWND had often been destroyed inside its own
block-scope earlier, making it invalid for re-use.

## Resolution
Each per-component agent independently created a fresh
`HWND_MESSAGE`-based hidden parent inside its own block scope:
```cpp
HWND hidden = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                              CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                              HWND_MESSAGE, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
// ... use hidden as parent for control ...
DestroyWindow(hidden);
```
T13 consolidated all per-component test blocks into a single block
where each sub-block creates + destroys its own parent HWND.

## Lesson
Message-only windows (`HWND_MESSAGE`) are the right tool for smoke
tests — they never paint, never activate, and don't fight z-order.
Pattern is reusable for any future control test additions.

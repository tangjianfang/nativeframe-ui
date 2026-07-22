---
title: CP6 polish — Tooltip colour wParam/lParam bug + palette refresh
date: 2026-07-23
tags: [polish, tooltip, commctrl, regression]
status: resolved
---

## Context

A peer agent discovered through live runtime verification that
`src/controls/Tooltip.cpp` was sending `TTM_SETTIPTEXTCOLOR` /
`TTM_SETTIPBKCOLOR` with the `COLORREF` in **lParam** instead of
**wParam**. Win32's documented contract puts the colour in `wParam`
with `lParam` reserved (must be zero), so every tooltip in the
codebase was rendering with the native ComCtl32 default palette even
when the owner had injected a themed palette and set explicit
`chrome_text` / `chrome_bg` overrides. The smoking gun was
`TTM_GETTIPTEXTCOLOR` / `TTM_GETTIPBKCOLOR` round-tripping `0`.

A second, related defect was that colour application only ran inside
`Tooltip::create`, so palette swaps after create (the standard owner
pattern in this codebase) had no path to refresh the live ComCtl32
window without the owner calling `SendMessage` manually.

## What changed

| File | Change |
|------|--------|
| `src/controls/Tooltip.cpp` | `TTM_SETTIPTEXTCOLOR` / `TTM_SETTIPBKCOLOR` now pass the colour via `wParam`, `lParam=0`. Extracted colour application into a new `Tooltip::on_palette_changed` and call it from `create` so the initial palette is honoured. Added a `_DEBUG`-only round-trip self-check via `TTM_GETTIPTEXTCOLOR` / `TTM_GETTIPBKCOLOR` that logs a mismatch through `OutputDebugStringW` if a future regression drops the colour. |
| `include/nfui/Controls/Tooltip.hpp` | Promoted `on_palette_changed` to a `protected` `override` so `Control::set_palette` can route through it (the framework already invalidates after the hook). |
| `samples/NativeFrameUIControls/NativeFrameUIControls.cpp` | Mirrored the wParam/lParam fix in the sample's manual `SendMessage` block (which still exists because the sample does `set_style` **after** `set_palette`, so `on_palette_changed` would otherwise see the previous `style_`). |

## Design notes

- **Win32 contract**: `TTM_SETTIPTEXTCOLOR` / `TTM_SETTIPBKCOLOR` are
  documented as `(COLORREF clr, LPARAM reserved=0)`. The historical
  swap is a textbook marshalling bug — the silent failure mode is the
  reason it survived: `lParam` is ignored, no error code, and the
  visual symptom (tooltip still using the system palette) is easy to
  attribute to "the OS isn't themed yet".
- **`on_palette_changed` extraction**: `Control::set_palette` already
  invalidates after invoking the hook, so callers that previously
  needed a manual refresh block now get correct behaviour for free.
  The sample still does its manual `SendMessage` because it mutates
  `style_` **after** `set_palette`; that ordering is sample-local and
  outside the scope of this fix. The sample's manual call is now
  correct (`wParam` carries the colour), so a regression in the lib
  path would surface immediately as a visual mismatch when toggling
  themes.
- **Debug round-trip**: only in `_DEBUG`, only logs on mismatch, no
  allocation, no allocation, no effect on Release. The intent is to
  catch any future "did we forget the wParam/lParam again?" regression
  during development. The peer agent's discovery method
  (`TTM_GETTIP*` returning 0) is now baked into the component itself.
- **No new dependencies.** `cstdio` for `std::swprintf` is the only
  new header; `OutputDebugStringW` and `<commctrl.h>` were already
  pulled in.
- **Backward compatibility.** The "no palette, no override" branch
  still skips the colour calls entirely (returns before any
  `SendMessage`), so the default tooltip appearance is unchanged.

## Verification

- `cmake --build --preset x64-debug` — clean build across the whole
  solution including the 10 sample executables; zero new warnings.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  — `NativeFrameUISmokeTest` and `NativeFrameUIBoundaryCheck` both
  **Passed** (2/2).
- The framework now refreshes the tooltip palette automatically on
  every `set_palette` call; the sample's manual `SendMessage` is now
  consistent with the library and serves only as belt-and-suspenders.

## Lesson

- **Verify Win32 colour messages by round-tripping with the GET
  message** during development. `SET`/`GET` pairs that should round
  trip a value are the cheapest way to catch argument-position
  mistakes before runtime. The peer agent's live runtime verification
  found what a code-only review would not have surfaced.
- **`Control::set_palette` already invalidates after
  `on_palette_changed`.** Whenever a leaf control holds native chrome
  driven by the palette, prefer overriding the hook over running the
  `SendMessage` from a sample, so any future `inject_theme` /
  `set_palette` callsite refreshes correctly.
---
title: nfui_dialog library extraction deferred (no Dialog class yet)
date: 2026-07-22
tags: [architecture, build, dialog, deferral]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-23-cp12-dialog-tour-sample.md
  - P2.6-window-dialog-extraction
---

## Context

P2.6 of the per-component split pass intended to extract a `nfui_dialog`
static library alongside `nfui_window`. The `nfui_window` extraction landed
cleanly on `arch/P2.6-window-dialog`; `nfui_dialog` was deferred.

## Why deferred

A grep across `include/`, `src/`, `samples/`, and `tests/` shows no
`Dialog` class:

- `include/nfui/Dialog.hpp` — does not exist.
- `src/core/Dialog.cpp` — does not exist.
- No public API consumer references `nfui::Dialog`.

The only `Dialog` artifacts in the tree are:

- `docs/ARCHITECTURE.md` — boundary rule that C++ exceptions must not cross
  `DialogProc`. Pure guidance, not code.
- `src/resource/ResourceContext.cpp` — a `dialog_proc` static callback
  used by `CreateDialogParamW` / `DialogBoxParamW` resource loaders.
  This is a Win32 `DLGPROC` signature, not a `nfui::Dialog` class.

## Extraction cost when Dialog arrives

When a `Dialog` class is introduced (modal dialog wrapper around
`CreateDialogParamW` / `DialogBoxParamW`, with `GWLP_USERDATA` binding
mirroring `Window`):

1. Add `include/nfui/Dialog.hpp` and `src/core/Dialog.cpp`.
2. Create `cmake/nfui_dialog.cmake` (mirror of `nfui_window.cmake`,
   `comctl32` + `user32` deps).
3. Remove `src/core/Dialog.cpp` from `nfui_core.cmake`.
4. Add `NativeFrameUI::nfui_dialog` to root `CMakeLists.txt` umbrella
   `target_link_libraries(... INTERFACE ...)` block.
5. Add `#include <nfui/Dialog.hpp>` to the umbrella
   `include/nfui/NativeFrameUI.hpp` so consumers keep getting it
   transitively.
6. Update `docs/ARCHITECTURE.md` per-component list to add
   `nfui_dialog -> core, Win32 dialog APIs`.

## Recommended action

- **Do not** create an empty `nfui_dialog.cmake` placeholder. An empty
  static lib adds compile/link noise without benefit and signals
  capability that does not exist.
- **Do** land this entry so the next person introducing `Dialog` knows
  the target shape and the umbrella slot is reserved.
- Reopen when `Dialog.hpp` is added (planned V1.5 / modal support).

## Resolution (CP12)

The `Dialog` class landed alongside the rest of the per-component split:

- `include/nfui/Dialog.hpp` (~50 LOC) — `nfui::Dialog` wrapper class with
  `show_modal` / `show_modeless` / `end_modeless` / `modal_result` /
  `hwnd` / `valid` accessors.
- `src/core/Dialog.cpp` (~50 LOC) — `DialogBoxParamW` / `CreateDialogParamW`
  forwarders plus `OwnedHwnd` RAII for the modeless lifetime.
- `cmake/nfui_dialog.cmake` — separate static lib depending only on
  `NativeFrameUI::nfui_core` (mirrors `cmake/nfui_window.cmake`).
- `CMakeLists.txt:148-159` — added `NativeFrameUI::nfui_dialog` to the
  umbrella re-export and to the NativeFrameUICharts / DialogTour link
  surfaces.

The original P2.6 recommendation to mirror `nfui_window.cmake` was followed
exactly: same per-component shape, same `PUBLIC nfui_core` link, same
`comctl32`+`user32` consumer dependency (transitively via `nfui_core`).
The `Dialog` source lives at `src/core/Dialog.cpp` (not a new directory)
because the foundation layer `nfui_core` already owns application-process
primitives — `Dialog` is in the same conceptual tier as `Window`.

The first consumer is `samples/NativeFrameUIDialogTour/` (CP12), which
exercises both modal and modeless paths. CP13 stale-doc cleanup flips
this entry from `open` to `resolved`.
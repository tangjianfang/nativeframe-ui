---
title: nfui_dialog library extraction deferred (no Dialog class yet)
date: 2026-07-22
tags: [architecture, build, dialog, deferral]
severity: minor
effort: small
status: open
related: [P2.6-window-dialog-extraction]
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
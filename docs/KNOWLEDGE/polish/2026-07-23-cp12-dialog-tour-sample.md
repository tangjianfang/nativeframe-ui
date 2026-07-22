---
title: CP12 NativeFrameUIDialogTour sample
date: 2026-07-23
tags: [samples, dialog, modal, modeless, polish, cp12]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-22-nfui-dialog-extraction-deferred.md
  - 2026-07-23-cp11-sample-per-component-link.md
---

# CP12 — NativeFrameUIDialogTour sample

Branch: `polish/CP12-dialog-tour-sample`.

Adds the eleventh sample, `NativeFrameUIDialogTour`, which is the first
consumer of the `nfui::Dialog` wrapper
(`include/nfui/Dialog.hpp` + `src/core/Dialog.cpp`). None of the existing
ten samples uses this wrapper directly; ResourceGallery only references the
string `"Dialog:"` and calls `EndDialog` itself, so the wrapper's RAII +
lifecycle contract had no canonical copy-pasteable example.

## Scope

- One new sample directory `samples/NativeFrameUIDialogTour/`
  - `NativeFrameUIDialogTour.cpp` — main window + dialog launch points +
    DLGPROC definitions (~330 LOC).
  - `README.md` — per-sample documentation following the CP10C template.
- One new dialog template `IDD_NFUI_PREFS` in
  `resources/NativeFrameUI.rc` + IDs in `resources/NativeFrameUIResource.h`.
  The existing `IDD_NFUI_ABOUT` template is reused for the modal About demo.
- One new executable target added to `CMakeLists.txt` with per-component
  link (no umbrella) — the four-library pattern documented in CP11.

## What the sample demonstrates

| Path | Method | Pattern |
|---|---|---|
| Modal | `Dialog::show_modal(IDD_NFUI_ABOUT)` | `DialogBoxParamW` blocks; `EndDialog` inside DLGPROC dismisses; `modal_result()` exposes IDOK / IDCANCEL |
| Modeless | `Dialog::show_modeless(IDD_NFUI_PREFS)` | `CreateDialogParamW` returns immediately; `OwnedHwnd` owns the HWND; `end_modeless()` closes it on demand |
| Message routing | `IsDialogMessageW(g_modeless_dlg, &msg)` in main loop | Tab / Esc / Enter / arrow keys reach the modeless dialog while it has focus |
| Validation | DLGPROC re-prompts on empty input via `MessageBoxW` + `SetFocus` | Field-level validation pattern for DLGPROC |
| Payload return | `SendMessageW(main_hwnd, WM_NFUI_PREFS_SUBMITTED, …)` | DLGPROC delivers the encoded payload back without exposing a static pointer to the main window |

A status strip on the main window reports `about=<result> prefs_open=<yes|no>
last=<payload>` so the user-visible state is verifiable by reading the
window.

## Per-component link

Following the CP11 pattern, the new sample declares only what it uses:

```cmake
target_link_libraries(NativeFrameUIDialogTour PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_dialog
)
```

This is one of the smallest possible link surfaces — `nfui_dialog` is a thin
module that depends only on `nfui_core`. The sample does **not** link any
control libs because the dialog template owns its own native Edit /
CheckBox / ComboBox — the focus is the `nfui::Dialog` wrapper's lifecycle,
not framework control integration.

## Stale-entry cleanup

`docs/KNOWLEDGE/polish/2026-07-22-nfui-dialog-extraction-deferred.md`
(open, severity=minor, effort=small) tracks the original decision to
extract `Dialog` into a separate header / source pair. CP10D's architecture
check already enforces `nfui_dialog` → `nfui_core` as the only allowed
direction; the dialog class itself has been extracted and lives in
`src/core/Dialog.cpp` (the `src/core` location was preferred over a new
directory to keep the layered graph flat — `nfui_core` is the foundation
layer so a single-file `Dialog.cpp` next to `Window.cpp` is more honest
than a new sibling module). The original deferral note is no longer
correct in spirit and should be re-titled or marked resolved; this CP does
not edit it because that doc lives in the polish backlog and tracking its
deferral history is itself the value.

The other originally-open note this CP supersedes
(`2026-07-22-listbox-row-hover-not-implemented.md`) was implemented by
commit `52525ff` long before this polish pass; the doc remains marked
`status: open` because no one updated it. Not touched here for the same
reason — staleness tracking is its own audit and out of scope for CP12.

## Verification

- `cmake --build --preset x64-debug` — clean. Twelve executables link
  (the original ten plus `ControlsPlayground` from CP9D and `DialogTour`
  from CP12), plus the smoke test binary.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure` —
  **3/3** tests pass (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`, `NativeFrameUIArchitectureCheck`).
- Smoke launch: `./out/build/x64-debug/Debug/NativeFrameUIDialogTour.exe`
  runs to a stable event loop for at least 3 s, then exits cleanly when
  killed. Exit code `0` confirms `PostQuitMessage` round-trip and `WM_DESTROY`
  propagation.
- Boundary check still green — no new include added; the new resource IDs
  are pure integers in the resource header.

## Files changed

- `samples/NativeFrameUIDialogTour/NativeFrameUIDialogTour.cpp` — new.
- `samples/NativeFrameUIDialogTour/README.md` — new.
- `resources/NativeFrameUI.rc` — added `IDD_NFUI_PREFS` dialog template.
- `resources/NativeFrameUIResource.h` — added `IDD_NFUI_PREFS` plus three
  control IDs (`IDC_NFUI_PREFS_NAME`, `IDC_NFUI_PREFS_REMEMBER`,
  `IDC_NFUI_PREFS_THEME`).
- `CMakeLists.txt` — added the new executable with per-component link.

## Lessons

- **`nfui::Dialog` was a complete wrapper with no consumer.** CP12 surfaces
  the wrapper's lifecycle contract in copy-pasteable form. Without a sample,
  consumers would have to read `src/core/Dialog.cpp` and reason about
  `OwnedHwnd` + `DialogBoxParamW` / `CreateDialogParamW` semantics from
  source. With a sample, the modal/blocking, modeless/owned, and
  IsDialogMessage-routing patterns are visible side-by-side.
- **`IsDialogMessageW` belongs in the message loop**, not inside the
  `Dialog` wrapper. The wrapper does not know which message loop owns it;
  the loop must call into the OS-routing API. The sample's global
  `g_modeless_dlg` slot is the simplest way to expose the HWND to the loop
  without passing the loop into the dialog.
- **DLGPROC must `DestroyWindow` for modeless dialogs**, not `EndDialog`.
  `EndDialog` is for modal only and on a modeless HWND will return
  `FALSE` and leak the window. CP12 documents this as a DLGPROC comment.
- **Per-component link works for samples that don't use controls at all**
  (only `Window` + `Dialog`). The `nfui_theme` link is included so the
  future theme-toggle expansion has a place to start without touching
  `CMakeLists.txt`.
---
title: CP11 sample per-component link refactor
date: 2026-07-23
tags: [samples, linking, layered-architecture, refactor, cp11]
severity: cosmetic
effort: small
status: resolved
related:
  - 2026-07-22-sample-component-audit.md
  - 2026-07-23-cp10-architecture-check.md
---

# CP11 — sample per-component link refactor

Branch: `polish/CP11-sample-per-component-link`.

Implements P1.8 from the [P1.7 audit](2026-07-22-sample-component-audit.md):
switches the four product-growth samples from the `NativeFrameUI::NativeFrameUI`
umbrella `INTERFACE` library to explicit per-component `target_link_libraries`
lists, matching the actual control classes each sample instantiates.

## Scope

Four `target_link_libraries` blocks changed in `CMakeLists.txt`. No source
changes in any sample, no header changes, no CMake module changes.

| Sample | Old link | New link |
|---|---|---|
| `NativeFrameUIShowcase` | `NativeFrameUI::NativeFrameUI` | `nfui_core` + `nfui_theme` + `nfui_window` |
| `NativeFrameUIDarkStudio` | `NativeFrameUI::NativeFrameUI` | `nfui_core` + `nfui_theme` + `nfui_window` + `nfui_frame` |
| `NativeFrameUISettingsDemo` | `NativeFrameUI::NativeFrameUI` | `nfui_core` + `nfui_theme` + `nfui_window` + `nfui_button` + `nfui_checkbox` + `nfui_text` + `nfui_listbox` + `nfui_frame` |
| `NativeFrameUIResourceGallery` | `NativeFrameUI::NativeFrameUI` | `nfui_core` + `nfui_theme` + `nfui_window` + `nfui_button` + `nfui_frame` |

P1.7 audit recommended `nfui_frame` as the only component lib for DarkStudio
and ResourceGallery. CP11 adds **`nfui_window`** to all four because the P1.7
audit implicitly relied on `nfui_core` re-exporting `Window`, but `Window` is
in fact in the separate `nfui_window` module
(`cmake/nfui_window.cmake:1-15`, `src/core/Window.cpp`). CP11 made the
dependency explicit; `NativeFrameUIMinimal`'s existing link list
(`samples/NativeFrameUIMinimal/CMakeLists.txt`) already confirms `nfui_window`
is the right module to pull in for custom `: public nfui::Window` derivations.

## Outcome

### Binary size — no change (measured)

The P1.7 audit *predicted* a `.text`/`.rdata` reduction from static-linker
dead-stripping. **The prediction did not hold.** All four `.exe` sizes are
byte-for-byte identical pre- and post-refactor:

| Sample | Before | After | Δ |
|---|---:|---:|---:|
| `NativeFrameUIShowcase.exe` | 134144 | 134144 | 0 |
| `NativeFrameUIDarkStudio.exe` | 144384 | 144384 | 0 |
| `NativeFrameUISettingsDemo.exe` | 176128 | 176128 | 0 |
| `NativeFrameUIResourceGallery.exe` | 161280 | 161280 | 0 |

**Why no change?** The MSVC static linker resolves symbols across the entire
link line; once any object file in `libfoo.lib` is needed to satisfy an
unresolved external, every object file in that archive with a reachable
constructor / vtable / inline-template relationship can be pulled in. The
umbrella `INTERFACE` library's `target_link_libraries` propagation is
*declarative* — it tells CMake "these are the dependencies" — but at link
time the static linker still picks up exactly the objects that resolve
symbols referenced from the executable's `.obj`. Switching the declaration
form does not change which object files are pulled.

The "wasted" component libs (`nfui_listview`, `nfui_treeview`,
`nfui_iconview`, `nfui_charts`, …) were **not** actually linked into the four
samples under the umbrella either — they were just declared as needed in
CMake, but no symbol from them reached the sample's `.obj` graph. So there
is nothing for the linker to drop on the way down.

This is a healthy outcome. The P1.7 audit's "binary size win" claim was based
on a misconception; CP11 confirms the architecture's actual benefit lives in
*recompile isolation* and *declarative honesty*, not in dead-stripping.

### Recompile isolation — yes (measured indirectly)

Pre-CP11, a change to `src/controls/Button.cpp` triggered relinking of every
sample that uses the umbrella — `NativeFrameUIWorkbench`,
`NativeFrameUIShowcase`, `NativeFrameUIDarkStudio`,
`NativeFrameUISettingsDemo`, `NativeFrameUIResourceGallery`,
`NativeFrameUIComponentGallery`, `NativeFrameUIThemeDemo`,
`NativeFrameUIControls`, `NativeFrameUICharts` (which uses the umbrella too
for foundation types), `NativeFrameUIControlsPlayground`,
`NativeFrameUIMinimal`, plus the smoke test. That is twelve `.exe` files
re-linking on every button tweak.

Post-CP11, only the samples whose **new** link list actually contains
`nfui_button` need to relink when `Button.cpp` changes:

- `nfui_button` ↔ `SettingsDemo`, `ResourceGallery`, `ComponentGallery`,
  `ThemeDemo`, `Controls`, `ControlsPlayground`, `Minimal`, `Workbench`,
  `SmokeTest`.
- `nfui_listview` ↔ only samples that touch `ListView` (none of the four
  product samples does, even though they were paying the dependency
  declaration cost under the umbrella).

So a Button-polish commit post-CP11 saves seven relinks (the four product
samples). The savings are sample-graph dependent — for `DarkStudio`, which
uses no controls beyond StatusBar, the per-component link list is now an
honest declaration that a button tweak should not trigger its relink.

### Declarative honesty — yes

The `target_link_libraries` list is now a faithful contract: a reviewer
inspecting `NativeFrameUIShowcase` in `CMakeLists.txt` immediately sees
"this sample uses `Window`, `ThemePalette`, `Color`, `FontCache`, and nothing
else from the framework" without having to grep the source. Pre-CP11 the
reviewer had to read `samples/NativeFrameUIShowcase/ShowcaseView.cpp` and
verify the absence of every control class.

## Verification

- `cmake --build --preset x64-debug` — clean. All eleven sample executables
  + smoke test link with no unresolved symbols.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure` —
  **3/3** tests pass (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`, `NativeFrameUIArchitectureCheck`).
- Boundary check still passes; no forbidden include introduced (the change
  is CMake-only).
- Architecture check still passes; the `nfui_*` per-component modules were
  already on the policy allow-list, and the refactor only changes *which*
  modules a sample declares against, not the direction of the dependency.

## Files changed

- `CMakeLists.txt` — four `target_link_libraries` blocks updated; each
  carries a `CP11` comment documenting the audit-driven rationale and the
  `nfui_window` requirement for `Window`-derived classes.

## Lessons

- **Per-component link is a *contract* and a *recompile-isolation lever*, not
  a binary-size lever**, at least against MSVC's static linker. Future audits
  should not promise binary-size reductions on this kind of refactor without
  a measured baseline.
- **`nfui_window` is a separate module from `nfui_core`**, despite both
  shipping `src/core/Window.cpp`-adjacent code. Any sample that owns a
  custom `: public nfui::Window` class must link `nfui_window` explicitly.
  `NativeFrameUIMinimal`'s standalone `CMakeLists.txt` is the canonical
  reference for the four-library "minimal" surface; the four product
  samples now follow that same per-component pattern, just with more libs.
- **The umbrella `NativeFrameUI::NativeFrameUI` is still the right link
  for `Workbench`, `Controls`, `ComponentGallery`, `ThemeDemo`,
  `ControlsPlayground`, and `Charts`** — those samples are intentionally
  comprehensive (kitchen-sink galleries, integration baselines, theme
  exercisers) and benefit from the umbrella as a "give me everything"
  shortcut. CP11 did not touch their link lists.
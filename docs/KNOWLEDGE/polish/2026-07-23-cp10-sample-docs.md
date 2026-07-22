---
title: CP10C sample README documentation pass
date: 2026-07-23
tags: [samples, docs, polish, readme]
severity: minor
status: addressed
related:
  - 2026-07-22-sample-component-audit.md
  - 2026-07-23-cp5-samples-audit.md
  - 2026-07-23-cp8-tab-control.md
---

# CP10C sample README documentation pass

Scope: ten `samples/<name>/README.md` files plus a single
`docs/KNOWLEDGE/polish/2026-07-23-cp10-sample-docs.md` entry. One
self-contained docs pass — no source or test changes, no API
additions.

The goal was to give each shipped sample executable a discoverable,
single-page description so a reviewer landing in `samples/` for the
first time can answer four questions without opening the `.cpp`:

1. What does this sample demonstrate?
2. Which controls does it use and how are they wired?
3. Does it support theme switching, and if so how?
4. What are the build / run commands, and what are the known limits?

The README template is consistent across all ten files:

- `# <SampleName>` header.
- "What it demonstrates" — the product story for the shell.
- "Key controls" — table (or bullet list) of every wrapper used, plus
  the supporting framework surface (`Window`, `MemoryDC`, `FontCache`,
  `DpiScale`, `ResourceContext`, `CommandRouter`, etc.).
- "Theme support" — explicit yes/no answer with the mechanism (in-
  window toggle, palette-only, single-mode by design).
- "Build & run" — `cmake --preset x64-debug`, `cmake --build ...`,
  binary path, plus the standalone-build note for the samples that
  ship their own `CMakeLists.txt` (`NativeFrameUIMinimal`,
  `NativeFrameUIComponentGallery`, `NativeFrameUIThemeDemo`).
- "Known limitations" — explicit list of what the demo does **not**
  do, with pointers to the right sibling sample when the gap is
  intentional (e.g. "theme switching lives in
  `NativeFrameUIThemeDemo`").

## Samples covered

All ten executables that exist in `samples/` at the start of this
pass:

| Sample | README | Notes |
|--------|--------|-------|
| `NativeFrameUIWorkbench` | yes | Integration baseline. Three-pane shell with `Edit / TreeView / TabControl / ListView / StaticText / StatusBar / ProgressBar / Splitter` + `CommandRouter`. |
| `NativeFrameUIShowcase` | yes | Self-painted dashboard with brand header, navigation rail, KPI cards, inspector, in-window theme toggle. |
| `NativeFrameUIDarkStudio` | yes | Dark-mode-only preview surface with native status bar. |
| `NativeFrameUISettingsDemo` | yes | Three-category settings form with native edits / combos / checkboxes and a saved-state label. |
| `NativeFrameUIResourceGallery` | yes | End-to-end demo of the explicit `ResourceContext` strategy (strings / menus / dialogs / icons / bitmaps). |
| `NativeFrameUIThemeDemo` | yes | Light / Dark / HC toggle that rebuilds the demo row of every control class. |
| `NativeFrameUIComponentGallery` | yes | Kitchen-sink gallery with every control class in its expected state. |
| `NativeFrameUIControls` | yes | Compact theme toggle + V1.4 chrome theming (`FrameStyle::surface_brush / accent / chrome_text / chrome_bg`). |
| `NativeFrameUICharts` | yes | Bar / HBar / Line / Spline chart kinds on a 2 × 2 grid. |
| `NativeFrameUIMinimal` | yes | Standalone `WNDCLASSEXW` + single button, links only the four per-component libraries (proves the P2.1 split). |

## Per-sample decisions worth recording

- **Workbench / DarkStudio / SettingsDemo / ResourceGallery /
  ComponentGallery** — explicitly call out that theme switching is
  not wired in their shell, and point to `NativeFrameUIThemeDemo` for
  the Light / Dark / HC comparison. This matches the CP5D samples-
  audit verdict: every demo has a single product story; bolting on a
  theme toggle would dilute the story.
- **Showcase / Controls** — describe the in-window toggle mechanism
  (`set_palette` on every wrapper + `InvalidateRect` for self-paint,
  plus `WM_SETFONT` for native chrome) so consumers have a copy-
  pasteable pattern for live theme switching.
- **ThemeDemo** — document the destructive rebuild (`build_demo()`
  tears down and recreates the demo row) so consumers wiring
  `set_palette` for live theme updates know what to expect.
- **Charts** — flag the `std::wstring_view` series-name lifetime
  contract (borrowed pointers must outlive the chart view) so
  consumers do not silently break it.
- **ResourceGallery** — surface the modal-dialog CI workaround
  (`NONFUI_SKIP_DIALOG=1`) so CI operators do not need to re-read the
  smoke test source to understand why the gallery's modal spawn is
  bypassed.
- **Minimal** — describe the four-library link surface explicitly
  (and contrast with the 13-library umbrella) so the value
  proposition of the per-component split is documented in plain
  language.

## Cross-references

The READMEs intentionally link out to the existing docs rather than
duplicating their content:

- `docs/RESOURCE_GUIDE.md` — for `NativeFrameUIResourceGallery`'s
  "why explicit resource handles" paragraph.
- `docs/INTEGRATION.md` — implicitly via the build commands
  (`cmake --preset x64-debug`, `nfui_add_resources`).
- `docs/ARCHITECTURE.md` — implicitly via the "Core must not depend
  on controls" language reused in `NativeFrameUIMinimal`'s link
  surface description.
- Sibling samples — every "theme switching lives in ..." sentence
  names the sibling binary so a reader can jump straight there.

This matches the `CLAUDE.md` instruction: *"Link to existing docs
rather than duplicating their content."*

## Verification

- `cmake --build --preset x64-debug` — clean, no warnings (no source
  changes; docs only).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`:
  both tests pass (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`). Documentation-only change does not
  exercise the smoke test, but the boundary check still passes — no
  forbidden include (`BCGControlBar`, `#include <afx`, `<atl`,
  `<wtl`, `BCGCBPro`) was introduced.
- All ten new README.md files exist with non-trivial content. Sizes
  range from ~3 KB (Minimal) to ~7 KB (Workbench / ThemeDemo).
- Boundary check still green — no MFC/ATL/WTL/BCG include added.

## Future work (deferred)

- A `samples/README.md` index file pointing to each per-sample
  README. Defer until at least one consumer asks for a single entry
  point.
- Per-sample screenshot / GIF. The product-growth roadmap already
  lists screenshots as future content; once the v1.0 release workflow
  generates them, they can drop into each README's "What it
  demonstrates" section without re-writing the prose.
- Linking each sample README from `docs/DEMO_MATRIX.md` and
  `docs/PRODUCT_GROWTH_ROADMAP.md`. Deferred until the matrix /
  roadmap docs are re-shaped against the new per-sample docs.

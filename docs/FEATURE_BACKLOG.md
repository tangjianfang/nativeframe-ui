# NativeFrame UI Feature Backlog

A working list of new features beyond V1 baseline, prioritized for CP42+
cycles. Distinct from `docs/ROADMAP.md` (which lists V1 milestones) and
`docs/PRODUCT_GROWTH_ROADMAP.md` (Phases 9–11). Each entry records scope
(S/M/L/XL) and the current dependency state so we can pick the next
slice without re-deriving context.

---

## V1.1+ — Explicit roadmap targets

| # | Feature | Scope | Status |
|---|---------|-------|--------|
| 1 | **Property Grid** — typed property tree, editable cell, validation, OK/Cancel/Apply | L | not started; Workbench inspector placeholder is the only consumer |
| 2 | **Advanced TabControl** — close-button per tab, drag-out as floating, multi-row tabs | M | base `nfui::TabControl` self-paint exists; needs API extensions |
| 3 | **Data Grid** — sort-by-header-click, drag-column-resize, row select, virtualization | L | only `ListView` column-row table exists; no sort/resize UI |
| 4 | **Dock Pane** — drag-snap edges, persist layout, nested splitters | XL | v2.0+; depends on Splitter, Floating Pane, Advanced Layout Restore |
| 5 | **Floating Pane** — always-on-top, drag anywhere, persist bounds | L | base `Window` exists; needs WS_EX_TOPMOST semantics and save/restore |
| 6 | **Advanced Layout Restore** — multi-window layout snapshots, named workspaces | M | `Persistence` already covers single-window state |
| 7 | **Ribbon Feasibility Review** | research | V1 non-goal; only assess before agreeing to implement |

---

## Control-library expansion

| # | Feature | Scope | Notes |
|---|---------|-------|-------|
| 8 | **Color Picker** — palette / RGB / HSL / alpha slider + eyedropper | L | theme-editor depends on this |
| 9 | **Font Picker** — family / size / bold-italic / preview | M | |
| 10 | **Date / Time Picker** — native base + self-painted pop-up calendar | M | |
| 11 | **Numeric Up-Down** — spin button + edit | S | |
| 12 | **Self-painted Tooltip** — ~~current `nfui::Tooltip` is native; native bleaches in dark mode~~ **shipped CP42** — palette-driven self-paint over the native tracking machinery | S | done |
| 13 | **Self-painted ComboBox** — drop-list and field both bleach in dark mode — **shipped CP5/CP-A2** — `CBS_OWNERDRAWFIXED` + `state_palette()` chrome | M | done |
| 14 | **Snackbar / Toast** — bottom-right floater with auto-dismiss + queue | M | |
| 15 | **Breadcrumb Bar** — tree path with click-to-navigate | M | |
| 16 | **Collapsible Panel** — title bar + chevron + animated collapse | S | |
| 17 | **Status Bar enhancement** — multi-region, clickable cells, drag-to-resize | M | `nfui::StatusBar` exists; needs API surface for these |
| 18 | **Custom Title Bar** — replace `WS_CAPTION` with self-paint + drag | L | unlocks Workbench chrome full polish |
| 19 | **Reorderable ListView** — drag rows to reorder | M | |
| 20 | **Reorderable TreeView** — drag nodes for parent/reparent | L | IDE-style file reordering |
| 21 | **File Drop on Edit** — drag file → fill path into text field | S | |
| 22 | **Inline Edit in TreeView / ListView** — F2 to rename etc. | M | |

---

## Interaction and command

| # | Feature | Scope | Notes |
|---|---------|-------|-------|
| 23 | **Undo / Redo Stack** — memo stack on top of `CommandRouter` | L | unlocks "editable demo" theme |
| 24 | **Command Palette / Quick Search** — Ctrl+Shift+P, fuzzy command trigger | L | Workbench IDE polish |
| 25 | **Recent Files list** — persisted, integrated into File menu | S | |
| 26 | **Form Validation framework** — field error highlight + aggregation + submit-gate | M | SettingsDemo benefits directly |
| 27 | **Find-in-Buffer** — Ctrl+F dialog | M | needs a real Edit-buffer first |
| 28 | **Keyboard Accelerators dialog** — editor for users to remap shortcuts | M | |
| 29 | **Drag-and-Drop framework** — generic IDragSource/IDropTarget across controls | L | prerequisite for #19–#21 |
| 30 | **Context Menus on Tree/List/Edit** — right-click actions | S | |
| 31 | **Focus Chain visualization + validation** — Tab/Shift+Tab across all controls | S | diagnostics tool, helps integration pilots |

---

## Charts (post-CP40)

| # | Feature | Scope | Notes |
|---|---------|-------|-------|
| 32 | **Cursor tooltip readout** — hover displays per-series value | M | crosshair exists; readout panel is missing |
| 33 | **Legend drag-to-reorder** | M | data-driven today |
| 34 | **Heatmap / Scatter chart classes** | L | derive from `ChartView` |
| 35 | **KPI Tile demo configuration panel** — toggle compact / expanded / sparkline-only | M | reuses existing `nfui::KpiTile` |
| 36 | **PDF export** via WIC | M | PNG/BMP export shipped in CP39 |
| 37 | **Print / Print Preview** | L | Win32 `PrintDlg` + `PrintDlgEx` |
| 38 | **Animated cursor crosshair** | S | `Animation.hpp` exists |
| 39 | **Multi-axis (secondary Y)** | L | non-trivial layout rework |

---

## Platform integration

| # | Feature | Scope | Notes |
|---|---------|-------|-------|
| 40 | **Live dark/light/high-contrast toggle** at runtime | S | sample apps already accept `--theme` arg; needs an in-app UI control |
| 41 | **Theme editor** — live edit + persist `ThemePalette` tokens | M | depends on #8 Color Picker |
| 42 | **Localization / runtime language switch** | M | `ResourceContext` already supports language selection; demo wiring missing |
| 43 | **Tray icon + balloon notification** | S | shell API |
| 44 | **Global Hotkey** (`RegisterHotKey`) | S | |
| 45 | **UWP toast notification** | M | |
| 46 | **Shell file dialogs** (`IFileOpenDialog` / `IFileSaveDialog`) — modern look | M | CP35 already implemented PrintWindow + WIC for capture |

---

## Code-base quality and tooling

| # | Feature | Scope | Notes |
|---|---------|-------|-------|
| 47 | **Architectural dependency check** — already CI; extend to detect layer-drift | S | `tools/verify_boundaries.ps1` |
| 48 | **Per-Monitor DPI live-migration test harness** — drag window across monitors, assert layout fidelity | M | exposes real-world DPI bugs |
| 49 | **Accessibility coverage** — UI Automation provider for Narrator + integration tests | L | large surface; consider MSAA bridge first |
| 50 | **UI Automation integration tests** — FlaUI or WinAppDriver against Workbench and SettingsDemo | L | Phase 7 deferred this |
| 51 | **Performance benchmark target** — fixed-workload paint + dispatch + layout cycle, regression-tracked | M | |
| 52 | **Resource leak detector** — already partial (`OwnedHwnd`, `WM_NCDESTROY`); extend to OLE/GDI objects in long-running samples | M | |
| 53 | **First-party consumer project** — Phase 9 deliverable | L | `samples/` extensions can serve, but a distinct "consumer" exe outside `samples/` strengthens the contract |
| 54 | **CONTRIBUTING.md / SECURITY.md / SUPPORT.md / issue templates** — Phase 9 deliverable | S | |
| 55 | **Release checklist doc** | S | Phase 9 |

---

## Suggested CP42+ slicing

A pragmatic order: small-with-clear-ROI first, then self-contained medium
features, then platform or architecture projects last.

| Slice | Items | Why |
|-------|-------|-----|
| CP42 — Polish foundation | 12 (self-paint Tooltip), 13 (self-paint ComboBox) | **shipped** — ComboBox in CP5/CP-A2, Tooltip in CP42 (2026-08) |
| CP43 — Inspector done right | 1 (Property Grid) | replaces the Workbench placeholder; replaces "Inspector — select an item" copy |
| CP44 — Edit experience | 21 (file drop), 27 (Find), 22 (inline rename) | builds editing affordance for tree + files |
| CP45 — Live configuration | 8 (Color Picker), 41 (theme editor) | lets users tune palette at runtime |
| CP46 — Notification surface | 14 (snackbar), 43 (tray), 44 (hotkey), 45 (UWP toast) | platform glue |
| CP47 — Undo + Command Palette | 23, 24 | turns Workbench into a believable IDE shell |
| CP48 — Charts round-out | 32, 33, 38 | completes hover readouts |
| CP49 — Pane system | 5, 6, 4 | Dock + Floating + layout restore as one workstream |
| CP50 — Form + validation | 26, 11, 9, 10 | SettingsDemo grows into a real product shell |
| CP51 — Quality | 47, 48, 50, 51, 52 | before claiming V1 acceptance |
| CP52 — Phase 9 docs | 53, 54, 55 | closes community-track deliverables |

---

## Cross-references

- Existing CP40 dashboard polish (KPI tiles, dashboard chrome, chart AA,
  cursor crosshair, value readouts): see git history `polish(cp40): …`
  and memory `feedback-self-painted-controls-on-dark`,
  `feedback-paint-parent-body-bg`, `feedback-chart-kind-mismatch`.
- Memory feedback rules persist for individual edits; this file carries
  the larger picture.
- V1 non-goals (Ribbon, full docking, visual designer, complete Property
  Grid, complete Data Grid, plugin system, ARM64, Direct2D/DirectWrite)
  still apply; revisit `CLAUDE.md` before planning any of them.

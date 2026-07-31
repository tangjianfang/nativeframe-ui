# Section B Follow-Ups Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve every deferred item in `docs/FOLLOW_UP.md` (A–F). Closes the chrome polish gaps, palette tokens, scrollbar forwarding, and per-pixel audit gate from CP-A4 follow-ups.

**Architecture:** Six CPs grouped into three families:
- **Chrome polish (CP-B15):** MF_OWNERDRAW per-item menu chrome across every demo that hosts a menu.
- **Design system (CP-B16, CP-B17):** `palette.divider` + `palette.surface_variant` tokens, plus per-control migration.
- **Scrollbar unification (CP-B18, CP-B19):** standalone wrapper integration + `NM_CUSTOMDRAW` forwarding contract.
- **QA (CP-B20):** real per-pixel diff-vs-baseline gate scorer.

**Tech Stack:** Same as the library — Win32 C++20, MSVC v143, x64, Unicode, `/MD`/`/MDd`, Per-Monitor DPI V2, CMake 3.25+ presets. PowerShell for audit + gate scripts.

## Global Constraints

These are binding across all six CPs. Every implementer MUST read this section.

- **No MFC / ATL / WTL / BCG includes anywhere.** Boundary check stays green.
- **No breaking changes to public `nfui::*` API.** New tokens, new wrapper fields, new hooks are allowed. No existing signature changes without a separate design review.
- **No C++ exceptions across `WindowProc`, `DialogProc`, subclass proc, dispatcher, system callback boundaries.** Paint paths are `noexcept`; logging catches and swallows.
- **Only the UI thread creates / destroys / moves / repaints / themes windows.** No callbacks on background threads.
- **Per-Monitor DPI Awareness V2.** Logical units via `DpiScale`. Device pixels only in paint.
- **FrameStyle overrides** for per-demo chrome tints — never edit the framework default palette.
- **Theme propagation.** Every demo that surfaces a theme switch routes through `ThemeBroker::instance().set_theme(...)` so `WM_THEMECHANGED` broadcasts reach every chrome HWND.
- **Audit captures.** Each CP regenerates 42 PNGs and passes the gate (CP-B20 upgrades the gate to per-pixel; earlier CPs keep the byte-size placeholder).
- **Architecture / boundary / smoke gates stay green.** Any CP that breaks an invariant fails the task.
- **Commit message format:** `feat(cpB<N>): <scope>` / `fix(cpB<N>): <specific fix>`.
- **Branch:** all work on `feat/section-b-followups`. Merge to `main` after every CP is reviewed clean.

## Validation Common to All Tasks

Every CP closes with the same validation block. The implementer MUST run each command and report output:

```bash
cmake --build --preset x64-debug 2>&1 | tail -5
ctest --preset x64-debug 2>&1 | tail -8
pwsh tools/verify_architecture.ps1 -Root . 2>&1 | tail -3
pwsh tools/verify_boundaries.ps1 -Root . 2>&1 | tail -3
pwsh tools/visual_audit/run_audit.ps1 2>&1 | tail -5
pwsh tools/visual_audit/gate.ps1 2>&1 | tail -5
```

Expected: build clean, 7/7 ctest pass, architecture + boundary + audit gate all green.

---

### Task B15: Per-item menu chrome (FOLLOW_UP A)

**Files:**
- Modify: `include/nfui/Menu.hpp`, `src/controls/Menu.cpp`, `src/theme/MenuOwnerDraw.*` (new)
- Modify: every demo that hosts a menu — `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`, `samples/NativeFrameUIDialogTour/...`, `samples/NativeFrameUIResourceGallery/...`, `samples/NativeFrameUIThemeDemo/...`, `samples/NativeFrameUISettingsDemo/...`
- Test: `.superpowers/sdd/task-B15-report.md`
- Commit: `feat(cpB15): per-item menu chrome — MF_OWNERDRAW themed items`

**Scope:**
- Register `MF_OWNERDRAW` items on every `Menu` instance with `IconKind` + label + accelerator item data.
- Implement `WM_MEASUREITEM` (28 px height, 16 px icon gutter, accelerator gutter) and `WM_DRAWITEM` (rounded selection fill, icon, label, right-aligned accelerator, separator).
- Honour `palette.accent` on selection, `palette.divider` (or `border` fallback until item C) for separators, `palette.text_secondary` for accelerators.
- Update every demo that previously relied on the system default to register items through the new wrapper path.

**Acceptance:** Visual-audit PNGs for every demo that hosts a menu show themed items in all three themes. ctest + architecture + boundary gates stay green.

---

### Task B16: `palette.divider` token (FOLLOW_UP C)

**Files:**
- Modify: `include/nfui/Theme.hpp` (`ThemePalette`), `src/theme/Theme.cpp`
- Modify: every self-painted surface that draws a separator — `src/controls/StatusBar.cpp`, `src/controls/Scrollbar.cpp`, `src/controls/Menu.cpp`
- Test: `tests/test_theme.cpp` (extend existing tests)
- Commit: `feat(cpB16): ThemePalette::divider token — hairline separator colour`

**Scope:**
- Add `nfui::Color divider` to `ThemePalette`. Per-theme value distinct from `border`, with ≥ 1.3:1 luminance delta vs `surface`.
- Migrate separator paint in StatusBar / Scrollbar / Menu to read `palette.divider`.
- Extend `test_theme.cpp` with assertions: `divider.rgb != 0`, `divider != border`, `divider != surface`, contrast passes.

**Acceptance:** Build clean, `test_theme.cpp` passes, visual-audit PNGs show separators at the new weight.

---

### Task B17: `palette.surface_variant` token (FOLLOW_UP D)

**Files:**
- Modify: `include/nfui/Theme.hpp`, `src/theme/Theme.cpp`
- Modify: `src/controls/ListView.cpp`, `src/controls/TreeView.cpp`, `src/controls/TabControl.cpp`
- Test: `tests/test_theme.cpp`
- Commit: `feat(cpB17): ThemePalette::surface_variant — elevated / nested surface`

**Scope:**
- Add `nfui::Color surface_variant` to `ThemePalette`. Distinct from `surface` and `surface_hover` per theme.
- Migrate ListView alternating-row backgrounds, TabControl tab fills, card-style surfaces to read `surface_variant`.
- Extend `test_theme.cpp` with assertions: `surface_variant != surface`, `surface_variant != surface_hover`.

**Acceptance:** Build clean, tests pass, audit PNGs show the new container tones.

---

### Task B18: Standalone Scrollbar integration (FOLLOW_UP E)

**Files:**
- Modify: every wrapper that still leaks native scrollbar chrome in audit PNGs — likely ListView / TreeView / Edit subclass procs, Scrollbar wrapper integration paths
- Audit script: `tools/visual_audit/run_audit.ps1` (regenerate after edits; spot-check no native chrome)
- Commit: `feat(cpB18): standalone Scrollbar — close residual native chrome across demos`

**Scope:**
- Audit each of the 42 PNGs for native scrollbar chrome.
- For any demo that still shows native chrome, integrate the standalone `Scrollbar` wrapper (either directly or via `NM_CUSTOMDRAW` forwarding per item F).
- Document the integration pattern in `docs/CHROME_QA.md`.

**Acceptance:** Zero native scrollbar chrome visible in any visual-audit PNG.

---

### Task B19: ListView / TreeView / Edit scrollbar via `NM_CUSTOMDRAW` (FOLLOW_UP F)

**Files:**
- Modify: `src/controls/ListView.cpp`, `src/controls/TreeView.cpp`, `src/controls/Edit.cpp`
- Create / Modify: `docs/CHROME_QA.md` (forwarding contract)
- Commit: `feat(cpB19): NM_CUSTOMDRAW scrollbar forwarding — unified chrome`

**Scope:**
- Forward `NM_CUSTOMDRAW` from each wrapped control's subclass proc into the shared `Scrollbar` paint pipeline.
- Document the forwarding contract: `dwDrawStage` values accepted, `NMHDR` fields read, thumb rect communication.
- Verify themed scrollbars on every hosting control in all three themes.

**Acceptance:** ListView / TreeView / Edit scrollbars paint via shared `Scrollbar` through `NM_CUSTOMDRAW` forwarding. Contract documented. Audit PNGs clean.

---

### Task B20: Real per-pixel diff-vs-baseline gate scorer (FOLLOW_UP B)

**Files:**
- Create: `tools/visual_audit/baseline/` (42 PNGs)
- Modify: `tools/visual_audit/gate.ps1` (per-pixel diff scorer)
- Create: `tests/visual_audit_gate_tests.ps1` (smoke fixtures)
- Commit: `feat(cpB20): per-pixel diff-vs-baseline audit gate — closes CP-A4 placeholder`

**Scope:**
- Snapshot the current 42 PNGs as the new baseline (post-B19).
- Rewrite the gate to compute per-pixel RGB diff with configurable tolerance. Per-frame tolerance: per-pixel delta > 5 %; project-wide average delta > 2 % fails.
- Add smoke tests:
  - `clean`: capture vs baseline identical — gate PASS.
  - `regression`: capture vs baseline with a programmatic injected change — gate FAIL with delta reported.
  - `missing-baseline`: capture without baseline entry — gate FAIL with explicit error.

**Acceptance:** Gate computes per-pixel RGB diff against baseline, fails on regression or missing baseline, smoke tests cover all three fixtures.

---

### Task B21: Merge, push, tag

After every CP is reviewed clean:
1. Run the full validation block once on the branch.
2. Fast-forward `main` to `feat/section-b-followups`.
3. Push `main` and tag `v1.1.0` (minor bump — palette tokens + chrome work).
4. Update `docs/RELEASE_CHECKLIST.md` and `docs/source/段_B.md` (if it exists) with the new release notes.

Final report at `.superpowers/sdd/task-B21-report.md`.
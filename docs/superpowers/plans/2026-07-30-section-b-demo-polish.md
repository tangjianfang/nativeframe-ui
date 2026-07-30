# Section B — All Demos Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish all 14 sample demos from baseline avg 53.5/100 to avg ≥ 90/100 / min ≥ 85/100 across all three themes (light / dark / hc). Eliminate native gray chrome islands, layout truncation, debug strings, and theme-propagation gaps.

**Architecture:** Per-demo polish builds on the CP-A1..A4 framework foundation (design tokens, ThemeBroker, state palette, self-painted Edit/ComboBox/CheckBox/RadioButton/ListView/TreeView/TabControl/StatusBar/Menu/Scrollbar). Each demo's task targets the demo-specific layout, content density, and UX rather than framework chrome. FrameStyle overrides are used heavily to tint per-demo chrome without altering the framework.

**Tech Stack:** NativeFrameUI (Win32 C++20, per-monitor DPI V2), MSVC v143, x64, Unicode, `/MD`|`/MDd`, VS 2022, Windows SDK 10.0.22621+. CMake 3.25+ presets. PrintWindow + WIC PNG for visual audit. The visual-audit gate at `tools/visual_audit/gate.ps1` is a byte-size placeholder; full per-pixel scoring is a deferred item (docs/FOLLOW_UP.md B).

## Global Constraints

These are binding across all 14 demo polish tasks. Every per-demo implementer MUST read this section.

- **No MFC / ATL / WTL / BCG includes anywhere.** `#include <afx*>`, `<atl*>`, `<wtl*>`, and any `BCG*` token are forbidden. Boundary check (`tools/verify_boundaries.ps1`) fails the build if any leak.
- **No breaking changes to public `nfui::*` API.** FrameStyle overrides and per-control hooks are the public extension surface; new helpers may be added but no existing signature may change without a separate design review.
- **No C++ exceptions across WindowProc / DialogProc / subclass proc / dispatcher / system callback boundaries.** Paint paths are `noexcept`; logging catches and swallows.
- **Only the UI thread creates / destroys / moves / repaints / themes windows.** Background work goes through `nfui::dispatcher::post(...)`. Never invoke user callbacks while holding internal locks.
- **Per-Monitor DPI Awareness V2.** Logical units (the `DpiScale` helper) in layout; device pixels only in paint. `LONG_PTR` / `SetWindowLongPtr` / `GetWindowLongPtr` for pointer data — never `LONG` / `SetWindowLong`.
- **FrameStyle override pattern.** When a demo wants a non-default chrome tint, set `style.background`, `style.foreground`, `style.border`, etc. via `Control::set_style(...)` — do not edit the framework's default palette. The framework palette is a stable contract.
- **Theme propagation.** Every demo that surfaces a theme switch (light/dark/HC) MUST route the menu / button click through `ThemeBroker::instance().set_theme(...)` so the cross-window `WM_THEMECHANGED` broadcast repaints every chrome HWND on the same message-loop turn. The `--theme X` argv parser (when added) posts `ID_THEME_X` to the main window which delegates to `ThemeBroker::set_theme`.
- **Audit captures.** `pwsh tools/visual_audit/run_audit.ps1` regenerates the 42 PNGs (14 demos × 3 themes). Each per-demo task must re-capture, then verify the audit PNG bytes / pixel content match the expected post-polish state. The gate at `tools/visual_audit/gate.ps1` is a byte-size placeholder — full per-pixel diff-vs-baseline is deferred to `docs/FOLLOW_UP.md` item B.
- **No backsliding.** The architecture and boundary checks (`tools/verify_architecture.ps1`, `tools/verify_boundaries.ps1`) MUST stay green. The smoke test (`ctest --preset x64-debug -R NativeFrameUISmokeTest`) MUST stay green. Any per-demo polish that breaks a framework invariant fails the task.
- **Commit message format:** `feat(cpB<N>): <demo name> polish — <scope>` or `fix(cpB<N>): <demo name> — <specific fix>`. One demo per CP.
- **Branch:** all 段 B work lands on `feat/all-demos-polish-foundation`. Final merge to `main` and push happen in Task 14 after every CP is reviewed clean.

## Per-Demo Audit Baseline (from `docs/VISUAL_AUDIT/AUDIT.md`)

| Demo | Pre-CP-A Score | Key Issues |
|---|---:|---|
| Workbench | 38 | Edit/Tree/Tab/ListView all gray; inspector empty; theme only flips title bar |
| DialogTour | 42 | 3 buttons + debug string; traditional Win32 dialog; theme doesn't propagate |
| ComponentGallery | 45 | 7 control classes leak native gray; bottom truncated; no hover/focus/disabled matrix |
| ThemeDemo | 48 | White native islands in dark/HC; title / ListView / TreeView truncated |
| SettingsDemo | 54 | Form chrome native gray; empty bottom; theme preference doesn't switch shell |
| ResourceGallery | 61 | Menu / StatusBar native; debug checklist; preview is empty color block |
| Showcase | 68 | Brand/inspector truncated; dark/HC content unchanged; KPI card has raised shadow |
| DarkStudio | 72 | Orange canvas dominates; light/HC content absent; status bar native |
| Controls | (new in CP-A) | First-audit baseline — to be measured post-CP-A |
| ControlsPlayground | (new in CP-A) | First-audit baseline — to be measured post-CP-A |
| Charts | (new in CP-A) | First-audit baseline — to be measured post-CP-A |
| ChartsInteractive | (new in CP-A) | First-audit baseline — to be measured post-CP-A |
| Minimal | (new in CP-A) | First-audit baseline — to be measured post-CP-A |
| IconGallery | (new in CP-A) | First-audit baseline — to be measured post-CP-A |

The CP-A framework foundation is expected to lift most demos by 10–25 points via self-painted Edit/ComboBox/CheckBox/RadioButton + container controls + chrome. 段 B targets the remaining demo-specific polish.

## Validation Common to All Tasks

Every per-demo CP closes with the same validation block. The implementer MUST run each command and report output:

```bash
cmake --build --preset x64-debug 2>&1 | tail -5
ctest --preset x64-debug 2>&1 | tail -8
pwsh tools/verify_architecture.ps1 -Root . 2>&1 | tail -3
pwsh tools/verify_boundaries.ps1 -Root . 2>&1 | tail -3
pwsh tools/visual_audit/run_audit.ps1 2>&1 | tail -5
pwsh tools/visual_audit/gate.ps1 2>&1 | tail -5
```

Expected: build clean, 7/7 ctest pass, architecture + boundary + visual-audit gate all green. Per-demo report file (at `.superpowers/sdd/task-B<N>-report.md`) records the actual outputs and per-demo audit PNG delta.

---

### Task B1: Workbench polish

**Files:**
- Modify: `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp` (and any sibling `.hpp` / View files in that directory)
- Possibly create: 1–2 new view / inspector files for content density
- Test: `.superpowers/sdd/task-B1-report.md` (audit delta + before/after PNG notes)
- Commit: `feat(cpB1): Workbench polish — three-pane layout + inspector content + menu chrome`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIWorkbench"):**
- Inspector pane is empty — fill with real content (file metadata table / recent activity card / context-sensitive info tied to the selected row).
- Layout density is broken (center + right ~80% empty, splitter + borders heavy) — tighten vertical rhythm; remove the unused right pane border if the inspector content fits a narrower rail.
- Menu chrome is system default — `Menu::set_palette(theme_palette(current_mode))` in the create() path so File/Help follow the theme.
- Tree / Tab / ListView self-paint landed in CP-A3 — verify the captures show the new chrome and not white islands; if any residual gray leaks, identify the offending branch and tighten.
- Search Edit self-paint landed in CP-A2 — verify border / hover / focus read cleanly in all three themes.

**Acceptance:**
- Inspector pane has visible content (no empty card / no large blank rectangle).
- Splitter + section spacing follows the 8 / 12 / 16 / 24 design tokens.
- File / Help menu chrome matches the rest of the shell.
- All three theme captures show consistent chrome; no white islands in dark / HC.
- ctest + visual-audit gate green.
- Report `.superpowers/sdd/task-B1-report.md` records audit PNG byte deltas + before / after qualitative notes.

### Task B2: DialogTour polish

**Files:**
- Modify: `samples/NativeFrameUIDialogTour/NativeFrameUIDialogTour.cpp`
- Possibly create: small header file documenting the tour-stage model
- Test: `.superpowers/sdd/task-B2-report.md`
- Commit: `feat(cpB2): DialogTour polish — product tour model + theme propagation`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIDialogTour"):**
- Replace the 3-button stack + debug-string model with a proper product tour: title, description, primary / secondary actions, status card.
- Remove the `about=unset prefs_open=no last=<none>` debug string; replace with a labelled status card (e.g. "Last action: <human-readable>").
- Establish primary / secondary button hierarchy (primary = filled accent, secondary = ghost).
- Wire `--theme X` argv to `ThemeBroker::set_theme(X)` and verify the customer-area repaints (CP-A1 broker is in place; this is just plumbing + verification).
- Use `nfui::design::spacing_*` and `control_height_*` constants for vertical rhythm.

**Acceptance:**
- No `about=` / `prefs_open=` / `last=<none>` strings visible in any capture.
- Primary action button is visually distinct (accent fill, larger weight) from secondary (ghost / outline).
- `--theme dark` and `--theme hc` flips actually change the customer area, not just the title bar.
- All three captures show consistent chrome with the rest of the framework.
- ctest + visual-audit gate green.

### Task B3: ComponentGallery polish

**Files:**
- Modify: `samples/NativeFrameUIComponentGallery/*.cpp` + any sibling view files
- Possibly create: 1–2 view sections (per-control hover/focus/disabled matrix)
- Test: `.superpowers/sdd/task-B2-report.md`
- Commit: `feat(cpB3): ComponentGallery polish — control state matrix + truncation fix`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIComponentGallery"):**
- For each of the 7 control classes (CheckBox, RadioButton, Edit, ComboBox, ListView, TreeView, TabControl), show a state matrix: rest / hover / pressed / focused / disabled / error across all three themes. (CP-A2/A3 chrome is in place; this task surfaces the matrix in the gallery rather than adding new chrome.)
- Fix the bottom truncation — the audit notes "Panel + Splitter content gets cut off" and "right scrollbar looks like traditional Win32".
- StatusBar tab relationship in the audit (`StatusBar label and actual TabControl position relationship even makes the semantics look misplaced`) — re-flow the layout so StatusBar lives at the bottom of the content area, not adjacent to the tabs.
- Re-flow vertical rhythm to the 8 / 12 / 16 / 24 design tokens; fix arbitrary widths.

**Acceptance:**
- Every control class shows a complete state matrix in all three themes (use small per-state samples side by side).
- No bottom truncation in any capture.
- StatusBar placement matches a product UI (bottom edge of the window, full width).
- ctest + visual-audit gate green.

### Task B4: ThemeDemo polish

**Files:**
- Modify: `samples/NativeFrameUIThemeDemo/NativeFrameUIThemeDemo.cpp`
- Test: `.superpowers/sdd/task-B4-report.md`
- Commit: `feat(cpB4): ThemeDemo polish — eliminate white islands + truncation fixes`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIThemeDemo"):**
- Eliminate every white native island in dark / HC: CheckBox, RadioButton, ListView header, TabControl strip, TreeView empty area. (CP-A2/A3 chrome should land most of these; audit post-CP-A and tighten the leaks.)
- Fix title truncation: the audit says the page header is "covered / cut off by the theme buttons" — re-flow so buttons sit in a header strip below or beside the title.
- Fix ListView row content + TreeView child item truncation (rows clipped by control height).
- Ensure the per-control state matrix from B3 also lives here as a single page that toggles between light / dark / HC for QA.

**Acceptance:**
- Zero white islands in dark or HC captures.
- Title + section labels fully visible.
- ListView rows + TreeView items render at full height (no clipping).
- ctest + visual-audit gate green.

### Task B5: SettingsDemo polish

**Files:**
- Modify: `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp`
- Test: `.superpowers/sdd/task-B5-report.md`
- Commit: `feat(cpB5): SettingsDemo polish — form spacing rhythm + instant theme shell switch`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUISettingsDemo"):**
- Form rhythm: 8 / 12 / 16 px spacing between label / input / help text. Input heights follow `control_height_md`.
- Edit / ComboBox / CheckBox chrome landed in CP-A2 — verify all six states (rest / hover / pressed / focused / disabled / error) read cleanly.
- Theme preference selector must immediately re-skin the entire shell — wire to `ThemeBroker::set_theme(X)` and verify all child HWNDs repaint.
- Tighten content max-width so ultra-wide windows don't make the form float.
- Add a help-text row beneath each field with semantic foreground token so the form reads with proper hierarchy.

**Acceptance:**
- Form vertical rhythm matches the design tokens (8 / 12 / 16 / 24).
- Each field shows label + input + help-text with consistent spacing.
- Theme preference selection visibly re-skins the entire shell within one frame.
- ctest + visual-audit gate green.

### Task B6: ResourceGallery polish

**Files:**
- Modify: `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp`
- Possibly create: 1 view file for the resource item card
- Test: `.superpowers/sdd/task-B6-report.md`
- Commit: `feat(cpB6): ResourceGallery polish — scan-friendly items + themed chrome`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIResourceGallery"):**
- Replace the debug checklist with a real asset list / grid: thumbnail (16×16 vector icon or a coloured swatch), name, status badge, action.
- Theme the menu + status bar (CP-A4 chrome is in place; verify the menu bar uses the palette and the status bar follows `palette.surface_variant`).
- Eliminate the giant empty card lower halves — fill with content density (small badges, status dots, captions).
- Add visible interaction feedback (hover state on items).

**Acceptance:**
- Each asset row reads as a scannable item (thumbnail + name + status + action).
- Menu + status bar chrome matches the shell in all three themes.
- No giant empty rectangles in any capture.
- ctest + visual-audit gate green.

### Task B7: ControlsPlayground polish

**Files:**
- Modify: `samples/NativeFrameUIControlsPlayground/NativeFrameUIControlsPlayground.cpp`
- Test: `.superpowers/sdd/task-B7-report.md`
- Commit: `feat(cpB7): ControlsPlayground polish — three-theme coverage + spacing rhythm`

**Scope:**
- The Playground is the second gallery (alongside ComponentGallery). Mirror B3's state-matrix + spacing work but with the Playground's interactive angle (try each control, see the live state).
- Ensure all three theme captures show meaningful differences — verify post-CP-A and tighten any remaining gray.
- Apply the 8 / 12 / 16 design tokens.

**Acceptance:**
- All three themes render with consistent chrome + meaningful content differences.
- Spacing rhythm matches design tokens.
- ctest + visual-audit gate green.

### Task B8: Charts polish

**Files:**
- Modify: `samples/NativeFrameUICharts/NativeFrameUICharts.cpp` + sibling chart view files
- Test: `.superpowers/sdd/task-B8-report.md`
- Commit: `feat(cpB8): Charts polish — three-theme + legend polish (builds on cp40/42)`

**Scope (from spec + audit):**
- CP40 / CP42 already added axis titles, mouse readout, comparison toggle, and edited write-back. 段 B targets: legend polish (positioning, contrast against axis titles), three-theme coverage (verify dark / HC charts read cleanly without dark-area white text bleed), and any remaining native chrome on the chart container.
- Verify the chart container's chrome (any TabControl / ListView inside the demo) follows the theme.
- Add a small caption strip below each chart with the dataset name (so legend isn't the only naming surface).

**Acceptance:**
- Legend reads cleanly in all three themes; axis titles + tick labels don't bleed over data.
- Caption strip below each chart identifies the dataset.
- ctest + visual-audit gate green.

### Task B9: Showcase polish

**Files:**
- Modify: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp` + sibling View files
- Test: `.superpowers/sdd/task-B9-report.md`
- Commit: `feat(cpB9): Showcase polish — truncation fixes + brand button states + dark/HC`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIShowcase"):**
- Fix brand title truncation (`NativeFrame U`) and subtitle truncation — re-flow the hero header.
- Fix inspector card text truncation at the bottom — either widen the inspector rail or shrink the caption line count to fit.
- Theme-switch button (currently reads "Switch to dark" in every capture) must actually flip the shell + persist; wire to `ThemeBroker::set_theme(X)` and verify.
- Remove the raised shadow on the first KPI card; unify card elevation across the grid.
- Add real dark / HC content (currently the dark / HC captures are the same as light).

**Acceptance:**
- Brand title + subtitle fully visible.
- No inspector card text truncation.
- Theme switch button visibly re-skins the shell + the button label updates to "Switch to light".
- Card elevation uniform; raised shadow gone.
- ctest + visual-audit gate green.

### Task B10: DarkStudio polish

**Files:**
- Modify: `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp`
- Test: `.superpowers/sdd/task-B10-report.md`
- Commit: `feat(cpB10): DarkStudio polish — orange weight reduction + light/HC content`

**Scope (from `docs/VISUAL_AUDIT/AUDIT.md` "NativeFrameUIDarkStudio"):**
- The orange preview canvas dominates the layout — replace with a neutral `palette.surface` background that hosts the actual orange only as a focused preview tile (smaller) or a chip strip.
- Increase body text contrast (audit notes "low contrast grey text"; bump to `palette.text` / `palette.text_secondary` semantic foreground).
- Add real light / HC content (currently the captures show fixed dark with white / HC titles).
- Status bar / resize grip polish landed in CP-A4 — verify post-CP-A.

**Acceptance:**
- Layout reads as "neutral dark dashboard with restrained accent", not "orange wall".
- Body text follows semantic foreground tokens.
- Light and HC captures show meaningful content differences.
- ctest + visual-audit gate green.

### Task B11: ChartsInteractive polish

**Files:**
- Modify: `samples/NativeFrameUIChartsInteractive/NativeFrameUIChartsInteractive.cpp`
- Test: `.superpowers/sdd/task-B11-report.md`
- Commit: `feat(cpB11): ChartsInteractive polish — three-theme + interactive controls polish`

**Scope:**
- Mirrors B8 with the interactive angle (live data updates, hover tooltips, zoom, etc.).
- Polish the interactive controls (sliders / buttons / scrollbars inside the demo) so they pick up the CP-A chrome.
- Three-theme coverage verification.

**Acceptance:**
- Interactive controls read cleanly in all three themes.
- Tooltip / readout chrome matches the shell.
- ctest + visual-audit gate green.

### Task B12: IconGallery polish

**Files:**
- Modify: `samples/NativeFrameUIIconGallery/NativeFrameUIIconGallery.cpp`
- Test: `.superpowers/sdd/task-B12-report.md`
- Commit: `feat(cpB12): IconGallery polish — icon system coverage + grid spacing`

**Scope:**
- Verify the icon system (vector icons in `nfui::VectorIcon`) covers the gallery's full set. If gaps exist, add them following the existing pattern.
- Grid spacing follows the 8 / 16 / 24 design tokens.
- Each tile shows icon + name + (optional) category badge.
- Three-theme coverage (icons recolor where needed; verify dark / HC reads cleanly).

**Acceptance:**
- Every displayed icon is named + categorised.
- Grid spacing matches design tokens.
- Three-theme coverage verified.
- ctest + visual-audit gate green.

### Task B13: Minimal polish

**Files:**
- Modify: `samples/NativeFrameUIMinimal/NativeFrameUIMinimal.cpp`
- Test: `.superpowers/sdd/task-B13-report.md`
- Commit: `feat(cpB13): Minimal polish — framework self-check + three-theme coverage`

**Scope:**
- Minimal is intentionally small — it exists to verify the framework is healthy. 段 B ensures it actually exercises all CP-A chrome: every self-painted control class appears at least once, all three themes flip cleanly, the visual-audit captures show meaningful differences across themes, and the smoke test + boundary / architecture gates all stay green.
- If any framework feature isn't reachable from Minimal, add a minimal demo of it (one Edit, one ComboBox, one CheckBox, one RadioButton, one ListView, one TreeView, one TabControl, one StatusBar — even if redundant with other demos, it's the framework health check).

**Acceptance:**
- Every CP-A control class appears at least once.
- All three themes render with meaningful differences.
- ctest + visual-audit gate + architecture + boundary all green.

---

### Task B14: Final merge to main + push

**Files:**
- Modify: none (git operation only)
- Test: full pre-merge validation
- Commit: merge commit

**Steps:**
1. Confirm working tree on `feat/all-demos-polish-foundation` is clean (`git status`).
2. Run the full validation block:
   - `cmake --build --preset x64-debug 2>&1 | tail -5` — clean
   - `ctest --preset x64-debug 2>&1 | tail -10` — 7/7 pass
   - `pwsh tools/verify_architecture.ps1 -Root .` — green
   - `pwsh tools/verify_boundaries.ps1 -Root .` — green
   - `pwsh tools/visual_audit/run_audit.ps1 2>&1 | tail -5` — 42 PNGs captured
   - `pwsh tools/visual_audit/gate.ps1 2>&1 | tail -10` — green (with `::warning` for placeholder)
3. Final whole-branch review via `superpowers:requesting-code-review` over the full merge-base to HEAD range; fix any Important / Critical findings before merge.
4. `git checkout main` (or fetch + checkout if local main is stale).
5. `git merge --no-ff feat/all-demos-polish-foundation -m "feat: all-demos polish foundation + 13 demo polishes"`.
6. `git push origin main` (user has explicitly requested push).
7. Tag the release: `git tag v1.0.0 -m "All demos polished — avg ≥ 90 / min ≥ 85 across 14 demos × 3 themes"`; `git push origin v1.0.0`.

**Acceptance:**
- `main` contains every 段 A + 段 B commit.
- `git log --oneline -1` shows the merge commit on `main`.
- `git push origin main` exits 0; remote `main` matches local `main`.
- Tag `v1.0.0` exists on origin.

---

## Execution Notes

- **Per-task scope discipline:** each demo's task is sized so a fresh subagent can complete it in one review cycle. If the per-demo work exceeds a single task (e.g. ComponentGallery needs both a state matrix AND a layout rewrite AND a tab relationship fix), split into B3a / B3b before dispatching.
- **Reviewer calibration:** each per-demo task's reviewer checks (a) the audit issues listed in the task scope are addressed, (b) no other demo regressed, (c) ctest + gates green, (d) the per-demo PNG captures read cleanly in all three themes. Reviewer reports at `.superpowers/sdd/task-B<N>-review.md`.
- **Cross-demo interactions:** most demos are independent, but the ThemeBroker singleton (ThemeBroker.hpp:33-38) couples theme state across the process. Per-demo polish that adds `--theme` argv handling must not race with a sibling demo's theme switch. The convention is one theme switch per demo at create time (CP-A1 broker handles the broadcast); runtime switching is one demo at a time.
- **Real per-pixel diff gate:** still deferred (docs/FOLLOW_UP.md B). The byte-size placeholder gate is sufficient to catch missing / near-empty captures, which is the regression class the chrome pass is scoped to prevent. Per-demo PNG byte deltas (in each task report) substitute for full per-pixel diff until the real scorer lands.
- **If a per-demo CP cannot reach ≥ 85 by audit score after one review cycle,** split it (B<N>a / B<N>b) rather than expanding scope; each demo must individually close before the next launches.
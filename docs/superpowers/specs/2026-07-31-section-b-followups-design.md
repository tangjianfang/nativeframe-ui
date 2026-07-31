# Section B Follow-Ups Design — Closed Follow-Up Items A–F

> Companion spec to `docs/superpowers/specs/2026-07-30-all-demos-polish-design.md` and the implementation plan `docs/superpowers/plans/2026-07-31-section-b-followups.md`.

**Goal:** Resolve every deferred item tracked in `docs/FOLLOW_UP.md` (items A through F) so the next stable release can drop the "honest placeholder" notes from the chrome polish, palette, scrollbar, and visual-audit gate.

**Architecture:** All six items sit squarely inside the public `nfui` API — none require a new module. Items A and F extend the existing `Menu` and Scrollbar wrappers with owner-draw + `NM_CUSTOMDRAW` forwarding; items C and D extend `ThemePalette` with two new tokens (`divider`, `surface_variant`) read by every self-painted surface that previously leaned on `border` / `surface`; item E is a final-pass sweep that wires the standalone Scrollbar wrapper into any control that still leaks native scrollbar chrome; item B upgrades the audit gate from byte-size heuristic to per-pixel RGB diff with a checked-in baseline.

**Tech Stack:** Same as the existing library — Win32 C++20, MSVC v143, x64, Unicode, `/MD`/`/MDd`, Per-Monitor DPI V2, CMake 3.25+ presets.

## Global Constraints

- No MFC / ATL / WTL / BCG includes anywhere. Boundary check stays green.
- No breaking changes to public `nfui::*` API. New tokens, new control hooks, and new frame-style fields may be added; no existing signature changes without a separate design review.
- Theme-aware at all layers — every new feature must render correctly in light / dark / high_contrast with palette-driven colour, no hard-coded RGB.
- Owner-draw contracts documented in `docs/ARCHITECTURE.md` (or a new `docs/CHROME_QA.md` if the chrome-rewrite deserves its own doc).
- Audit captures: every CP regenerates the 42 PNGs and passes the per-pixel gate (item B).
- Commit message format: `feat(cpB<N>): <scope>` for new work, `fix(cpB<N>): <specific fix>` for regression repair.
- Branch: all 段 B 续 work lands on `feat/section-b-followups`. Final merge to `main` and push happen after every CP is reviewed clean.

## Items

### A. Per-item menu chrome (MF_OWNERDRAW)

`Menu` currently paints its host HWND chrome (rounded popup frame, border, accent title) but items still draw with the legacy non-themed default. Each item needs:

- 28 px item height with 16x16 leading icon (or icon-only / text-only variants).
- Accent fill on selection + 1 px hairline border that matches the popup frame.
- Accelerator text aligned right, dimmed to `palette.text_secondary`.
- Drop shadow / rounded corners on the popup (already done in CP-A4 step 2 — verify and document).
- Separator bar at 1 px, palette `divider` (lands in item C; pending that, fall back to `border` until the token exists).

Mechanism: when the demo creates a menu, register `MF_OWNERDRAW` items with item data carrying `IconKind`, `label`, `accelerator`, and the active `ThemePalette`. The wrapper intercepts `WM_MEASUREITEM` (size), `WM_DRAWITEM` (paint), and `WM_MENUCHAR` so users can still type-ahead.

Scope: every demo that hosts a menu — `Workbench`, `DialogTour`, `ResourceGallery`, `ThemeDemo`, `SettingsDemo`. `Charts` and `ChartsInteractive` use buttons instead of menus — no change.

### B. Real per-pixel diff-vs-baseline gate scorer

Replace the byte-size placeholder in `tools/visual_audit/gate.ps1` with a per-pixel RGB diff scorer:

- `docs/VISUAL_AUDIT/baseline/` holds checked-in baseline PNGs (42 files; same names as captures).
- The gate computes per-pixel RGB diff for each capture against its baseline.
- Configurable per-frame tolerance and project-wide average tolerance (spec § 2: avg ≥ 70 / 100; we set avg tolerance ≤ 5 % RGB delta per pixel for "no significant regression").
- Smoke tests cover three fixtures: diff-clean (no diff), regression (intentional large change), missing-baseline.
- Gate result printed in same format as today: `PASS` / `FAIL` with line counts and bytes.

### C. `palette.divider` token

- New field on `ThemePalette`. Distinct from `border` per theme; passes a hairline contrast check (≥ 1.3:1 luminance delta vs the matching `surface`).
- Migrate StatusBar / Scrollbar / Menu separators off `border` onto `divider`.
- Add accessor + smoke test that asserts the three values are non-zero and distinct.

### D. `palette.surface_variant` token

- New field on `ThemePalette`. Distinct from `surface` and `surface_hover` per theme. Concept: "elevated / nested surface" for container controls.
- Migrate ListView row backgrounds (alternating rows), TabControl tab fills, card-style surfaces to read `surface_variant`.
- Add accessor + smoke test.

### E. Standalone Scrollbar integration

Item B from CP-A4 step 3 shipped the `Scrollbar` wrapper but did not wire it into every control. This pass:

- Audits every demo PNG for residual native scrollbar chrome.
- For any control still showing native chrome, replaces the integration with the shared `Scrollbar` wrapper (either directly or via `NM_CUSTOMDRAW` forwarding per item F).
- Closes the visual audit: zero native scrollbar chrome visible in any 42 PNG.

### F. ListView / TreeView / Edit scrollbar via `NM_CUSTOMDRAW`

Defines the forwarding contract that item F in CP-A4 step 3 deferred:

- `NM_CUSTOMDRAW` notifications from the wrapped control subclass proc are forwarded into the `Scrollbar` paint pipeline.
- The forwarding contract specifies:
  - which `dwDrawStage` values are accepted (`CDDS_POSTPAINT` for the track, `CDDS_ITEMPREPAINT` for the thumb).
  - which `NMHDR` fields the wrapper reads (`hwnd`, `dwDrawStage`, `rc`, `dwItemSpec`).
  - how the thumb rect is communicated (`NMCUSTOMDRAW::rc` after the wrapper measures).
- Implements forwarding for ListView (`LVN_CUSTOMDRAW` ↔ `NM_CUSTOMDRAW`), TreeView (`TVN_CUSTOMDRAW` ↔ `NM_CUSTOMDRAW`), and Edit (`NM_CUSTOMDRAW` direct).
- Documented in `docs/CHROME_QA.md`.

## Out of Scope

- Visual designer / Property Grid / Data Grid / Ribbon / docking / plugins / ARM64 / Direct2D / DirectWrite — per the V1 non-goals.
- Per-item menu chrome that requires changing menu resource templates — this is wrapper-level owner-draw, not resource-template-level.

## Acceptance per Item

| Item | Acceptance |
|---|---|
| A | Every demo PNG showing a menu renders themed items: rounded popup, accent selection, icon + text + accelerator layout, separator. Three themes. |
| B | Gate computes per-pixel RGB diff; reports PASS / FAIL; smoke tests cover clean / regression / missing-baseline. |
| C | `ThemePalette::divider` exists in three themes, distinct from `border`, ≥ 1.3:1 luminance delta vs `surface`. Self-painted chrome reads `divider`. |
| D | `ThemePalette::surface_variant` exists in three themes, distinct from `surface` / `surface_hover`. Container self-paint reads it. |
| E | Zero native scrollbar chrome visible in any visual-audit PNG. |
| F | ListView / TreeView / Edit scrollbars paint via shared `Scrollbar` through `NM_CUSTOMDRAW` forwarding. Forwarding contract documented in `docs/CHROME_QA.md`. |
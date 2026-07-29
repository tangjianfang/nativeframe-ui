# NativeFrameUI Follow-Up

Items deferred from CP-A3 / CP-A4 that need a tracked home so 段 B
launch does not silently skip them. Each entry: title, status, source,
why deferred, where it lands, acceptance.

## A. `MF_OWNERDRAW` per-item menu chrome

- **Status:** open
- **Source:** CP-A4 step 2 — only host-HWND `theme_disable_window_theme` shipped; per-item `WM_DRAWITEM` chrome (rounded corners / drop shadow / 28px item height / 16x16 icons / accelerators / selection bg / separators) was the visible gap.
- **Why deferred:** Step 2 closed the Win32 popup chrome; item-level owner-draw was scoped out to keep the step small. Popup items still draw with the legacy non-themed default.
- **Where it lands:** 段 B chrome polish sub-project (or a new CP-A5). Belongs alongside item F (item-level custom draw).
- **Acceptance:** Popup items render through `WM_DRAWITEM` / `WM_MEASUREITEM` with all six visual elements above, in all three themes. Visual-audit PNGs for every demo that hosts a menu show the new chrome.

## B. Real per-pixel diff-vs-baseline gate scorer

- **Status:** open
- **Source:** CP-A4 step 4 placeholder. `gate.ps1` ships as an honest byte-size heuristic, documented as such in its header.
- **Why deferred:** Producing and reviewing per-pixel baselines for 14 demos x 3 themes = 42 PNGs is a meaningful chunk of work that did not fit CP-A4. The heuristic catches the two regression classes CP-A4 was designed to fix (missing / near-empty capture), enough to gate CI for now.
- **Where it lands:** 段 B chrome QA sub-project. Builds a checked-in `docs/VISUAL_AUDIT/baseline/` and rewrites the gate to compute per-pixel RGB diff with a configurable tolerance.
- **Acceptance:** Gate computes per-pixel RGB diff against baseline, fails if any frame exceeds per-frame tolerance or average exceeds the project threshold (spec § 2: avg >= 70). Baselines reviewed and checked in. Smoke tests cover diff-clean / regression / missing-baseline fixtures.

## C. `palette.divider` token in `ThemePalette`

- **Status:** open
- **Source:** CP-A4 step 1. `ThemePalette` aliases divider onto `palette.border`.
- **Why deferred:** Borders need to read with weight; dividers are hairline separators that should sit just-barely-visible against the surface family. Reusing `border` gives an overly heavy line that fights the rest of CP-A4's chrome.
- **Where it lands:** 段 B palette polish. Follow the `border` / `surface` pattern: pick a value per theme that clears hairline contrast, add a public accessor, migrate StatusBar / Scrollbar / Menu separators off `border` onto `divider`.
- **Acceptance:** `ThemePalette::divider` exists in all three themes, distinct from `border`, clears a per-surface contrast check (>= 1.3:1 luminance delta vs. matching `surface`). Self-painted chrome reads from `divider`, not `border`. Visual-audit PNGs show separators at intended weight.

## D. `palette.surface_variant` token in `ThemePalette`

- **Status:** open
- **Source:** CP-A3 step 3. `ThemePalette` aliases `surface_variant` onto `palette.surface`.
- **Why deferred:** Container controls (ListView rows, TabControl tabs, card-style surfaces) need a value one step removed from `surface` so chrome polish can express elevation without hand-rolling per-control palettes.
- **Where it lands:** 段 B palette polish. New field on `ThemePalette`, populated per theme with a value distinct from `surface` and `surface_hover`. Migrate container self-paint to read `surface_variant` where the reading is "elevated / nested surface".
- **Acceptance:** `ThemePalette::surface_variant` exists in all three themes, distinct from `surface` and `surface_hover`, used by ListView / TreeView / TabControl self-paint. A grep for the new accessor shows real uses; no control regresses against the visual-audit baseline.

## E. Standalone Scrollbar integration (built but unused)

- **Status:** open
- **Source:** CP-A4 step 3 created a self-painted Scrollbar wrapper (`paint_thumb` / `paint_track` / `paint_arrows`) but did not wire it into any existing control.
- **Why deferred:** Step 3 closed the standalone chrome; the broader NM_CUSTOMDRAW rewrite of ListView / TreeView / Edit was its own sub-project that did not fit CP-A4 budget.
- **Where it lands:** 段 B chrome QA sub-project, resolved alongside item F.
- **Acceptance:** Scrollbar wrapper instantiated (directly or via NM_CUSTOMDRAW forwarding) by every wrapped control with a scrollbar. No native scrollbar chrome visible in any visual-audit PNG.

## F. ListView / TreeView / Edit scrollbar rewrite

- **Status:** open
- **Source:** CP-A4 step 3 deferred. `paint_thumb` from the new Scrollbar wrapper is meant to be reused via `NM_CUSTOMDRAW` forwarding per the brief — forwarding contract needs a 段 B design decision.
- **Why deferred:** The cleanest path is to forward `NM_CUSTOMDRAW` from each wrapped control's subclass proc into the Scrollbar paint pipeline. The forwarding contract (which `dwDrawStage`s, which `NMHDR` fields, how the thumb rect is communicated) was not settled in CP-A4.
- **Where it lands:** 段 B chrome QA sub-project. Resolves the forwarding contract, implements it for ListView / TreeView / Edit, unifies with item E.
- **Acceptance:** All three controls paint scrollbars via the shared Scrollbar wrapper through `NM_CUSTOMDRAW` forwarding. Forwarding contract documented in `docs/ARCHITECTURE.md` or a dedicated chrome doc. Visual-audit PNGs show themed scrollbars on every hosting control, in all three themes.

---
title: CP13 polish backlog stale-doc cleanup
date: 2026-07-23
tags: [polish, docs, audit, cp13]
severity: cosmetic
effort: trivial
status: resolved
---

# CP13 — stale polish backlog cleanup

Branch: `polish/CP13-stale-doc-cleanup`.

Audited the nine `docs/KNOWLEDGE/polish/2026-07-22-*.md` entries that were
still marked `status: open` and reconciled each with the current code. Eight
of nine were already addressed by the CP1–CP10 polish iterations; one is
explicitly deferred to V1.5+ per an earlier decision.

## Status table

| File | Old | New | Why |
|---|---|---|---|
| `2026-07-22-button-disabled-state-color.md` | open | **resolved** | CP3 + CP10B: `Button::on_paint` disabled branch now lightens border 12% toward surface (CP3) and adds HC-aware inverted face (CP10B). |
| `2026-07-22-charts-split-recommendation.md` | open | **resolved** | The doc itself was the recommendation: **DO-NOT-SPLIT**, ratified by the layered-architecture spec §2.1 line 129 and re-confirmed by CP10D's architecture check. Decision finalized; entry closed. |
| `2026-07-22-combo-box-dropdown-item-style.md` | open | **deferred** | Doc already records "Decision: option 1 — defer to V1.5+" in its Resolution section; the `status: open` line was inconsistent. ComboBox dropdown HWND is OS-internal `ComboLBox`; ~150 LOC of plumbing deferred. |
| `2026-07-22-dpi-wm-setfont.md` | open | **resolved** | CP2 added `WM_SETFONT` handler in `src/controls/Controls.cpp:143-153`; CP8 added FontCache `font_pt::ui` token + per-DPI invalidation. |
| `2026-07-22-listbox-row-hover-not-implemented.md` | open | **resolved** | Commit `52525ff` (predates CP1) implemented the feature; doc was never updated to reflect it. |
| `2026-07-22-nfui-dialog-extraction-deferred.md` | open | **resolved** | CP12 added `include/nfui/Dialog.hpp` + `src/core/Dialog.cpp` + `cmake/nfui_dialog.cmake` and the first consumer `NativeFrameUIDialogTour`. |
| `2026-07-22-sample-component-audit.md` | open | **resolved** | CP11 implemented the per-component link refactor as P1.8 (the audit's own prescribed next step). |
| `2026-07-22-splitter-drag-feedback.md` | open | **resolved** | CP7A added drag hit-line visual feedback to `Splitter`. |
| `2026-07-22-statusbar-theme-color.md` | open | **resolved** | CP1 replaced SB_SETBKCOLOR (unsupported in modern ComCtl32 v6) with a self-paint WM_PAINT that fills `palette.surface` via MemoryDC. |

## Net effect

- 8 of 9 stale `open` entries flipped to **resolved** with a Resolution
  paragraph that names the commit / CP / file-line that addressed the
  concern. Future readers can follow the related-link directly to the
  implementation.
- 1 entry flipped to **deferred** to make the deferral intent visible in
  the status field instead of buried in a Resolution paragraph.
- All 9 entries gained a `related:` frontmatter field pointing at the
  CP-doc that did the work, so the polish backlog and the polish iterations
  cross-link cleanly.

The polish backlog now contains zero `status: open` entries from the
2026-07-22 vintage. Any new open concern will stand out as fresh.

## Verification

- No source files modified; docs-only pass.
- `cmake --build --preset x64-debug` — clean (no source changes).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure` —
  **3/3** tests pass (SmokeTest + BoundaryCheck + ArchitectureCheck).
- Boundary check still green; the new `related:` fields are frontmatter
  metadata, not code.

## Lesson

A polish backlog that is not actively reconciled against the codebase turns
into a misleading signal. Future agents who grep for `status: open` expect
those entries to mean "real, actionable work" — when 8 of 9 are actually
"resolved but not yet flipped", the backlog overstates the project's debt
by an order of magnitude. The right time to update an entry's status is
the same commit that resolves it. This CP13 audit catches up the backlog;
CP1–CP10 should have done so at the time.
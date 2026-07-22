---
title: CP8A TabControl visual polish
date: 2026-07-23
tags: [tabcontrol, frame-types, padding, polish]
severity: minor
status: addressed
related:
  - 2026-07-23-cp3-component-states.md
  - 2026-07-23-cp5-frame-types.md
---

# CP8A TabControl visual polish

Scope: `TabControl` (header + source) and its presence in the three samples
that already use it (`NativeFrameUIWorkbench`, `NativeFrameUIThemeDemo`,
`NativeFrameUIComponentGallery`). One polish pass that follows up on the
CP3 / CP5A "TabControl stays on native ComCtl32 chrome" decision and
exposes one program-driven tuning knob (padding) without committing to
custom-draw work.

## Audit findings

| # | Audit question | Verdict | Action |
|---|----------------|---------|--------|
| 1 | Selected tab highlight colour / bottom indicator line | OK | None — ComCtl32 v6 themed chrome owns this. Custom-draw remains deferred per CP3. |
| 2 | Unselected tab hover feedback | OK | None — themed v6 tracks the hot tab automatically; no per-class state needed. |
| 3 | Tab spacing / padding around icon and label | Defect — no public hook today. Consumers could only get a non-default rhythm by sending TCM_SETPADDING themselves. | Added `TabControl::set_padding(int cx, int cy)` that forwards `TCM_SETPADDING`. |
| 4 | Focused vs normal tab border | OK | None — themed v6 draws the focus rectangle correctly under high-contrast; not a polish defect. |
| 5 | `on_palette_changed` hook correctness | OK | None — TabControl is pure native chrome; `Control::set_palette` already invalidates and the subclass proc chains `DefSubclassProc` for `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` so system-theme updates reach the native control automatically. |
| 6 | Visibility across samples | Partial — present in 3 of 10 samples (Workbench, ThemeDemo, ComponentGallery). The other 7 are scenario-focused and adding tabs would change their product story. | None — visibility is not a polish regression. Documented below. |

### Why the others stay native

The CP3 decision record
(`docs/KNOWLEDGE/polish/2026-07-23-cp3-component-states.md`) and CP5A
frame-types audit
(`docs/KNOWLEDGE/polish/2026-07-23-cp5-frame-types.md`) both explicitly
keep `TabControl` on ComCtl32 v6 themed chrome. Reasons recorded there:

- Native theming handles high-contrast, DPI, and per-monitor theme
  switching correctly. Owner-drawing the tab strip would re-implement
  focus rectangle, hot tracking, multi-monitor colour profiles, and
  accessibility roles.
- `FrameStyle::chrome_text` / `chrome_bg` are explicitly tagged as
  reserved for the future custom-draw hook; adding them now would bind
  a public API to a behaviour we have not spec'd.
- All three other "native chrome" controls in the same situation
  (`Tooltip`, `TreeView`, plus the parts of `StatusBar`/`ProgressBar`
  that own renderer-managed text) follow the same rule.

This pass keeps that decision. The only concession is exposing a knob
the OS control already supports — the spacing/padding that's the single
most-visible non-default shape lever.

## Code changes

### `src/controls/TabControl.cpp` + `include/nfui/Controls/TabControl.hpp`

A new `set_padding` method that forwards `TCM_SETPADDING` to the native
HWND. The API:

```cpp
[[nodiscard]] bool set_padding(int cx, int cy) noexcept;
```

- `cx` is horizontal padding per tab in *device pixels* (same unit the
  caller already feeds to `MoveWindow` on a DPI-aware parent). This
  matches every other layout helper in the framework — wrapper classes
  do not multiply by DPI themselves.
- `cy` is vertical padding per tab in device pixels.
- Negative values are clamped to zero (native v6 silently ignores them,
  but the API contract is friendlier with a documented clamp).
- `[[nodiscard]]` so callers notice the "no HWND yet" failure mode.
- Returns `false` only when `valid()` is false (before `create()`
  finishes). When the HWND exists, `TCM_SETPADDING` is documented as
  void-return; the API reports success once the message is delivered.

Implementation note: the message is `SendMessageW(hwnd(), TCM_SETPADDING, 0, MAKELPARAM(cx, cy))`.
Both `wc.tabcontrol.TCM_SETPADDING` and `MAKELPARAM` come from
`<commctrl.h>`, which is already a transitive dependency of the
control base.

### Sample wiring

All three current consumers wire the new API so the difference is
visible in every demo that shows tabs:

- `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp` — calls
  `tabs_.set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4))`
  right after `tabs_.create()`. Matches the dense 12-DIP gallery rhythm
  used elsewhere in this integration baseline.
- `samples/NativeFrameUIThemeDemo/NativeFrameUIThemeDemo.cpp` — same
  `(12, 4)` DPI-scaled call. ThemeDemo already switches Light / Dark /
  High Contrast; the wider padding tracks the larger 12-DIP gap that
  the toggle-row layout already uses.
- `samples/NativeFrameUIComponentGallery/NativeFrameUIComponentGallery.cpp` —
  same `(12, 4)` DPI-scaled call. The kitchen-sink gallery explicitly
  uses a 12-DIP row rhythm, so the new padding slots in without
  changing the section height.

Default padding from ComCtl32 v6 is approximately `(cx=6, cy=3)` device
pixels at 96 DPI. After scaling to `(12, 4)` logical units at 100 %
DPI, the Workbench tabs read visibly wider than the stock native
behaviour, which is the entire point of the polish pass.

## Sample visibility matrix

Confirmed by grepping `samples/**`:

| Sample | TabControl visible? | Notes |
|--------|---------------------|-------|
| `NativeFrameUIWorkbench` | yes — two tabs over the central list | Already tuned to `(12, 4)`. |
| `NativeFrameUIThemeDemo` | yes — three tabs in dedicated row | Already tuned to `(12, 4)`. |
| `NativeFrameUIComponentGallery` | yes — three tabs in dedicated row | Already tuned to `(12, 4)`. |
| `NativeFrameUISettingsDemo` | no — single dialog with form controls | Scenario: form surface, tabs would be feature work. |
| `NativeFrameUIShowcase` | no — direct-paint evaluation surface | Scenario: focuses on metrics and charts. |
| `NativeFrameUIDarkStudio` | no — single dark-mode product page | Scenario: dark-only hero surface. |
| `NativeFrameUIResourceGallery` | no — asset-grid surface | Scenario: browse-and-open flow. |
| `NativeFrameUICharts` | no — chart-only surface | Scenario: charts. |
| `NativeFrameUIControls` | no — single-window theme toggle sample | Light/dark controls demonstration. |
| `NativeFrameUIMinimal` | no — single centred button | Demonstration of minimal-link sample. |

This matches the CP5 samples-audit verdict: not every executable should
be a tab demo. Adding TabControl to the seven samples that don't have
it would change the product story each demo is telling.

## `on_palette_changed` audit

`Control::set_palette` already invalidates the HWND and the subclass
proc chains `DefSubclassProc` for `WM_THEMECHANGED` and
`WM_SYSCOLORCHANGE`, so system-theme changes reach the native control
without any per-class code. TabControl stays correct without a hook
override.

| Control       | Hook override | Reason |
|---------------|---------------|--------|
| `TabControl`  | no (no change) | Native ComCtl32 v6 chrome; system theme is source of truth, `DefSubclassProc` chain already passes it through. |

## Verification

- `cmake --build --preset x64-debug` — clean, no warnings (added
  `set_padding` does not trigger C4834 because every sample site is
  already `static_cast<void>(...)`-ed, consistent with the convention
  used for the other `bool`-returning native forwards).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`:
  both tests pass (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`). Smoke test still exercises TabControl
  creation + parent-destruction invalidation at `tests/nativeframeui_smoke.cpp:403-405`
  and `tests/nativeframeui_smoke.cpp:652-653`; no regressions.
- No public API breakage: `set_padding` is purely additive (single new
  method, single new free function-style forwarder).
- Boundary check still green — no MFC/ATL/WTL/BCG include added.

## Future work (deferred)

- `TabControl::set_min_tab_width(int)` and `set_fixed_item_size(int w, int h)`
  that forward `TCM_SETMINWIDTH` / `TCM_SETITEMSIZE` so fixed-width
  mode (`TCS_FIXEDWIDTH`) can be opted into without a raw
  `SendMessage`. Same pattern as `set_padding` — additive only, no
  custom draw required.
- Per-tab icons via `TCIF_IMAGE` + `TCM_SETIMAGELIST` once a consumer
  asks for them. The current API surface stays minimal until there's a
  real use case.
- Custom-draw tab hook consuming `FrameStyle::chrome_text` /
  `chrome_bg`. Still explicitly deferred per CP3 + CP5A. The headers
  already reserve the fields for it.

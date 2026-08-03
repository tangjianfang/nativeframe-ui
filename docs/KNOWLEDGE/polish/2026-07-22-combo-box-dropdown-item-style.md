---
title: ComboBox dropdown item styling
date: 2026-07-22
tags: [combobox, dropdown, ux]
severity: major
effort: medium
status: resolved
---

CBS_DROPDOWNLIST renders via the OS listbox internally; the V1 ListBox
owner-draw styling does NOT apply to ComboBox dropdowns because they
share a different HWND. Consumers using ComboBox get native chrome
selection visuals, not the Claude palette.

Two paths forward:
1. (Cheap) Document the limitation, ship as-is, defer.
2. (Real) Subclass the ComboBox's dropdown HWND with the same owner-draw
   pipeline as ListBox; complex because the dropdown HWND lifetime is owned
   by the OS.

Defer to V1.5+.

## Resolution (2026-07-22)

Decision: **option 1 — defer to V1.5+**.

The dropdown listbox is an OS-internal `ComboLBox` HWND created on first
popup and destroyed on close. The existing `Control::subclass_proc`
hook only fires for controls subclassed via `Control::create_native`;
it does not reach `ComboLBox`. Intercepting its draw would require:

1. `SetWindowsHookEx(WH_CALLWNDPROCEXT, ...)` to catch the popup
   creation.
2. A separate subclass proc installed on the transient popup HWND.
3. Mirror `Control::draw_list_item` against the popup's DC.
4. Clean up the hook on parent `WM_NCDESTROY`.
5. Defensive code for cross-version behaviour between user32 and
   ComCtl32 v6.

Realistic footprint: ~150 LOC of plumbing with non-trivial lifetime
semantics and version-dependent fragility. The V1.4 polish budget
cannot absorb this risk. Smoke test only validates HWND existence
and styles — it does not exercise a live dropdown, so it would pass
even with a broken styling hook. Cost outweighs the polish value
for V1.4.

Track in `docs/KNOWLEDGE/polish/` for future revisit when a sample
app specifically needs ComboBox dropdown theming.

## Superseded (CP-A2, 2026-07)

The WH_CALLWNDPROCEXT hook path was never needed. `nfui::ComboBox` now
creates the control with `CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
CBS_HASSTRINGS`: owner-draw applies to **both** the closed selection
field and the transient `ComboLBox` popup items, because ComCtl32 sends
`WM_DRAWITEM` (type `ODT_COMBOBOX`) for the popup rows to the control's
parent — no hook, no OS-owned HWND lifetime problem. The reflection path
in `ComboBox::visual_subclass_proc` draws every row from the injected
palette (`draw_item`), and `paint_chrome()` paints the border / button /
chevron from `state_palette()` per visual state. `theme_disable_window_theme`
suppresses the ComCtl32 dark-chrome double-paint. This entry is resolved.

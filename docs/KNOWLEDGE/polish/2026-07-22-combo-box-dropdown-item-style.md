---
title: ComboBox dropdown item styling
date: 2026-07-22
tags: [combobox, dropdown, ux]
severity: major
effort: medium
status: open
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

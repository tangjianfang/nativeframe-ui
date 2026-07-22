---
title: ListBox per-row hover highlight
date: 2026-07-22
tags: [listbox, hover, ux]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-23-cp11-sample-per-component-link.md
---

The monolithic Controls.cpp contains a comment `// LBS_OWNERDRAW* excluded:
per-row hover highlight not yet implemented; avoids needless repaint.`
This means hovering over a row in a ListBox shows no visual feedback until
selection. Owner-draw fixed-style ListBoxes can implement per-row hover via
ODT_LISTBOX + WM_MOUSEMOVE on the parent.

Implementation sketch:
1. Track mouse via HoverState on the ListBox HWND.
2. On hover state change, invalidate the previously-hovered row + the new row.
3. In ListBox::draw_item, read hover state and apply a lighter background.

Defer to V1.4 polish.

## Resolution

Commit `52525ff polish(listbox): per-row hover highlight via
ListBox::set_hovered_row` (predates CP1) implemented the feature:

- `ListBox::hovered_row_` member (`include/nfui/Controls/ListBox.hpp:44`).
- `ListBox::set_hovered_row(int row)` (`src/controls/ListBox.cpp:15-25`).
- `ListBox::on_subclass_mouse_move(LPARAM lparam)` queries `LB_ITEMFROMPOINT`
  to map cursor → row index (`src/controls/ListBox.cpp:66-82`).
- `ListBox::on_subclass_mouse_leave()` clears the hover state
  (`src/controls/ListBox.cpp:84-86`).
- `ListBox::draw_item` applies a 6% text-on-surface tint when the current
  row matches `hovered_row_` (`src/controls/ListBox.cpp:35-40`).

The implementation chose whole-control invalidation (cheap for typical list
sizes) over per-row rect invalidation. Per-row rect invalidation remains a
follow-up for V1.5 if a consumer ever shows a list large enough that whole-
control redraws become noticeable.

CP13 stale-doc cleanup flips this entry from `open` to `resolved`.

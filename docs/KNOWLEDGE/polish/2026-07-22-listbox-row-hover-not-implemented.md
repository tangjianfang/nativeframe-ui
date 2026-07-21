---
title: ListBox per-row hover highlight
date: 2026-07-22
tags: [listbox, hover, ux]
severity: minor
effort: small
status: open
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

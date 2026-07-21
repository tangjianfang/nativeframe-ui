---
title: ListBox::draw_item was dead code until P1.2
date: 2026-07-22
tags: [listbox, owner-draw, dead-code]
status: resolved
---

## Problem
After the layered-architecture migration (T8), `nfui::ListBox::draw_item`
was declared in `ListBox.hpp` and defined in `ListBox.cpp`, but the
actual owner-draw dispatch in `Control::subclass_proc`
(`src/controls/Controls.cpp`) routed `OCM_DRAWITEM` for `ODT_LISTBOX`
to an anonymous-namespace helper `draw_list_item`. The per-component
method was unreachable.

## Impact
The new `ListBox::draw_item` (with `ListStyle` support) was effectively
dead code. Consumers using `nfui_listbox` got the old helper's output
without `ListStyle` overrides. Visual mismatch between the new
`ListStyle` API and rendered output.

## Resolution
P1.2 (commit 52525ff4) added per-row hover highlight and noticed the
disconnect while implementing. Fix: removed `draw_list_item` helper,
made `ListBox::draw_item` `public`, added dispatch in `subclass_proc`
(`static_cast<ListBox*>(control)->draw_item(di)`).

## Lesson
When extracting a per-component class from a monolithic implementation,
verify the dispatch path actually routes to the new method. "Declared
in header, defined in source" is necessary but not sufficient — the
caller must call it.

---
title: V2.0 Property Grid control
date: 2026-07-22
status: living
tags: [v2, propertygrid, control]
---

## Goal
Provide a two-column grid (property name | editable value) for
inspecting and editing object properties. Drives IDE-style tools,
configuration panels, and dynamic property editors.

## Why deferred from V1
- Per-type editors require a registry + factory pattern.
- Nested properties (expandable categories) need tree-like UI.
- Custom attributes (`[Category]`, `[Description]`, `[Editor]`) need
  reflection or registration.

## Architecture
- `nfui::PropertyGrid` (Control).
- `nfui::PropertyDescriptor` (name, type, getter/setter, attributes).
- `nfui::PropertyEditor` (per-type editor base).
- `nfui::PropertyEditorFactory` (registry of type → editor).

## Components
- New `nfui_propertygrid` library.
- Reuses V1 `nfui::ExpandablePanel` (planned V1.5) for categories.

## Open questions
- Reflection: do we require C++20 sourcegen, runtime RTTI only, or
  consumer-provided descriptor arrays?
- Validation: where do invalid value notifications live (model layer
  vs editor layer)?
- Read-only mode: full grid disabled, or per-property lock?

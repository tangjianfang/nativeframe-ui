---
title: wxWidgets comparison
date: 2026-07-22
tags: [wxwidgets, cross-platform, competitive]
status: living
---

## What they have
- Cross-platform: Win32, macOS (Cocoa), GTK, Motif. Each native widget
  wrapped.
- Mature ecosystem: 30+ years of accumulated controls + samples.
- Permissive license (wxWindows).
- Per-platform appearance (truly native, not just rendered).

## What we have that they don't
- Modern C++20 throughout (wxWidgets mixes C++98-era idioms).
- Zero dependency on platform GUI toolkits (we own the rendering).
- Per-component STATIC library split.
- Free-form owner-draw theming — palette-driven, not bound to OS chrome.
- CSS-style `*Style` struct overrides per control.

## What we explicitly choose NOT to clone
- Cross-platform abstraction (V1 non-goal; we're Win32-only).
- Per-platform native chrome delegation (V1 uses owner-draw for Claude
  palette fidelity).

## Positioning
wxWidgets is the "Win32 API on every platform" choice. We're the
"Win32 API done modern, with fine-grained components". Overlap is
small — most consumers will pick one or the other based on whether
 they need cross-platform or just Windows.

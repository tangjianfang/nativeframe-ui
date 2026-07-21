---
title: WinForms comparison
date: 2026-07-22
tags: [winforms, managed, competitive]
status: living
---

## What they have
- Mature (.NET, since 2002).
- Simple event-driven model.
- Designer integration in Visual Studio.
- Large control library (300+).

## What we have that they don't
- Native C++20, no CLR dependency.
- Per-component STATIC library split.
- Owner-draw theming via Claude palette.
- Smaller footprint, deterministic destruction.

## What we explicitly choose NOT to clone
- Visual Studio designer integration (V1 non-goal).
- 300+ control surface (V1 has ~15 focused controls).

## Positioning
WinForms is the "rapid Windows forms in .NET" choice. Same niche as
WPF for managed stacks. NativeFrameUI is the native alternative.

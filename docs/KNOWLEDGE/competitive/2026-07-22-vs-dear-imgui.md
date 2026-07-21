---
title: Dear ImGui comparison
date: 2026-07-22
tags: [dear-imgui, immediate-mode, competitive]
status: living
---

## What they have
- Immediate-mode rendering: every frame, the entire UI is described by
  sequential `if (ImGui::Button("..."))` calls. No retained widget tree.
- Single-pass layout: no invalidation, no caching, no z-order fights.
- Built-in docking + viewports (1.90+).
- Multi-platform: Win32, macOS, Linux, web via Emscripten.
- Permissive license (MIT).
- ~3000 LOC core; small build footprint.

## What we have that they don't
- Retained widget tree with object lifetime — Windows outlive frames, so
  control state (selection, scroll position, focus) persists naturally.
- Native HWND per Control — accessible to accessibility tools, screen
  readers, OS theming, IME.
- Per-component STATIC library split — fine-grained linking.
- Owner-draw styled chrome matching a designed palette (Claude), not
  bitmaps rasterized to a texture.

## What we explicitly choose NOT to clone
- Immediate-mode paradigm (V1 non-goal — architecture is retained-mode
  by design).
- Built-in docking framework (V1 non-goal).

## Why this matters
Dear ImGui solves a different problem (developer-facing debug UI and
tools) than NativeFrameUI (product-facing polished apps). The contrast
illustrates that retained-mode + native HWND + per-component linking
incur some complexity, but are essential for shipping a real product
where users interact with the UI over time, not just inspect it.

If you need a debug-only UI for a tools product, Dear ImGui is
excellent. If you need a polished Win32 product UI, NativeFrameUI
serves that niche.

---
title: Sciter / Ultralight comparison
date: 2026-07-22
tags: [sciter, ultralight, html, competitive]
status: living
---

## What they have
- HTML/CSS/JS rendering embedded in a native window.
- Web technology reuse — same UI runs in browser and desktop.
- Sciter: TIScript, single-file deployable, ~5MB engine.
- Ultralight: modern fork using HTML/CSS/JS, hardware-accelerated,
  GPU rendering.

## What we have that they don't
- No HTML/CSS/JS learning curve — plain C++.
- No engine runtime dependency (no WebView2, no Sciter DLL).
- Native control accessibility without needing ARIA.
- Tighter integration with Win32 message loop.
- Smaller footprint.

## What we explicitly choose NOT to clone
- HTML/CSS markup (V1 non-goal).
- JavaScript scripting (V1 non-goal).
- WebView2 / WebKit embedding (V1 non-goal).

## Positioning
Sciter/Ultralight is the "ship web UIs in native apps" choice.
NativeFrameUI is the "ship native Win32 UIs without web layers". Both
solve "polished desktop UI" but from opposite ends.

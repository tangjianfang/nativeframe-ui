---
title: Wine (Linux/macOS via Wine) compatibility
date: 2026-07-22
status: living
tags: [platform, wine, linux, macos]
---

## Context
Wine translates Win32 calls to POSIX. Some consumers want to run
NativeFrameUI apps on Linux/macOS via Wine or Proton. We don't
officially support this, but surveying what's needed informs future
porting choices.

## Survey
- **GDI/GDI+**: Wine supports a large subset but reports
  compatibility issues with some owner-draw + double-buffering
  combinations. Most basic painting works.
- **Common Controls v6**: Wine ships comctl32 with most features.
  Themed v6 mostly works but some `DrawThemeText` calls have
  rendering glitches.
- **DWM**: Wine has partial DWM support — composition effects may
  not render the same as Windows.

## Blockers
- **HiDPI**: Wine's DPI virtualization is less mature than
  Windows'; per-monitor V2 may behave inconsistently.
- **Direct2D**: Wine's Direct2D implementation is incomplete;
  apps selecting the D2D backend would fail. The GDI fallback
  would still work.
- **DWM rounded corners**: may render as sharp corners.

## Plan
1. V1.3: add a `wine-test` job to CI that runs the smoke test
   binary under Wine on Ubuntu. Mark as informational, not blocking.
2. V1.4: document which controls have known Wine rendering issues.
3. V2.0: when D2D backend is selectable, document that Wine forces
   GDI fallback.

## Open questions
- Is Wine a supported platform or just "tested"? Decide at V1.3.
- macOS via CrossOver (Wine-derived) — same compatibility story.

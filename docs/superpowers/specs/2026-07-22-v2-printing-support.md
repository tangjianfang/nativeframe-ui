---
title: V2.0 printing and print preview
date: 2026-07-22
status: living
tags: [v2, printing, gdi]
---

## Goal
Provide print and print-preview APIs for documents built from
NativeFrameUI controls. Use GDI `StartDoc`/`StartPage` (works on all
V2-supported Windows versions) so no new dependencies.

## Why deferred from V1
- Pagination logic depends on data model; generic UI virtualization
  isn't enough.
- Print preview is a separate Window with its own chrome.
- Page setup dialog requires localization support.

## Architecture
- `nfui::PrintDocument` (model: pages, headers, footers, paper size).
- `nfui::PrintEngine` (drives StartDoc/StartPage).
- `nfui::PrintPreviewWindow` (renders pages in scrollable surface).

## Components
- New `nfui_print` library.
- `nfui::PageSetupDialog` (wraps `PrintDlgEx` with our defaults).
- `nfui::PrintPreviewControl`.

## Open questions
- Vector vs raster: stay GDI bitmaps, or print to XPS for vector?
- Direct2D print path: when Direct2D backend is selected, do we
  have a separate D2D-print path?
- Localization: separate strings file or message-only resources?

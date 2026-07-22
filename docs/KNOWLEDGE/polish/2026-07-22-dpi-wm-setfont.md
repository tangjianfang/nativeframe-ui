---
title: DPI change WM_SETFONT propagation
date: 2026-07-22
tags: [dpi, font, wm-setfont]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-23-cp8-font-cache.md
---

When the system DPI changes (user moves the window to a different
monitor), the OS sends `WM_SETFONT` with a new HFONT scaled for the
new DPI. Our `Control::subclass_proc` does not currently intercept
this; controls may render at the wrong size until they recreate.

Verify: does `nfui::Dpi::on_dpi_changed` get called on the
`Application` context? Does `FontCache` invalidate its cached HFONTs?
If not, manually send `WM_SETFONT` to all owned HWNDs when DPI
changes.

Implementation budget: ~30 LOC to wire up DPI-change broadcast via
WM_SETFONT. Defer to V1.4 polish.

## Resolution (CP2 + CP8)

`Control::subclass_proc` in `src/controls/Controls.cpp:143-153` now
intercepts `WM_SETFONT`:

```cpp
case WM_SETFONT: {
    // P6.1: forward WM_SETFONT to defaults and invalidate the control so
    // the new HFONT actually re-measures text on the next paint.
    ...
}
```

The handler chains `DefSubclassProc` so the native control (Edit,
ComboBox, ListBox, TabControl, StatusBar, TreeView, etc.) re-binds its
internal metrics against the new font, then `InvalidateRect`s the HWND
so framework owners repaint with the new face. CP8's FontCache work
(`2026-07-23-cp8-font-cache.md`) added the `font_pt::ui` token
replacement and per-DPI HFONT cache invalidation, so when DPI changes
the FontCache hands out a re-scaled font and the WM_SETFONT path
delivers it to the HWNDs.

Net effect: a DPI change (manual via Display settings or implicit via
window move between monitors) is now reflected in every framework
control's text size without recreating the HWND. CP13 stale-doc
cleanup flips this entry from `open` to `resolved`.
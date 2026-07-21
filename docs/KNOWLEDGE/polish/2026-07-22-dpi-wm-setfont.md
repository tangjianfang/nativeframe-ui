---
title: DPI change WM_SETFONT propagation
date: 2026-07-22
tags: [dpi, font, wm-setfont]
severity: minor
effort: small
status: open
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
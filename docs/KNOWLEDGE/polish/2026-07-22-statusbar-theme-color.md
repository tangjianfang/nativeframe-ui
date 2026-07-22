---
title: StatusBar Claude palette background
date: 2026-07-22
tags: [statusbar, chrome, theme]
severity: minor
effort: small
status: resolved
related:
  - 2026-07-23-cp1-statusbar-samples-audit.md
---

`SB_SETBKCOLOR` is unsupported in modern ComCtl32 v6 (confirmed via
MSDN and the P1.4-P1.6 agent's research). `nfui::StatusBar` currently
displays with the OS default chrome.

Alternatives:
1. Subclass the StatusBar HWND with a custom WM_PAINT that fills
   `palette.surface`.
2. Use the `SB_SETTEXT` + `SBT_OWNERDRAW` per-part flag (limited).
3. Accept the limitation and document.

Implementation budget: option (1) is ~40 LOC. Defer to V1.4 polish.
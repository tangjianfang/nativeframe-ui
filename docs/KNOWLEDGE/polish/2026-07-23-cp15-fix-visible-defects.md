---
title: CP15 fix user-visible defects + visual consistency
date: 2026-07-23
tags: [polish, controls, samples, button, statictext, dialog]
severity: major
status: in-progress
related:
  - 2026-07-23-cp2-corner-abruptness.md
  - 2026-07-23-cp3-component-states.md
  - 2026-07-23-cp10-high-contrast.md
  - 2026-07-23-cp12-dialog-tour-sample.md
---

# CP15 — Fix user-visible defects + visual consistency

Branch: `polish/CP15-fix-visible-defects`.

Triggered by user feedback 2026-07-23: "希望每一个 demo 都是精品、极品展示给用户，
而不是展示出一个原生的、灰突突的、一点样式都没有的 demo。" Three concrete
defects called out:

1. Buttons in dark theme show a white flash on the rounded corner.
2. Static text shows a gray background, strong contrast vs main chrome.
3. Native MessageBox is "very old-fashioned / 90s style".

## Root causes

### 1. Button white corner flash in dark theme

`src/controls/Button.cpp:10`:
```cpp
return create_native(L"BUTTON", owner_params, BS_OWNERDRAW | BS_FLAT);
```

`BS_FLAT` is not a no-op alongside `BS_OWNERDRAW`. Windows draws the default
flat-button chrome once during the first paint cycle before our DRAWITEM
handler reaches the DC, and the flat fill is the system light grey
(`RGB(240,238,230)`). In dark mode that frame is visible until the next
full repaint. CP2 fixed the runtime palette-switch case but not the
first-paint case.

### 2. Static text gray background

`src/controls/StaticText.cpp:10`:
```cpp
return create_native(L"STATIC", static_params, SS_LEFT);   // ← no SS_OWNERDRAW
```

Without `SS_OWNERDRAW`, `Control::subclass_proc` cannot dispatch
`WM_DRAWITEM` to the leaf — the system default paints via
`WM_CTLCOLORSTATIC` returning the system brush (`COLOR_BTNFACE`,
`RGB(240,240,240)` in light mode, `RGB(50,50,50)` in dark). Our
`on_paint` is only reached through `wants_self_paint()`-gated WM_PAINT,
which **chains** `DefSubclassProc` afterward — so the system still
overlays its gray background over our palette paint, and the user sees
gray.

### 3. Native MessageBox style

`samples/NativeFrameUIMinimal/NativeFrameUIMinimal.cpp:25`,
`samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp:35`,
`samples/NativeFrameUIDialogTour/NativeFrameUIDialogTour.cpp:271`

use `MessageBoxW` for click feedback, About dialogs, and validation
warnings. The Win32 MessageBox is a USER32.DLL dialog class that does
not honour per-app palettes, never gets `WM_CTLCOLOR*` routing, and
on Windows 11 still renders the 1995 chrome. NativeFrameUI owns
`nfui::Dialog` and a framework dialog template (`IDD_NFUI_ABOUT`)
but three samples bypass it.

## Fix

### A. Button: drop `BS_FLAT`

```cpp
return create_native(L"BUTTON", owner_params, BS_OWNERDRAW);
```

Comment now explains why `BS_FLAT` is rejected. The first-paint chrome
will go straight to our DRAWITEM handler.

### B. StaticText: switch to `SS_OWNERDRAW`

`src/controls/StaticText.cpp:10` becomes:
```cpp
return create_native(L"STATIC", static_params, SS_OWNERDRAW | SS_NOTIFY);
```

The existing `Control::subclass_proc` already routes
`ocm_base + WM_DRAWITEM` with `ODT_STATIC` to `on_paint`. But
`wants_self_paint()` returns `true`, so WM_PAINT also fires our paint —
double painting on every redraw. Fix in `src/controls/Controls.cpp`:

```cpp
case WM_PAINT: {
    if (control != nullptr
        && control->wants_self_paint()
        && !control_is_owner_draw(hwnd)) {     // CP15: skip for owner-draw
        // ... existing body
    }
    break;
}
```

`control_is_owner_draw(hwnd)` already exists (line 13).

### C. Samples: drop `MessageBoxW`

**`samples/NativeFrameUIMinimal`** — replace with in-window
`SetWindowTextW`-driven status label. A minimal sample does not need
a dialog at all; the new approach is also a better demonstration of
the per-component library footprint.

**`samples/NativeFrameUIWorkbench`** — replace About handler with a
modal `nfui::Dialog` over `IDD_NFUI_ABOUT`. The template already
exists in `resources/NativeFrameUI.rc`; we just route through it.

**`samples/NativeFrameUIDialogTour`** — the validation warning in
`prefs_dlg_proc` becomes a sibling `LTEXT` next to the Edit control
that turns red on empty submit. The user stays in the same dialog,
no native chrome flash.

## Files changed

| File | Change |
|---|---|
| `src/controls/Button.cpp` | Drop `BS_FLAT` from `create_native`. |
| `src/controls/StaticText.cpp` | Add `SS_OWNERDRAW \| SS_NOTIFY`. |
| `src/controls/Controls.cpp` | Skip `WM_PAINT` self-paint when owner-draw is set on the same HWND. |
| `samples/NativeFrameUIMinimal/NativeFrameUIMinimal.cpp` | Replace `MessageBoxW` with an in-window `StaticText` status label; link the extra lib. |
| `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp` | Use `nfui::Dialog` + `IDD_NFUI_ABOUT` for About; drop `MessageBoxW`. |
| `samples/NativeFrameUIDialogTour/NativeFrameUIDialogTour.cpp` | Validation warning becomes an in-dialog `LTEXT`. |
| `tests/nativeframeui_smoke.cpp` | Re-target the SS_LEFT assertion: StaticText is now `SS_OWNERDRAW`. |
| `docs/KNOWLEDGE/polish/2026-07-23-cp15-fix-visible-defects.md` | This doc. |

## Verification

- `cmake --build --preset x64-debug` clean.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 3/3 (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`, `NativeFrameUIArchitectureCheck`).
- Manual: launch each affected sample, confirm no white corner on
  buttons during theme switch, no gray box around static text, no
  native MessageBox chrome in any sample.

## Lessons

- **`BS_FLAT` is poison alongside `BS_OWNERDRAW`** — any flat-style
  flag triggers one extra default-paint pass during the first paint
  cycle. The same trap applies to `BS_PUSHBUTTON` etc. for buttons;
  we should add a lint check for this pattern in CP19.
- **Owner-draw style flags must propagate through both `DRAWITEM`
  and `WM_PAINT`** — without the `control_is_owner_draw` guard in the
  WM_PAINT path, adding `SS_OWNERDRAW` would have caused double-paint
  on every redraw. The existing code was right about owner-draw and
  WM_PAINT being orthogonal, but the guard was missing.
- **`MessageBoxW` is not a sample showcase** — its chrome bypasses
  every visual contract in the framework. Replacing it with
  `nfui::Dialog` (which we already have) keeps the samples honest
  about what the framework can deliver.
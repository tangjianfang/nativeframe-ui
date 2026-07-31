# Chrome QA Contract

> Single source of truth for the painted-chrome forwarding contract. Every
> `Control` wrapper that paints its own chrome MUST follow the rules in
> `docs/ARCHITECTURE.md`; this document only adds the cross-control
> forwarding layer.

## Scope

NativeFrameUI controls paint their chrome with two complementary mechanisms:

1. **Self-paint** — the wrapper owns the entire `WM_PAINT` / `WM_DRAWITEM`
   cycle (Button, ComboBox, CheckBox, RadioButton, TabControl, StatusBar,
   Edit, Scrollbar, Menu, Splitter). Nothing visual comes from the native
   control; everything is drawn by `nfui` code.
2. **Forwarded paint** — the wrapper forwards a notification (typically
   `NM_CUSTOMDRAW` or `WM_DRAWITEM`) into a `nfui` helper that paints the
   themed chrome on top of whatever the native control produced
   underneath. This is the contract that keeps ListView, TreeView, and the
   standalone Scrollbar reading as one design language.

The boundary check (see `tools/verify_boundaries.ps1`) enforces "no MFC /
ATL / WTL / BCG" at the include level. The chrome QA contract enforces
"native chrome and `nfui` chrome agree on colour and shape" at the
visual level.

## Forwarding pattern: themed thumb on system scrollbar

The native SCROLLBAR chrome is left intact underneath our themed thumb.
This is intentional — turning the native chrome off entirely triggers a
set of Windows bugs (no thumb drag, missing page-scroll, dark-mode
comctl32 background leak) that we already paid for once in
`docs/KNOWLEDGE/polish/2026-07-23-cp20-*.md`. Forwarding gives us a stable
60% alpha overlay in every theme without those regressions.

### Contract

The static helper:

```cpp
nfui::Scrollbar::paint_thumb_into(
    HDC target,           // destination HDC (CDDS_POSTPAINT hdc, PaintDC, etc.)
    const RECT& track,    // band along the right / bottom edge, host client coords
    bool vertical,        // true → vertical bar, false → horizontal bar
    int position,         // GetScrollPos(SB_VERT / SB_HORZ)
    int min,              // GetScrollInfo SIF_RANGE nMin
    int max,              // GetScrollInfo SIF_RANGE nMax
    const ThemePalette& palette  // active palette, NOT the host's effective_palette
) noexcept;
```

Forwarding host responsibilities:

1. **Opt in to POSTPAINT** in the `CDDS_PREPAINT` branch by including
   `CDRF_NOTIFYPOSTPAINT` in the return value.
2. **Resolve the band RECT** in the host's client coordinates: for
   `SB_VERT` that is `cd->nmcd.rc` with `left = right - sb_width`; for
   `SB_HORZ` that is `cd->nmcd.rc` with `top = bottom - sb_height`. The
   `sb_width` / `sb_height` come from `DpiScale::logical_to_pixels(16)` —
   `SM_CXVSCROLL` / `SM_CYHSCROLL` measured in logical pixels.
3. **Skip the paint when there is no scrollable range**:
   `GetScrollInfo` with `SIF_RANGE | SIF_POS | SIF_PAGE` and gate on
   `nMax > nMin && (nMax - nMin) > nPage`. Painting into a flat band is a
   no-op visually but still costs a draw cycle.
4. **Pass the active palette explicitly** — never the host's effective
   palette, so the helper can be reused from a control that has not
   declared its own `palette()` override.
5. **Do not call `paint_thumb_into` outside `CDDS_POSTPAINT`** — the
   forwarder contract relies on the system's clip region, which the host
   control only sets up correctly during its own paint cycle.

### Implementations

| Host control | File | Hook |
|---|---|---|
| ListView | `src/controls/ListView.cpp` | `handle_custom_draw(CDDS_POSTPAINT)` |
| TreeView | `src/controls/TreeView.cpp` | `handle_custom_draw(CDDS_POSTPAINT)` |
| Scrollbar (standalone) | `src/controls/Scrollbar.cpp` | `paint_chrome()` (internal use only — see "Single source of truth" below) |

Future controls that host a native SCROLLBAR inside their client area
(ComboBox dropdown, IconView overflow, RichEdit) MUST follow the same
five-step recipe and add their row to the table.

### Single source of truth

`Scrollbar::paint_chrome` calls `paint_thumb_into` with the wrapper's own
client rect as the track. The standalone instance is the entry point; the
forwarding hosts are consumers. Geometry math lives in
`thumb_rect_for` (anonymous namespace in `src/controls/Scrollbar.cpp`).

Changing thumb width, length, alpha, or palette requires no edits to the
forwarding hosts — `paint_thumb_into` is the single knob.

## Forwarding pattern: themed chrome on system button / edit

Forwarding a themed chrome onto a native button or edit control would
require replacing the entire `WM_PAINT` cycle, which is exactly what the
self-paint wrappers already do (Button, Edit, ComboBox). There is no
forwarding host for those — they are full self-paint.

If a future control needs a non-painting system widget inside an `nfui`
host (e.g. a property grid that hosts native COM controls), that host
MUST inherit from `SelfPaint` and follow the same `NM_CUSTOMDRAW` →
`paint_thumb_into` pattern this document already covers.

## Visual gate

The visual audit (`out/build/x64-debug/Debug/NativeFrameUIVisualAudit.exe`)
captures every demo at 100 / 125 / 150 / 200% DPI across `light` / `dark` /
`hc` themes. A per-pixel RGB diff against `baseline/` gates the build
(`docs/PROJECT_PLAN.md` phase 8 + `tools/verify_boundaries.ps1`). The
themed thumb renders inside the audit PNGs, so any regression in the
forwarding contract shows up there before the build does.

## Why we don't make the native chrome invisible

| Approach | Pros | Cons |
|---|---|---|
| Suppress native chrome entirely (theme_disable_window_theme + repaint) | One draw path | Breaks `WM_HSCROLL` / `WM_VSCROLL`, kills track-page-scroll, comctl32 leaks light-mode background into dark |
| Theme the native chrome (uxtheme ScrollbarPainter) | Native hit-test works | uxtheme on Win10+ ignores dark-mode accent; only one global theme |
| Forward our thumb on top of native chrome | Stable across all Win10 / 11 themes, 60% alpha overlay reads in any palette | Two paints (native + ours) — ~0.4 ms per draw cycle on mid-range hardware |

We chose forwarding. The two-paint cost is acceptable; the visual
consistency win is not negotiable.

## Forwarding contract changelog

- **CP-A4** — Standalone Scrollbar wrapper paints chrome via subclass
  proc that returns 0 on `WM_PAINT`. No native chrome involved.
- **CP-B19** — `Scrollbar::paint_thumb_into` added as static helper;
  ListView / TreeView forward their `CDDS_POSTPAINT` to it. This
  document created.
---
title: CP16 shadow + gradient system (Office Fluent / Material)
date: 2026-07-23
tags: [polish, paint, shadow, gradient, panel, button, theme]
severity: major
effort: medium
status: in-progress
related:
  - 2026-07-23-cp15-fix-visible-defects.md
  - 2026-07-23-cp3-component-states.md
---

# CP16 — Shadow + gradient system

Branch: `polish/CP16-shadow-gradient-system`.

CP15 closed the visible-defect surface (button first-paint chrome,
StaticText gray box, native MessageBox chrome). CP16 layers the depth
language that distinguishes "professional Win32" from "1995 chrome":
soft elevation shadows and surface gradients. The visual language is
**Office Fluent / Material**: 32-bit ARGB shadows with light, even
falloff; linear vertical gradients on faces; no glow / blur / chromatic
effects.

## Surface area

### 1. Paint primitives (`src/paint/Paint.cpp` + `include/nfui/Paint.hpp`)

```cpp
// Linear vertical gradient fill. Rounded when radius > 0.
// Falls back to a solid mid-tone if GradientFill is unavailable
// (pre-ComCtl32 v6 systems) so callers never see an unpainted rect.
void paint_linear_gradient(HDC dc, const RECT& bounds, int radius,
                           Color top, Color bottom) noexcept;

// Drop-shadow under a rounded rect. elevation ∈ {1, 2, 3} maps to
// {2px, 6px, 12px} soft falloff. Always renders 32-bit ARGB so it
// composites cleanly on any background.
void paint_drop_shadow(HDC dc, const RECT& bounds, int radius,
                       int elevation, Color shadow_color) noexcept;
```

Both helpers are header-only safe — they take `HDC` and never call
into the framework. They live in `nfui_paint` (already linked from
`nfui_control_base`), so no new transitive dependency.

### 2. Theme tokens (`include/nfui/Theme.hpp` + `src/theme/Theme.cpp`)

```cpp
struct ThemePalette {
    // ... existing 14 tokens ...
    Color shadow;             // CP16: default drop-shadow tint (RGB(0,0,0,40%) for light, RGB(0,0,0,60%) for dark)
};
```

Shadow elevation is a fixed pixel ramp (2/6/12) so we do not need a
per-palette intensity token — only the shadow colour. The shadow
alpha is encoded in the `shadow.rgb` value: callers pass
`RGB(0, 0, 0)` with the high byte ignored and the helper uses a
fixed 25% / 30% / 40% alpha ramp at e1/e2/e3 to keep the falloff
constant across themes.

### 3. FrameStyle elevation hook (`include/nfui/Controls/FrameTypes.hpp`)

```cpp
struct FrameStyle {
    // ... existing optional fields ...
    std::optional<int> elevation;  // CP16: 0..3, default 0 (flat)
};
```

Per-component consumption table (CP5A extension):

| Control       | Reads `elevation` |
|---------------|-------------------|
| `Panel`       | yes (shadow + gradient on surface) |
| `Splitter`    | no (deferred to V2 — grip_color instead) |
| `StatusBar`   | no |
| `ProgressBar` | no |

`Panel::on_paint` already branches on `style_.corner_radius`. It now
additionally reads `style_.elevation`:

```cpp
if (elevation >= 1) {
    paint_drop_shadow(target, paint_bounds, radius, elevation, p.shadow);
}
fill_rect(target, paint_bounds, p.background);  // CP2: erase outside corners
const RECT inner = shrink(paint_bounds, elevation * 2);
if (gradient_enabled(p, inner)) {
    paint_linear_gradient(target, inner, radius - elevation,
                          lighten(p.surface, 0.05f), p.surface);
} else {
    fill_rounded_rect(target, inner, radius - elevation, p.surface, p.border);
}
```

`gradient_enabled(p, ...)` returns true for light/dark themes and
false for high-contrast (HC stays flat — solid fill is the right
depth language for HC).

### 4. Button face gradient (`src/controls/Button.cpp`)

```cpp
Color face = p.accent;
Color face_top = lighten(face, 0.08f);    // gentle top highlight
Color face_bottom = darken(face, 0.10f);  // settle to anchor

// ... state branches unchanged, except face_top / face_bottom
//     are derived together ...
paint_linear_gradient(target, paint_bounds, radius, face_top, face_bottom);
```

Disabled / pressed states remain solid (CP3 decision: muted/anchor
faces need a clear, single tone to read at small sizes).

## Files changed

| File | Change |
|---|---|
| `include/nfui/Paint.hpp` | `paint_linear_gradient` + `paint_drop_shadow` declarations. |
| `src/paint/Paint.cpp` | Implementations using `GradientFill` and 32-bit ARGB DIB. |
| `include/nfui/Theme.hpp` | Add `ThemePalette::shadow` field. |
| `src/theme/Theme.cpp` | Set `shadow` per theme: light/dark = RGB(0,0,0), HC = RGB(255,255,255) (white shadow under HC accent reads as a "halo" that doesn't muddy contrast). |
| `include/nfui/Controls/FrameTypes.hpp` | Add `FrameStyle::elevation`. |
| `src/controls/Panel.cpp` | Render drop-shadow + surface gradient when `style_.elevation` > 0. |
| `src/controls/Button.cpp` | Vertical gradient on rest + hover face. |
| `samples/NativeFrameUIShowcase/ShowcaseView.cpp` | Raise one card to `elevation = 2` for the screenshot demo. |
| `samples/NativeFrameUIComponentGallery/NativeFrameUIComponentGallery.cpp` | Add an "elevation 1/2/3" row showing the same Panel at three depths. |
| `tests/nativeframeui_smoke.cpp` | Add paint-path coverage for `paint_drop_shadow` and `paint_linear_gradient` (verify HDC handles created and released). |
| `docs/KNOWLEDGE/polish/2026-07-23-cp16-shadow-gradient.md` | This doc. |

## Verification

- `cmake --build --preset x64-debug` clean.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 3/3 (`NativeFrameUISmokeTest`,
  `NativeFrameUIBoundaryCheck`, `NativeFrameUIArchitectureCheck`).
- Smoke launch:
  - `NativeFrameUIComponentGallery.exe` shows the elevation row.
  - `NativeFrameUIShowcase.exe` shows the raised card with shadow.
  - Dark mode: shadows remain visible (the dark `shadow` color
    RGB(0,0,0) composites correctly against the dark surface).

## Lessons

- **`GradientFill` requires a 32-bit DIB**, not the default 8-bit
  HDC. We allocate a per-paint DIB section and BitBlt back to the
  target; the per-paint allocation is cheap (32×200×4 = ~25 KB per
  call) and avoids a long-lived cache surface.
- **Drop shadows need their own DIB** because GDI's `AlphaBlend`
  expects premultiplied alpha and a separate source DC. We render
  a Gaussian-ish falloff into the DIB by stepping from outer to
  inner alpha at half-pixel increments — that's the trick that
  gives the shadow its soft, even edge.
- **HC stays flat**: HC profiles make shadow a "halo", not depth.
  The `shadow` colour flips to white in HC and the gradient path
  short-circuits, so HC still reads as flat, single-tone — exactly
  the depth language Windows HC conventions prescribe.
- **Gradient on buttons only when state is unambiguous**: rest +
  hover get the gradient; pressed, disabled, and HC profiles skip
  it. The eye uses gradient to read "interactive, calm"; solid
  fills read "transient, mute".
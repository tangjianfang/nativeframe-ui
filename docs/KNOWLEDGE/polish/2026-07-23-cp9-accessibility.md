# CP9 — Accessibility audit (a11y) of controls, Window, Dialog

Date: 2026-07-23
Scope: `src/controls/*`, `include/nfui/controls/*`, `src/core/Window.cpp`,
`src/core/Dialog.cpp`.

## TL;DR

The framework is broadly accessible **by construction**: every interactive
control is a real Win32 window class (`BUTTON`, `EDIT`, `COMBOBOX`, `LISTBOX`,
`SysListView32`, `SysTreeView32`, `SysTabControl32`, `STATIC`) that is *owner-
drawn / custom-painted on top of* the native control, never replaced. That
means MSAA/UIA roles, keyboard input, caret/selection, and Alt-mnemonics are
delivered by the OS for free — we only repaint chrome.

One genuine gap was found and fixed: **`Button` signalled keyboard focus with
the same colour it uses for mouse-hover**, so a tab-focused button was visually
indistinguishable from a hovered one (WCAG 2.4.7 / 2.4.11 focus-visible). A
distinct inset focus ring was added.

## Audit matrix

| # | Audit item | Result |
|---|------------|--------|
| 1 | `WM_GETDLGCODE` handled correctly | ✅ No custom control swallows dialog navigation. All interactive controls keep the native class's default `WM_GETDLGCODE` (via `DefSubclassProc`), so Tab/arrow/Enter/Esc routing is intact. Passive controls (`StaticText`, `Panel`, `ProgressBar`, `StatusBar`, `IconView`) intentionally return `DLGC_STATIC`. |
| 2 | Custom `IAccessible` / UIA provider | ✅ Not needed. Because each control is a native window, MSAA exposes the correct default role (push button, checkbox, editable text, combo box, list, outline, tab, static text). A hand-rolled `IAccessible` would only *regress* the free native implementation. Documented as a deliberate non-goal for V1. |
| 3 | Tab order & keyboard focusability | ✅ `ControlCreateParams::style` defaults to `WS_CHILD|WS_VISIBLE|WS_TABSTOP`; focusable controls keep it. `StaticText::create` strips `WS_TABSTOP` (correct — labels are not tab stops); `IconView` omits it (passive image); `Splitter` omits it (see item 4). Tab order follows z-order / creation order as usual. |
| 4 | Focus ring visible on every focusable control | ⚠️→✅ **Fixed for `Button`** (see below). Native controls (`CheckBox`, `RadioButton`, `Edit`, `ComboBox`, list/tree/tab families) already draw the platform focus indicator; `Edit`/`ComboBox` additionally repaint their border to `accent_hover` on `WM_SETFOCUS`. Passive controls are correctly non-focusable so need no ring. `Splitter` is intentionally mouse-only (keyboard splitter nudging is a V1.5 feature, needs design review — noted, not implemented). |
| 5 | Screen reader (UIA/MSAA) recognises type + state | ✅ Native classes report role + state (checked/pressed/disabled/focused/value). Owner-draw does not alter the accessibility tree. `ProgressBar` keeps `PROGRESS_CLASS` so its value is exposed via `IValueProvider`/`RangeValue`. |
| 6 | Mnemonics / Alt-key accelerators | ✅ Captions containing `&` produce underlined mnemonics because the caption is passed straight to the native class (`Button`, `CheckBox`, `RadioButton`, `StaticText`). `STATIC` labels created with `&Name` still transfer focus to the next tab-stop control, matching Win32 convention. |
| 7 | High-contrast (HC) theme, WCAG AAA | ✅ Chrome colours are 100% palette-driven; the HC palette (audited in CP10B) supplies the tokens. The new `Button` focus ring uses `accent_text`, which the theme guarantees contrasts the accent face in light, dark, **and** HC palettes, so the ring stays visible in HC without a special case. |

## The fix — `Button` keyboard focus ring

Before: `Button::on_paint` set the *border* colour to
`style_.border_color.value_or(p.accent_hover)` when `state.focused`. But
`accent_hover` is also the mouse-hover face/border colour, so:

- keyboard focus == mouse hover, visually. A sighted keyboard user could not
  tell which button had focus while the pointer sat over another button, and
- against palettes where `accent_hover` is close to `border`, the focus cue was
  near-invisible → fails **WCAG 2.4.7 Focus Visible** (AA) and **2.4.11 Focus
  Appearance** (AAA).

After: when `ODS_FOCUS` is reflected (`state.focused && state.enabled`), we draw
a **distinct inset ring** ~3 logical px inside the button, in `accent_text`
(guaranteed high contrast against the accent face). Implementation reuses the
existing `fill_rounded_rect` primitive — the inset rect re-fills the interior
with `face` (no visual change) and its *border* becomes the ring; the caption is
drawn afterwards so it stays on top. All painting stays inside the offscreen
`MemoryDC`, so no flicker.

A new opt-in `ButtonStyle::focus_ring_color` lets a consumer override the ring
colour; it defaults to `accent_text`.

```cpp
if (state.focused && state.enabled) {
    const DpiScale scale(dpi_of(hwnd()));
    const int inset = scale.logical_to_pixels(3);   // logical → device px
    const RECT ring{ /* paint_bounds inset by `inset` on all sides */ };
    if (ring.right > ring.left && ring.bottom > ring.top) {
        const int ring_radius = radius > inset ? radius - inset : 0;
        const Color ring_color = style_.focus_ring_color.value_or(p.accent_text);
        fill_rounded_rect(target, ring, ring_radius, face, ring_color);
    }
}
```

### Why `accent_text` and not a fixed colour

`accent_text` is defined by every palette as "text drawn on the accent surface",
so it is *already* validated to contrast the accent face (which is the button's
own fill). Reusing it means the ring inherits the theme's contrast guarantee for
free — including the CP10B HC palette — with no new token and no per-theme
branch.

## Explicitly-not-changed (with rationale)

- **`Splitter` keyboard operation** — a splitter *should* be keyboard-nudgable
  (arrow keys) for full a11y, but that needs a focus model, a `WM_GETDLGCODE`
  returning `DLGC_WANTARROWS`, and layout-callback plumbing. That is a feature,
  not a polish fix; deferred to V1.5 with a design review.
- **Custom `IAccessible`/UIA providers** — would regress the native tree. V1
  non-goal.
- **`IconView` focus/selection** — deliberately a passive image (CP5B decision,
  `2026-07-23-cp5-iconview-tooltip.md`). Left non-focusable.
- **`Edit`/`ComboBox` focus border** — these keep the native focus indicator
  *and* tint the border, and the tint is not ambiguous with hover for a text
  field (no hover-face). Left as-is.

## Verification

- `cmake --build --preset x64-debug` — clean build, all targets incl. samples.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` — `NativeFrameUISmokeTest` and
  `NativeFrameUIBoundaryCheck` both pass.

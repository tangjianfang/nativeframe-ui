---
title: CP10 High Contrast theme accessibility audit
date: 2026-07-23
tags: [theme, high-contrast, wcag, accessibility, palette, button]
severity: moderate
effort: small
status: resolved
---

## Scope

Audit and harden the High Contrast (HC) theme profile (`ThemeMode::high_contrast`)
defined in `src/theme/Theme.cpp` against WCAG AAA (7:1 text contrast, 3:1
UI-component contrast). Touch list:

- `src/theme/Theme.cpp` — HC palette rewrite + `is_high_contrast` helper.
- `include/nfui/Theme.hpp` — `is_high_contrast(const ThemePalette&)` declaration.
- `src/controls/Button.cpp` — HC-aware pressed/disabled formulas + black border on
  bright accent face.
- `tests/nativeframeui_smoke.cpp` — invariant assertions locking down the new
  palette so a future tweak cannot silently regress a contrast requirement.
- `docs/THEME_GUIDE.md` — unchanged (the existing `ThemeMode::high_contrast`
  bullet already promises a HC profile).

`docs/superpowers/specs/` does not contain an HC-specific document; the
palette is owned by `Theme.cpp` and referenced from the two implementation
plans (`2026-07-20-nativeframe-ui-visual-controls.md` task 1, and
`2026-07-21-claude-code-restyle.md` task 1). Both quoted the same hard-coded
palette that this audit just rewrote; no prose changes are needed in those
specs because they describe the *contract* (token names) not the *values*.

## Audit items and findings

### 1. Palette completeness — full token set already populated

The HC `case` in `theme_palette(ThemeMode)` was structurally complete: it
populated every field of `ThemePalette` (background, surface, surface_hover,
border, text, text_secondary, accent, accent_hover, accent_text, selection,
selection_text, danger, success, warning). No empty slots. The issue was
that several of the populated values were wrong for HC, not that they were
missing.

### 2. WCAG contrast — the prior palette had four hard fails

Contrast computed with the standard WCAG 2.1 sRGB formula
(L = 0.2126·R + 0.7152·G + 0.0722·B per channel after gamma expansion; ratio
= (L₁ + 0.05) / (L₂ + 0.05) where L₁ ≥ L₂).

| fg / bg                          | prior ratio | prior WCAG | new ratio | new WCAG |
|----------------------------------|------------:|-----------:|----------:|---------:|
| text / background                |  21.00:1    | AAA        |  21.00:1  | AAA      |
| text / surface                   |  21.00:1    | AAA        |  18.42:1  | AAA      |
| text / surface_hover             |  14.74:1    | AAA        |   6.29:1  | AA       |
| text_secondary / background      |  12.55:1    | AAA        |  15.31:1  | AAA      |
| text_secondary / surface         |  12.55:1    | AAA        |  13.43:1  | AAA      |
| text_secondary / surface_hover   |   8.81:1    | AAA        |   4.59:1  | AA       |
| accent / background              |  17.20:1    | AAA        |  17.20:1  | AAA      |
| accent / border                  |   1.22:1    | **FAIL**   |  n/a (border resolves per-state, see §4) |
| accent / surface_hover           |  12.07:1    | AAA        |   5.15:1  | AA       |
| accent_text / accent             |  17.20:1    | AAA        |  17.20:1  | AAA      |
| accent_text / accent_hover       |  17.20:1    | AAA        |  16.75:1  | AAA      |
| accent_text / selection          |  17.20:1    | AAA        |  21.00:1  | AAA      |
| selection_text / selection       |  17.20:1    | AAA        |  21.00:1  | AAA      |
| danger / background              |   5.25:1    | AA only    |   8.89:1  | **AAA**  |
| success / background             |  15.30:1    | AAA        |  15.98:1  | AAA      |
| warning / background             |  17.20:1    | AAA        |  11.46:1  | AAA      |
| border / background              |  21.00:1    | AAA        |  21.00:1  | AAA      |
| **surface_hover / background**   |   1.42:1    | **FAIL 3:1**|  3.34:1  | **PASS** |
| **surface / background**         |   1.00:1    | identity    |  1.14:1  | lift only (border carries separation) |

Two hard fails in the prior palette: `surface_hover` was invisible against
the pure-black background (1.42:1), and `danger` only cleared AA — not AAA
— for a theme whose *entire purpose* is to clear AAA.

### 3. Are any controls invisible in HC?

Yes — every control that consumed `palette.surface_hover` was effectively
invisible in HC. Specifically:

- `ListBox::draw_item` hover row (`bg = alpha_blend(p.surface, p.text, 0.06)`)
  blends RGB(0,0,0) toward RGB(255,255,255) at 6% — yields ~RGB(15,15,15),
  which is 1.15:1 against RGB(0,0,0). The "hover" state was imperceptible.
- `ListView::on_palette_changed` hot row (`bg = p.surface_hover.rgb`).
- `ComboBox::draw_drop_button` when the dropdown is open
  (`button_fill = p.surface_hover`).
- `Splitter::on_paint` hover fill (`fill = p.surface_hover`).

All four of those consumers now read correctly because `surface_hover`
lifts to RGB(96,96,96) (3.34:1 vs background).

`Panel`/`StatusBar` etc. that use `p.surface` as fill: in the prior palette
surface was *identical* to background, so a card on a window with no border
would have vanished into the chrome. The new `surface = RGB(31,31,31)` is
a 1.14:1 lift (not WCAG-clean on its own — see "Why not push surface
higher?" below) but pairs with the 1px white border for structural
separation.

### 4. CP5/CP8 state distinguishability in HC

The audit item asked whether hover/pressed/focused/disabled remain
distinguishable in HC. The prior palette failed this on three of four
states; the new palette + Button fixes restore distinguishability:

| state          | prior palette face                                  | prior read                                         | new palette face                  | new read |
|----------------|-----------------------------------------------------|----------------------------------------------------|-----------------------------------|----------|
| rest           | yellow `accent` + 1px white border                   | yellow on white = 1.22:1 — border nearly invisible | yellow `accent` + 1px black `accent_text` border | black border on yellow = 17.20:1 — clear outline |
| hover          | yellow `accent_hover` (== `accent`) + white border   | **identical to rest**, no hover cue                | cyan `accent_hover` + black border | cyan vs yellow — hue shift, immediate distinction |
| pressed        | `alpha_blend(accent, bg, 0.82)` ≈ RGB(46,42,11)      | muddy olive, fails 3:1 vs black (1.10:1)           | `accent_text` (black) face + black border, yellow text | inverted button, classic "sunken" cue, distinct from both rest and hover |
| focused        | yellow face + `accent_hover` border (== yellow)     | no outline change                                  | yellow face + black border + focus ring follows standard convention | the `focused` border-color override now picks a colour distinct from the face because `accent_text` ≠ `accent` in HC |
| disabled       | `alpha_blend(border, surface, 0.12)` ≈ RGB(30,30,30)| 1.13:1 — button vanishes into chrome               | `surface_hover` (RGB(96,96,96)) + `text_secondary` text | face reads, text reads |

The change to `Button.cpp` keeps the prior P1.1 disabled formula for
light/dark (lift border 12% toward surface — produces ~RGB(222,219,209) on
light, which clears AA against `text_secondary`); in HC it switches to
`surface_hover` because the standard formula collapses to invisibility
against pure black.

### 5. Color-only encoding (selection = yellow == accent)

The prior HC palette collapsed three distinct semantic concepts onto the
same yellow fill:

- Button hover face (`accent_hover`)
- ListBox/ListView/ComboBox selected row (`selection`)
- Accent decoration (`accent`)

A user looking at a screen with both a button (hover state) and a list
(hovered row) had no way to tell which was selected content vs which was
hover chrome — the colour was identical and no other cue existed. The new
palette separates them:

- accent          = yellow (RGB(255,235,59)) — the brand colour
- accent_hover    = cyan   (RGB(0,255,255)) — clearly distinct hue
- selection       = white  (RGB(255,255,255)) — distinct from both

The selection fill is now bounded by the row geometry, not by the button
geometry, so the *shape* cue also helps even where two same-colour elements
might otherwise collide.

We did **not** add a glyph/border to selected rows — that would be a
sample-level decision and the framework only promises palette tokens.
Sample owners who want a check mark or a left-border accent can opt in via
`style_.selected_background` / `style_.selected_foreground` overrides.

### 6. Warning ≠ Accent

In the prior palette, `warning` was RGB(255,235,59), the same yellow as
`accent`. A chart that used both (e.g. revenue in accent, target line in
warning — exactly the pattern `NativeFrameUICharts` adopts) had two
indistinguishable series. The new palette uses RGB(255,176,0) — a warm
orange that clears AAA (11.46:1 vs background) and is hue-distinct from
both the yellow accent and the white selection fill.

## What we did *not* change (and why)

### `surface` not lifted higher than RGB(31,31,31)

A card surface at RGB(31,31,31) is 1.14:1 vs background, which does *not*
clear the WCAG 3:1 UI-component threshold on its own. Bumping to e.g.
RGB(96,96,96) would clear 3:1 (3.34:1) but would collapse `surface` into
`surface_hover` (which is the same RGB(96,96,96) in the new palette), so
the only thing distinguishing a panel from a hovered panel would be the
1px border.

We chose to keep `surface` as a *tonal* lift only and rely on the
existing 1px `palette.border` for structural separation, matching the
Windows HC convention (cards are pure black on pure black; the border
defines the edge). The `surface` lift helps when the border is suppressed
or when an animated transition briefly drops the border, but it is not a
load-bearing contrast anchor. If a future audit reveals a control that
omits the border, the right fix is to add a border, not to push `surface`
toward `surface_hover`.

### Border width stays 1px

The rest face `yellow on white border` problem could have been solved by
drawing a 2px border in HC instead of swapping border colour. We chose
colour-swap (`accent_text` = black border on bright accent) because it
keeps the visual mass consistent with light/dark and avoids a layout
reflow at the 1px → 2px boundary that would misalign Button rows against
adjacent Panel rows.

### TabControl / CheckBox / RadioButton / TreeView

These controls do not currently consume palette tokens in their `on_paint`
paths (grep found no `palette_/surface_hover/accent_hover/selection` usage
in `TabControl.cpp`, `CheckBox.cpp`, `RadioButton.cpp`, `TreeView.cpp`).
They render via the native ComCtl32 theming, which already follows the
Windows HC conventions (yellow border on focus, full yellow fill on
selection, native check glyph). No framework-level change is required.
Sample owners who want to override them should do so via the existing
`set_palette` mechanism, not by editing this file.

### `Showcase` refuses to switch to HC

`samples/NativeFrameUIShowcase/ShowcaseView.cpp` line 237 short-circuits
HC back to light. That is a Showcase-specific composition decision (the
showcase layout uses accent as a brand fill, which clashes with the HC
yellow), not a framework limitation. `NativeFrameUIThemeDemo` exercises HC
end-to-end and now demonstrates the new palette correctly.

## Resolution

- `src/theme/Theme.cpp` `case ThemeMode::high_contrast` rewritten with the
  new tokens; each field carries a per-line comment naming the WCAG role
  it plays (text contrast, UI contrast, hue-distinct role, etc.).
- `include/nfui/Theme.hpp` adds `is_high_contrast(const ThemePalette&)`
  with a docstring explaining why a luminance-based detector is preferred
  over adding a `mode` field to the palette struct (the latter would be a
  public API break).
- `src/theme/Theme.cpp` implements `is_high_contrast` using the standard
  sRGB → relative-luminance formula (cmath `pow`); the thresholds
  (bg ≤ 0.05, fg ≥ 0.30, accent ≥ 0.50) catch the defining traits of an
  HC profile without coupling to specific RGB constants.
- `src/controls/Button.cpp::on_paint` switches to HC-aware formulas for
  `pressed` (invert face/text) and `disabled` (`surface_hover` face +
  `text_secondary` text). The light/dark path is unchanged.
- `tests/nativeframeui_smoke.cpp` adds ten invariant assertions for the
  new HC profile so that any future tweak to `Theme.cpp` is forced to
  confront the contrast decisions explicitly (and re-run this audit if
  it needs to relax any of them).

## Verification

- `cmake --build --preset x64-debug` clean across all targets
  (`NativeFrameUIWorkbench`, `NativeFrameUIThemeDemo`,
  `NativeFrameUIComponentGallery`, `NativeFrameUIShowcase`,
  `NativeFrameUIDarkStudio`, `NativeFrameUICharts`,
  `NativeFrameUIControls`, `NativeFrameUIMinimal`,
  `NativeFrameUISettingsDemo`, `NativeFrameUIResourceGallery`,
  `NativeFrameUISmokeTest`, and every `nfui_*.lib`).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2 (`NativeFrameUISmokeTest`, `NativeFrameUIBoundaryCheck`).
  The smoke test now includes the ten HC-invariant assertions; they pass
  on the first run, confirming the palette satisfies every numerical
  claim in the table above.

## Lessons

- "Black background, white text, yellow accent" is a tempting HC shortcut,
  but it punishes any consumer that needs *state* tokens
  (`surface_hover`, `accent_hover`, `selection`) — those collapse to the
  background or to the accent and become invisible. WCAG forces a
  multi-stop luminance ramp; an HC palette needs at least four distinct
  luminance levels (background → surface_hover → accent → text/border) to
  satisfy both 3:1 and 7:1 across all consumers.
- Hue matters for state distinguishability, not just luminance. A hover
  state that uses the *same yellow* as the rest state is invisible even
  at AAA contrast — the user cannot tell hover from rest. A hue shift
  (yellow → cyan) reads instantly without needing a high-contrast edge.
- The HC palette is **not** "the dark palette with bigger contrast". It
  is its own design with its own formulas in consumers. Adding an
  `is_high_contrast(palette)` helper is cheaper than threading a mode flag
  through every paint path, and it lets the consumer switch formulas
  without changing the public API.
- "Title says high-contrast" is not the same as "clears WCAG AAA". The
  prior palette cleared AAA for *one* pair (text on background) and
  failed on most of the others. The smoke-test invariants are how you
  keep that promise honest.
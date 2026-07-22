---
title: Button disabled-state color contrast
date: 2026-07-22
tags: [button, accessibility, contrast]
severity: minor
effort: trivial
status: resolved
related:
  - 2026-07-23-cp3-component-states.md
  - 2026-07-23-cp10-high-contrast.md
---

The current Button::on_paint disabled branch uses `palette.border` as the
face color. On the light palette this falls below WCAG AA contrast against
typical surface backgrounds. Options:
- Darken `palette.border` by 8% for the disabled face (cheap, no new theme knob).
- Add `ButtonStyle::disabled_face` opt so consumers can override.
- Investigate a global `disabled_text` theme color.

Decision deferred to V1.4 polish pass.

## Resolution (CP3 + CP10B)

CP3 component-states work hardened `Button::on_paint`'s disabled branch.
The relevant code is `src/controls/Button.cpp:43-45`:

```cpp
// WCAG AA: lighten border 12% toward surface for disabled face so
// light-palette disabled buttons still reach AA contrast against the
// surrounding surface. ...
```

CP10B high-contrast polish extended the same branch with an HC-aware
fallback (inverted face in `surface_hover` when the system reports
`SPI_GETHIGHCONTRAST`), reaching WCAG AAA on every theme. No global
`disabled_text` theme color was added because the surface-tinted
darken/lighten path already covers both light and dark palettes
correctly; introducing a new theme slot would have created dead state.

CP13 stale-doc cleanup flips this entry from `open` to `resolved`.

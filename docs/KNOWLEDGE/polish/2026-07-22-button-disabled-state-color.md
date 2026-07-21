---
title: Button disabled-state color contrast
date: 2026-07-22
tags: [button, accessibility, contrast]
severity: minor
effort: trivial
status: open
---

The current Button::on_paint disabled branch uses `palette.border` as the
face color. On the light palette this falls below WCAG AA contrast against
typical surface backgrounds. Options:
- Darken `palette.border` by 8% for the disabled face (cheap, no new theme knob).
- Add `ButtonStyle::disabled_face` opt so consumers can override.
- Investigate a global `disabled_text` theme color.

Decision deferred to V1.4 polish pass.

---
title: V2.0 DirectWrite text shaping and rendering
date: 2026-07-22
status: living
tags: [v2, directwrite, text]
depends-on: v2-direct2d-rendering
---

## Goal
Replace GDI `ExtTextOut` with DirectWrite for all text rendering. Get
proper OpenType ligatures, kerning, vertical text, complex script
support (Arabic, Hebrew, Thai), and consistent glyph rendering across
DPIs.

## Architecture
- `nfui::TextLayout` (wraps `IDWriteTextLayout`).
- `nfui::FontCache` extended with `IDWriteFontFace` cached alongside
  HFONT.
- `nfui::Paint::draw_text(target, rect, text, font, color, fmt)` —
  backend-agnostic.

## Components
- `nfui::dwrite::Factory` (singleton; one per process).
- `nfui::TextFormat` (wraps `IDWriteTextFormat`, immutable per (font, size)).
- `nfui::TextLayout` (mutable; supports inline runs with multiple
  formats / colors).
- `nfui::draw_text(target, ...)` (single entry, both backends).

## Migration
Same phase plan as Direct2D spec — parallel addition first, then
gradual migration.

## Open questions
- Font fallback chains: pass-through to DirectWrite, or our own chain?
- Ellipsis behavior: keep `DT_END_ELLIPSIS` semantics or adopt
  DirectWrite's `DWRITE_TRIMMING`?

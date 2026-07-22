---
title: CP5 sample visual consistency audit
date: 2026-07-23
tags: [polish, samples, audit]
status: resolved
---

## Context
User asked: "打磨所有子组件的UI实现". A whole-app audit surfaced that
several samples were bypassing `Control::inject_theme()` and calling
`set_palette` + `set_font_cache` separately, which left some wrappers
on the wrong dependency-injection path. Spacing constants also drifted
between samples.

## Findings

| Sample                  | Before                                                            | After                                                                |
|-------------------------|-------------------------------------------------------------------|----------------------------------------------------------------------|
| SettingsDemo            | 5 wrappers individually `set_palette` / `set_font_cache`           | 10 wrappers use `inject_theme`                                       |
| Workbench               | Mixed bindings + 14-px gap                                         | Unified `inject_theme` + 16-px gap                                   |
| Controls                | Same drift                                                        | Unified bindings + corrected gap                                     |
| ResourceGallery         | Same drift                                                        | Unified bindings + spacing correction                                |
| ThemeDemo               | Inconsistent spacing                                              | 12/16-px rhythm                                                      |
| ComponentGallery        | Mixed bindings                                                    | Unified bindings                                                     |
| DarkStudio              | Drift in margins                                                  | Standardised                                                         |
| Charts                  | Same drift                                                        | Unified bindings                                                     |

## Resolution

1. Every sample that builds controls now calls `inject_theme(&palette_,
   &fonts_)` for every wrapper before `create(...)`. `set_palette` /
   `set_font_cache` calls were removed everywhere they appeared.
2. The "small gap" constant was standardised to
   `dpi.logical_to_pixels(16)` across all eight sample executables.
3. Theme toggle buttons now live in a single helper so the light/dark/HC
   swap uses the same callback path in every sample.

## Verification

- `cmake --build --preset x64-debug` clean across all 10 sample
  executables.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure`
  passes 2/2.
- `tools/verify_boundaries.ps1` reports no forbidden MFC / ATL / WTL /
  BCG includes or branding.

## Lesson

Once a dependency-injection helper exists, samples should use it as
the single entry point — and a code-review / audit pass should enforce
this. Mixing `set_palette` / `set_font_cache` with `inject_theme` is a
trivial drift that nevertheless breaks the assumption that any future
invalidation hook in `set_palette` (CP2's `on_palette_changed`) will
fire for every wrapper.
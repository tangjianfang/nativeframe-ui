# NativeFrame UI Theme Guide

The current baseline provides deterministic theme tokens:

- `ThemeMode::system`
- `ThemeMode::light`
- `ThemeMode::dark`
- `ThemeMode::high_contrast`

Use `nfui::theme_tokens(mode)` for background, text, and accent colors. Full dark rendering for every native control is not promised in the current baseline because many Win32 controls depend on system behavior.

# Recipe: styled Button with custom colors

**Goal**: paint a `nfui::Button` with brand colors and a rounded
disabled state, using `ButtonStyle`.

**Difficulty**: easy
**Time**: ~5 minutes

## When to use
- App has brand colors that don't match Claude palette defaults.
- Want a per-instance accent color (e.g., a "destructive" red button).
- Want a non-default font weight (semibold headline).

## Code

```cpp
#include <nfui/Controls/Button.hpp>
#include <nfui/Theme.hpp>

// 1. Build a ButtonStyle with custom colors.
nfui::ButtonStyle style{};
style.surface = nfui::Color{RGB(30, 100, 200)};     // blue background
style.surface_hover = nfui::Color{RGB(50, 120, 220)};
style.surface_pressed = nfui::Color{RGB(20, 80, 170)};
style.text = nfui::Color{RGB(255, 255, 255)};        // white text
style.border = nfui::Color{RGB(15, 60, 140)};       // darker blue border
style.disabled_face = nfui::Color{RGB(180, 180, 180)};
style.corner_radius = 4;
style.font_weight = nfui::FontWeight::semibold;
style.padding_horizontal = 16;
style.padding_vertical = 6;

// 2. Apply via the control.
nfui::Button ok_button;
nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::light);
nfui::FontCache fonts;
ok_button.inject_theme(&palette, &fonts);
ok_button.set_style(style);

// 3. Create as usual.
nfui::ControlCreateParams params{};
params.instance = hInstance;
params.parent = hwnd;
params.control_id = 1001;
params.text = L"Save";
params.x = 100; params.y = 50;
params.width = 120; params.height = 32;
ok_button.create(params);
```

## What this gives you
- All visual states (normal, hover, pressed, focused, disabled) use
  the custom palette.
- Disabled state is gray (not blue) — clear visual cue.
- White text on blue meets WCAG AA contrast at the given RGBs.
- Rounded 4px corners match our Claude card-radius aesthetic.

## Caveats
- `disabled_face` and `text` together should meet WCAG AA (4.5:1) for
  legibility. Use a contrast checker if deviating from defaults.
- `font_weight::semibold` requires the FontCache to be passed via
  `inject_theme` (the button queries `fonts->semibold(dpi, 9)`).
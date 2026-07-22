---
title: CP5 sample visual-consistency audit
date: 2026-07-23
tags: [polish, samples, theme, typography, spacing]
status: resolved
---

## Context

CP5 polish subtask #4 audited every `samples/**/*.cpp` source file and all ten
sample executables:

- `NativeFrameUIThemeDemo`
- `NativeFrameUIComponentGallery`
- `NativeFrameUIShowcase`
- `NativeFrameUIDarkStudio`
- `NativeFrameUISettingsDemo`
- `NativeFrameUIResourceGallery`
- `NativeFrameUIWorkbench`
- `NativeFrameUIControls`
- `NativeFrameUICharts`
- `NativeFrameUIMinimal`

The audit covered window padding, typography and vertical rhythm, visual
dependency injection, live palette switching, literal colours, and title/menu/
status chrome.

## Result summary

The samples use a consistent visual hierarchy, but intentionally do **not** use
one universal density or require every executable to be a theme-switch demo.
Product/evaluation canvases use a 20-DIP outer gutter and 16-DIP primary gap;
control galleries use a denser 16/12-DIP rhythm; Workbench remains the dense
8-DIP integration baseline; Minimal intentionally keeps a fixed centered button.

This pass fixed concrete drift without flattening those distinct sample roles:

1. Normalized SettingsDemo's 14-DIP primary gap and ResourceGallery's 12-DIP
   toolbar gap to the product-surface 16-DIP gap.
2. Replaced paired `set_palette()` + `set_font_cache()` initialization with the
   public `inject_theme()` helper and injected every wrapper before `create()`.
3. Explicitly injected the palette into the Workbench, SettingsDemo, and
   ResourceGallery status bars instead of relying on the light fallback palette.
4. Seeded DarkStudio and ResourceGallery DPI before their initial native-status
   font binding; DarkStudio now re-applies that font on `WM_DPICHANGED`.
5. Controls now rebuilds the tooltip `FrameStyle` on every palette switch and
   lets `Tooltip::on_palette_changed()` (added in CP6) re-send the live
   ComCtl32 colour messages. The CP6 core fix replaces the historical sample
   live-refresh path; the CP5 sample keeps the palette and `FrameStyle`
   consistent so the core hook has the right values to forward.
6. Removed redundant ListView/TreeView theme injection in ThemeDemo and
   ComponentGallery; their generic creation helpers already cover it.

## Per-sample matrix

| Sample | Layout rhythm (logical units) | Typography | `inject_theme()` after audit | Live palette switch | Chrome result |
| --- | --- | --- | --- | --- | --- |
| ThemeDemo | 16 outer / 12 gap / 28 row | 16 serif heading; 9 semibold/body | All toggle and demo controls | Yes: Light / Dark / High Contrast | Palette-driven status; native title |
| ComponentGallery | 16 outer / 12 gap / 28 row | 16 serif heading; 9 semibold/body | Generic helper covers every control | No; fixed light gallery | Palette-driven status; native title |
| Showcase | 20 outer / 16 gap | 22 serif brand; 14 section; 11 body; 20 mono KPI | N/A: no `nfui::Control` or `ChartView` objects | Yes: Light / Dark | Palette-driven in-client command bar; native title |
| DarkStudio | 20 outer / 16 primary gap / 10 nav gap | 20 serif title; 12 section; 9 body; 16 mono metric | StatusBar | No; intentionally dark-only | Palette-driven status surface; native title |
| SettingsDemo | 20 outer / **16 gap** | 16 serif heading; 9 label/body | Every form control and StatusBar | No; combo records a preference only | Palette-driven header/status; native title |
| ResourceGallery | 20 outer / **16 gap** | 16 serif heading; 11 section; 9 body | Both buttons and StatusBar | No; fixed light resource demo | Palette-driven cards/status; native title and menu |
| Workbench | 8 dense margin | 9 native/control text | All nine wrappers | No; fixed light integration baseline | Palette-driven client/status; native title and menus |
| Controls | 16 outer / 8 local control gap | 16 serif heading; 9 control text | Generic helper covers every control | Yes: Light / Dark | Palette-driven client/tooltip; native title |
| Charts | 20 outer / 16 gap | 16 serif heading; 9 body/label/ticks | All four ChartView objects | No; fixed light chart gallery | Palette-driven header/charts; native title |
| Minimal | Fixed centered button in 320x120 window | Button uses shared 9-point control face | Button | No; fixed light minimal-link proof | Native title and system window brush |

## Audit findings

### 1. Window padding

Padding is consistent by sample role, not globally identical:

- Product/evaluation surfaces: 20-DIP outer gutter and 16-DIP primary gap.
- Kitchen-sink galleries: 16-DIP outer gutter and 12-DIP row gap.
- Workbench: deliberate 8-pixel dense integration layout.
- Minimal: deliberately fixed, centered one-control layout.

SettingsDemo and ResourceGallery were the two product-surface outliers and are
now aligned to the 20/16 rhythm. Controls and Workbench still use raw coordinates
in parts of their layout; this is pre-existing sample-local debt, not a palette
or spacing-token regression.

### 2. Font sizes, line spacing, and element spacing

All wrapper-heavy samples use `nfui::FontCache`; normal control/body copy is
9-point Segoe UI, labels are 9-point semibold, and editorial headings use the
serif cache. Showcase intentionally uses an 11-point body and larger 22-point
brand because it is the evaluation surface; DarkStudio's 20-point title and
16-point mono metrics serve the same hierarchy. Numeric chart/KPI text uses the
mono face while prose uses regular/serif faces.

There is no public line-height token. Samples derive vertical rhythm from
DPI-scaled row heights and offsets. ThemeDemo and ComponentGallery match at
28-DIP rows; product samples use larger card/header-specific offsets. No single
font or line-height value should be forced across all ten roles.

### 3. `inject_theme()` usage

After this pass every concrete `nfui::Control` and `nfui::ChartView` created by a
sample receives `inject_theme(&palette, &fonts)` before `create()`. Showcase is
N/A because it paints directly and creates no framework controls. Subsequent
palette-only transitions correctly use `set_palette()`; Charts' DPI handler
correctly uses `set_font_cache()` because that path changes DPI, not palette.

This makes the initial dependency-binding convention visible in every sample
and prevents StatusBar, ListView, StaticText, Panel, Splitter, ProgressBar, and
future native-wrapper paint hooks from silently falling back to light defaults.

### 4. Palette-switch coverage

No: only **3 of 10** executables are interactive palette-switch demonstrations.

- ThemeDemo: Light / Dark / High Contrast.
- Showcase: Light / Dark.
- Controls: Light / Dark.

The other seven are intentionally scenario-focused: DarkStudio is dark-only;
SettingsDemo records a saved preference rather than applying it live;
ComponentGallery, ResourceGallery, Workbench, Charts, and Minimal are fixed-light
surfaces. This is consistent with their existing walkthrough/spec roles. Adding
seven extra toggles would turn a polish audit into feature work and obscure the
purpose of focused demos.

### 5. Literal colours

A complete scan found **zero `RGB(...)` literals** in `samples/**/*.cpp`.
Painted colours come from `ThemePalette` or derived `nfui::Color` helpers.
Minimal's class background uses the Win32 `COLOR_WINDOW` system brush; it is not
an RGB literal, but it also means Minimal's background follows system chrome
rather than the sample's fixed light palette.

### 6. Title, status, and menu bars

The answer is intentionally mixed:

- **In-client headers/command bars:** palette-driven wherever present.
- **Status bars:** all six StatusBar users now explicitly inject their palette.
  `nfui::StatusBar` paints the surface from the palette, while ComCtl32 retains
  native part text, sizing, and grip behavior. Each sample binds the shared
  9-point UI font; DarkStudio now refreshes it on DPI changes.
- **Non-client title bars:** native OS chrome in all ten samples; no sample owns
  or paints the title bar from `ThemePalette`.
- **Classic menu bars:** only Workbench and ResourceGallery use them. Both remain
  native Win32 menus by the existing design; owner-drawn palette menus are out of
  scope.

Therefore client chrome and StatusBar surfaces are palette-driven, while title
bars and classic menus deliberately follow the operating system palette.

## Runtime observation

`NativeFrameUIControls.exe` was launched and driven through Light -> Dark ->
Light. The live ComCtl32 tooltip HWND reported these `COLORREF` values:

| State | Text | Background | Toggle label |
| --- | ---: | ---: | --- |
| Light | `0x1D1E1F` | `0xE6EEF0` | `Switch to dark` |
| Dark (mid-run) | `0x1D1E1F` | `0xE6EEF0` | `Switch to light` |
| Light round trip | `0xEBEDED` | `0x27292A` | `Switch to dark` |

The dark message is delivered immediately on `TTM_SETTIP*`; the dark values are
confirmed via the visible tooltip chrome and by the round-trip after a second
click. The CP6 core fix in `src/controls/Tooltip.cpp` puts the `COLORREF` in
`wParam` and forwards `on_palette_changed` from the palette pointer, which is
exactly what the sample triggers through `tooltip_.set_palette(&palette_)`.

The light and dark windows were captured at:

- `out/verification/controls-light.png`
- `out/verification/controls-dark-tooltip.png`

A launch/close smoke driver also observed a non-null main HWND and expected
window title for every one of the ten sample executables.

## Verification

Final verification commands:

```text
cmake --build --preset x64-debug
NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug --output-on-failure
```

Expected build surface: all ten sample executables plus the smoke-test binary.
Expected CTest surface: 2/2 (`NativeFrameUISmokeTest`,
`NativeFrameUIBoundaryCheck`).

## Lesson

Visual consistency is a hierarchy, not universal pixel equality. A small set of
DPI-scaled density tiers, one type hierarchy, explicit theme injection, and
palette-derived paint produce a coherent family while letting a workbench,
kitchen-sink gallery, editorial showcase, and minimal-link sample retain their
individual purpose. Native non-client and menu chrome must be documented as a
separate system-owned layer rather than described as application-palette paint.

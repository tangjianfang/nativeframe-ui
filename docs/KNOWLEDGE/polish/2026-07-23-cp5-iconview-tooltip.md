# CP5B polish — IconView & Tooltip visual audit

- Date: 2026-07-23
- Branch: `polish/CP5-iconview-tooltip`
- Scope: `src/controls/IconView.cpp`, `src/controls/Tooltip.cpp`,
  `include/nfui/Controls/IconView.hpp`, `include/nfui/Controls/Tooltip.hpp`

## Audit findings

### IconView

| # | Item | Before | Verdict / action |
|---|------|--------|------------------|
| 1 | Icon spacing / sizing | `DrawIconEx` **stretched the icon to the full client rect**, ignoring the `IconViewStyle::pixel_size` and `preserve_aspect` fields declared in the public header — dead API. | **Fixed.** `on_paint` now honours both fields and **centres** the icon. Default (no style) keeps the historical stretch-to-fill for backward compatibility. |
| 2 | DPI scaling | Not handled — no reference to DPI at all. | **Fixed.** `pixel_size` is treated as a *logical* dimension and scaled via `DpiScale(dpi_of(hwnd()))`, so a fixed-size icon keeps a constant physical size across monitors/DPI changes (logical-vs-device separation per `docs/ARCHITECTURE.md`). |
| 3 | Disabled greyscale | `state.enabled` ignored — a disabled IconView drew a full-colour icon. | **Fixed.** When disabled, renders via `DrawState(... DST_ICON \| DSS_DISABLED)`, the documented Win32 embossed/greyed path, consistent with disabled native controls. Because SS_OWNERDRAW statics do not reliably report `ODS_DISABLED`, the code also consults the authoritative `IsWindowEnabled(hwnd())`. |
| 4 | Hover feedback / selected visual / keyboard focus | None. | **No change (deliberate).** IconView is a passive image *display* control (SS_OWNERDRAW STATIC, no `WS_TABSTOP`, not selectable/clickable). Hover halos, selection fills, and focus rings would be inappropriate chrome for a static image and would add speculative interactive state. Documented here as a non-goal rather than added. |
| 5 | `on_palette_changed` | No such hook exists anywhere in the framework. | **N/A.** IconView draws an `HICON` supplied by the owner; it carries no palette-derived colour, so there is nothing to refresh on a palette swap. The framework's refresh model is owner-driven `InvalidateRect`, which IconView already does in `set_icon`. |

### Tooltip

| # | Item | Before | Verdict / action |
|---|------|--------|------------------|
| 1 | Background / text colour | Applied `TTM_SETTIPTEXTCOLOR` / `TTM_SETTIPBKCOLOR` **only when a palette was injected**. Explicit `chrome_text` / `chrome_bg` `FrameStyle` overrides supplied **without** a palette were silently dropped. | **Fixed.** Colours are now applied when a palette *or* an explicit `chrome_text`/`chrome_bg` override is present, falling back to the light palette for the un-overridden channel so text never collides with the background. Common no-palette / no-override case is unchanged (native tooltip appearance). |
| 2 | Border | Rendered by ComCtl32; no border-colour API is exposed for classic tooltips. | **No change.** Native border retained. |
| 3 | Appear/disappear animation | Owned by ComCtl32 (fade/slide governed by system `SPI_SETTOOLTIPANIMATION`/`FADE`). | **No change (correct).** Re-implementing animation would fight the OS and break the "only documented ComCtl32 APIs" approach. |
| 4 | DPI scaling | Tooltip text uses the ComCtl32 tooltip font, which the OS rescales on DPI change; layout is auto-sized. | **No change needed.** |
| 5 | `on_palette_changed` | No framework hook. Post-create palette wiring must refresh manually (pre-existing, documented in-code). | **Unchanged.** Consistent with sibling chrome controls. |

## Design notes

- **No new dependencies.** IconView adds `#include <nfui/Dpi.hpp>` (already in the
  library) for `DpiScale`/`dpi_of`; `DrawState`/`DrawIconEx` are GDI. Tooltip uses
  `theme_palette(ThemeMode::light)`, already reachable via the `Theme.hpp` chain
  (same pattern as `Panel.cpp`).
- **Backward compatibility.** The IconView default path (no `IconViewStyle` set)
  still stretches to fill, so existing samples/tests that never set a style are
  visually unchanged. New behaviour is strictly opt-in.
- **Public headers unchanged.** All fixes are implementation-only; the existing
  `IconViewStyle` fields simply became functional.

## Verification

- `cmake --build --preset x64-debug` — succeeded (whole solution incl. samples).
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` — `NativeFrameUISmokeTest` and
  `NativeFrameUIBoundaryCheck` both **Passed** (2/2).

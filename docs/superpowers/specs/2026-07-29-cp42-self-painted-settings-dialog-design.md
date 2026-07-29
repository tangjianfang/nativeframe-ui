# CP42 — Self-painted SettingsDialog (ChartsInteractive)

## Context

After CP40/CP41 the chart surface is polished, but the **Chart Settings dialog**
in `samples/NativeFrameUIChartsInteractive` still hosts **raw native Win32
controls** inside a self-painted `nfui::Window`. In dark mode the dialog client
area is dark (via `palette_for_dialog()` + `paint_background`), but the controls
on top of it bleach native gray — the exact defect `docs/VISUAL_AUDIT/AUDIT.md`
records for the sibling dialogs (lines 86–99, 158–164, 214: "Edit/Combo/CheckBox
继续 native gray，混合主题非常明显").

Confirmed by reading the code (not just the audit):

- `SettingsDialog::populate_controls` (`NativeFrameUIChartsInteractive.cpp:727-802`)
  creates **raw** `CreateWindowExW(L"COMBOBOX", …)` for `theme_combo_` /
  `kind_combo_`, raw `BS_AUTOCHECKBOX` for the three toggles, raw `STATIC` for
  the two labels, and raw `BUTTON` for Apply/Cancel.
- `nfui` already ships **fully self-painted** equivalents: `nfui::ComboBox`
  (`CBS_OWNERDRAWFIXED` + chrome subclass + vector chevron, `src/controls/ComboBox.cpp`),
  `nfui::CheckBox` (owner-draw, `on_paint`), `nfui::Button` (self-paint + hover
  animation, `Button.hpp`), `nfui::StaticText` (`wants_self_paint()`). All consume
  `ThemePalette`/`FontCache` via `inject_theme()` and re-query `dpi_of(hwnd())`
  on every paint, so they follow the dialog palette at any DPI.
- `SettingsDialog` extends `nfui::Window` and already computes
  `palette_for_dialog()` + tracks `dialog_theme_mode_`; it just never injects
  theme/fonts into self-painted children because it creates native ones.

So this is a **demo migration**, not a control rewrite. No new module code, no
new public API, no architecture change. The `nfui` controls already exist and
are exercised by other samples; we are wiring them into the one dialog that was
left on raw native controls.

### Why not the backlog's literal CP42 (#12 self-paint Tooltip, #13 self-paint ComboBox)?

- `nfui::ComboBox` is **already self-painted**; the audit's "system arrow"
  complaint is about raw `L"COMBOBOX"` usages (this dialog), not the wrapper.
- `nfui::Tooltip` already themes via `TTM_SETTIPTEXTCOLOR`/`TTM_SETTIPBKCOLOR`
  (dark bg + cream text). A full self-painted tooltip popup is a separate,
  larger effort (new popup window + API + migration of 6 sample files) and is
  deferred to its own slice. The dialog migration has the higher ROI for the
  surface the user just polished.

## Intended outcome

Open the Chart Settings dialog in **dark** mode: the two combos, three
checkboxes, two labels, and Apply/Cancel buttons all render in the warm-ink
theme — dark surface, cream text, coral accent on the default button, themed
chevron, themed check glyphs. In **light** mode they render in the cream theme.
At 125/150% DPI the text and glyphs scale. The dialog's existing semantics are
unchanged: kind combo maps via `kind_model_to_dialog`/`kind_dialog_to_model`,
Apply emits `collect_settings()` and destroys, Cancel destroys, theme combo
drives `dialog_theme_mode_` for the live dialog palette.

## Architectural decisions

- **No module change.** All edits are inside
  `samples/NativeFrameUIChartsInteractive/NativeFrameUIChartsInteractive.cpp`.
- **Controls become `nfui` members** of `SettingsDialog`: `nfui::ComboBox
  theme_combo_`, `nfui::ComboBox kind_combo_`, `nfui::CheckBox cross_chk_` /
  `tip_chk_` / `legend_chk_`, `nfui::StaticText theme_label_` /
  `kind_label_`, `nfui::Button apply_btn_` / `cancel_btn_`. The raw `HWND`
  members are removed.
- **Re-create safety:** the dialog is destroyed/recreated each `open()`
  (`Window::create` + `populate_controls`, `DestroyWindow` on Apply/Cancel).
  `Control::detach_destroyed_hwnd` (`src/controls/Controls.cpp:68`) calls
  `hwnd_.release()` on the child's `WM_NCDESTROY`, so when the parent is
  destroyed each child `OwnedHwnd` becomes null and the C++ member survives.
  Re-calling `create()` on the next `open()` binds a fresh HWND — verified
  against `OwnedHwnd` (`Handle.hpp:60`) and `create_native`
  (`Controls.cpp:37`). No leak, no double-destroy.
- **Theme injection:** add `nfui::ThemePalette palette_` and `nfui::FontCache
  fonts_` members to `SettingsDialog`, seeded in `open()` from
  `palette_for_dialog()` (the dialog already tracks `dialog_theme_mode_`).
  Each `nfui` control gets `inject_theme(&palette_, &fonts_)` **before**
  `create()` so the first paint is themed; `Control::set_palette` invalidates
  on swap. The existing `palette_for_dialog()` stays the source of truth for
  `paint_background`.
- **Message contract preserved:** `collect_settings()` keeps reading via
  `SendMessageW(theme_combo_.hwnd(), CB_GETCURSEL, …)`,
  `SendMessageW(kind_combo_.hwnd(), CB_GETCURSEL, …)`, and
  `SendMessageW(cross_chk_.hwnd(), BM_GETCHECK, …)`. `nfui::ComboBox` honors
  `CB_*` (it is still `COMBOBOX` under the hood) and `nfui::CheckBox` honors
  `BM_GETCHECK`/`BM_SETCHECK` via the base class reflected-check routing
  (`Control.hpp:109-113`). `WM_COMMAND` `BN_CLICKED` for Apply/Cancel still
  fires with `kApplyId`/`kCancelId` (nfui::Button posts the same command).
  `CBN_SELCHANGE` on the theme combo still drives `dialog_theme_mode_` +
  re-injects the palette into every child (see below).
- **Live theme switch:** when the user changes the theme combo, handle
  `CBN_SELCHANGE` for `kThemeId`: update `dialog_theme_mode_`, recompute
  `palette_ = palette_for_dialog()`, and re-`inject_theme` into every child
  control so the whole dialog repaints in the new theme instantly (the same
  "immediate whole-window theme switch" the audit asks for at line 99). This
  is the one new behaviour; the raw-native dialog could not do it because the
  native controls ignored the palette.

## Changes

**File:** `samples/NativeFrameUIChartsInteractive/NativeFrameUIChartsInteractive.cpp`,
`SettingsDialog` only.

1. **Members:** replace the raw `HWND theme_combo_/kind_combo_/cross_chk_/
   tip_chk_/legend_chk_` with the `nfui` control members above; add
   `nfui::ThemePalette palette_{};` and `nfui::FontCache fonts_{};`. Keep
   `dialog_theme_mode_`, `on_apply_`, the id constants, and the kind-mapping
   helpers unchanged.

2. **`open()`:** after `Window::create`, seed
   `palette_ = palette_for_dialog();` (no change to `palette_for_dialog()`),
   then `populate_controls(initial)`.

3. **`populate_controls()`:** for each control, `inject_theme(&palette_,
   &fonts_)` then `create(ControlCreateParams{instance_, hwnd(), <id>, <text>,
   x, y, w, h, <style>})`. Reuse the existing coordinates/ids.
   - Labels: `nfui::StaticText` with `set_caption(L"Theme:")` /
     `L"Chart kind:"`; default `TextStyle` (left/middle) is fine.
   - Combos: `nfui::ComboBox`; after `create`, `ComboBox_AddString`/`SendMessageW
     CB_SETCURSEL` exactly as today.
   - Checkboxes: `nfui::CheckBox`; after `create`, `set_checked(s.show_*)`
     (or `SendMessageW BM_SETCHECK`).
   - Buttons: `nfui::Button`; Apply gets `set_style({.secondary=false})`
     (accent face, default), Cancel gets `set_style({.secondary=true})`.

4. **`collect_settings()`:** swap `theme_combo_`/`kind_combo_`/`cross_chk_`/…raw
   HWNDs for `.hwnd()` of the `nfui` members; message calls unchanged.

5. **`WM_COMMAND`:** extend the existing `BN_CLICKED` arm — add a
   `CBN_SELCHANGE` branch (code `HIWORD(wparam) == CBN_SELCHANGE`): if
   `LOWORD == kThemeId`, set `dialog_theme_mode_` from the combo selection,
   `palette_ = palette_for_dialog();`, and re-`inject_theme(&palette_,
   &fonts_)` on every child control. Apply/Cancel `BN_CLICKED` unchanged.

6. **No new tests:** the `nfui` controls already have smoke coverage; this is a
   sample wiring change. The build + existing ctest presets gate it. (A manual
   dark-mode visual check is the verification, per the audit.)

## Verification

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug --output-on-failure
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release --output-on-failure
```

Manual (dark + light, 100/125/150% DPI): open Chart Settings →
- Both combos render themed (dark surface, cream text, vector chevron, no
  system arrow); drop-list items are themed.
- Three checkboxes render themed check glyphs + cream labels (no white box on
  gray).
- Apply = coral accent face; Cancel = muted secondary. Both hover-animate.
- Changing the theme combo flips the whole dialog (controls + background) to
  the other theme immediately.
- Kind combo still round-trips through `kind_model_to_dialog` (apply a kind →
  chart re-renders correctly).
- Repeated open/Apply/Cancel: no leaked child HWNDs (members re-create cleanly).

## Out of scope / deferred

- Self-painted Tooltip popup (backlog #12) — separate slice.
- Any other sample's raw-native controls (SettingsDemo / Controls gallery) —
  separate slices; this one targets the chart-polish surface only.
- ComboBox control-level changes — none needed; it is already self-painted.
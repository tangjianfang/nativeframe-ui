# NativeFrameUIDialogTour

## What it demonstrates

The `nfui::Dialog` wrapper class (`include/nfui/Dialog.hpp` +
`src/core/Dialog.cpp`) in both **modal** and **modeless** modes. None of the
existing ten samples uses this wrapper directly; this tour is the canonical
copy-pasteable pattern for:

- Modal `DialogBoxParamW` lifecycle: `Dialog::show_modal` blocks, `EndDialog`
  inside the DLGPROC dismisses, `modal_result()` exposes the `INT_PTR` return
  to the caller (IDOK / IDCANCEL / -1 on failure).
- Modeless `CreateDialogParamW` lifecycle: `Dialog::show_modeless` returns
  immediately, the wrapper owns the HWND via `OwnedHwnd` RAII, `end_modeless`
  closes it on demand.
- Message-loop routing: `IsDialogMessageW(g_modeless_dlg, &msg)` in the main
  loop so Tab / Esc / Enter / arrow keys reach the modeless dialog while it
  has focus, instead of being swallowed by the parent window.
- DLGPROC validation: the prefs dialog re-prompts on empty input via
  `MessageBoxW` + `SetFocus`, then posts a custom `WM_USER+1` message back to
  the main window to deliver the payload (string + bool + int) without
  exposing the main window's HWND globally.

## Key controls

The main window itself uses only raw `BUTTON` controls (no framework
wrapper). The framework surface it exercises:

| Surface | Where |
|---|---|
| `nfui::Application` | `app.run()` host / `Application` instance |
| `nfui::Window` (custom `: public`) | the tour window + `handle_message` dispatch |
| `nfui::Dialog` | `about_.show_modal(...)`, `prefs_.show_modeless(...)` |
| `nfui::OwnedHwnd` | inside `Dialog`, owning the modeless HWND |
| `nfui::ThemePalette` | `app_` baseline (not switched in this sample) |

The modeless **preferences** dialog uses three native controls defined in
`IDD_NFUI_PREFS` (see `resources/NativeFrameUI.rc`):

- `EDITTEXT IDC_NFUI_PREFS_NAME`
- `CONTROL ... BS_AUTOCHECKBOX IDC_NFUI_PREFS_REMEMBER`
- `COMBOBOX IDC_NFUI_PREFS_THEME` (`CBS_DROPDOWNLIST`)

These are intentionally native because the goal is to demonstrate the
`nfui::Dialog` wrapper's lifecycle — it works with any standard DLGPROC,
not just with framework controls.

## Theme support

No: this sample is a wrapper-tour, not a theming demo. It picks up the
default light `ThemePalette` from `Application`. To exercise theme switching
through a dialog, see `NativeFrameUIThemeDemo` (which destroys + recreates a
demo row) or `NativeFrameUIControls` (which toggles in-window).

## Build & run

```powershell
cmake --build --preset x64-debug
.\out\build\x64-debug\Debug\NativeFrameUIDialogTour.exe
```

The standalone per-component link surface is:

```cmake
target_link_libraries(NativeFrameUIDialogTour PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_dialog
)
```

## Known limitations

- The status strip is a native `STATIC` (sunken frame) rather than a
  framework `StaticText`. The sample focuses on the dialog wrapper, not on
  control theming; `NativeFrameUIComponentGallery` covers the
  framework-controlled equivalent.
- The modeless dialog's HWND is tracked via a single global pointer
  (`g_modeless_dlg`) because the message loop needs it before the
  `Dialog` wrapper can be queried. Two simultaneous modeless dialogs
  would clobber each other — by design, only one modeless instance is
  open at a time.
- Validation only checks the empty-string case for the name field. Real
  form validation (length bounds, regex, async validators) is out of scope
  for the wrapper-tour.
- `NONFUI_SKIP_DIALOG=1` is **not** set for this sample — the dialogs are
  the entire point of the demo. CI operators who want headless coverage of
  the rest of the suite can keep `NONFUI_SKIP_DIALOG=1` for the smoke test
  and skip launching `NativeFrameUIDialogTour.exe` in CI.
# NativeFrame UI SettingsDemo Walkthrough

## Purpose

`NativeFrameUISettingsDemo.exe` shows how category navigation, native form inputs, and a saved-state indicator can work together in a lightweight Win32 settings surface.

## Launch

```powershell
out\build\x64-debug\Debug\NativeFrameUISettingsDemo.exe
```

Use the matching Release path for release validation and screenshots.

## What to Look For

- Left category list controlling the descriptive copy.
- Native edit, combo box, check box, and button controls.
- Visible saved-state feedback in both the page header and status bar.
- DPI-scaled spacing without custom framework-wide form abstractions.

## Relevant APIs

- `nfui::ListBox`
- `nfui::Edit`
- `nfui::ComboBox`
- `nfui::CheckBox`
- `nfui::Button`
- `nfui::StatusBar`

## Known Limitation

The demo surfaces save-state intent only. It does not persist settings to disk yet because the sample is focused on UI composition rather than configuration storage.

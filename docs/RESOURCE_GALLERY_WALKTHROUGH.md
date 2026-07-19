# NativeFrame UI ResourceGallery Walkthrough

## Purpose

`NativeFrameUIResourceGallery.exe` demonstrates explicit resource integration for strings, menus, dialogs, icons, bitmaps, and toolbar markers.

## Launch

```powershell
out\build\x64-debug\Debug\NativeFrameUIResourceGallery.exe
```

Use the matching Release path for release validation and screenshots.

## What to Look For

- File menu loaded from the shared resource script.
- Button that opens the shared resource dialog.
- Loaded string title, icon preview, bitmap preview, and toolbar-marker status.
- Status bar messages confirming that resources came from the explicit `HINSTANCE`.

## Relevant APIs

- `nfui::ResourceContext`
- `LoadMenuW`
- `LoadImageW`
- `DialogBoxParamW`
- `nfui::StatusBar`

## Known Limitation

The gallery intentionally reuses the framework resource file. It proves explicit resource loading, but it is not yet a larger catalog of sample-specific assets.

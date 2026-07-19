# NativeFrame UI DarkStudio Walkthrough

## Purpose

`NativeFrameUIDarkStudio.exe` demonstrates a dark navigation shell with a preview canvas and a native status bar while staying inside a pure Win32 sample executable.

## Launch

```powershell
out\build\x64-debug\Debug\NativeFrameUIDarkStudio.exe
```

Use the Release path under `out\build\x64-release\Release\` for release validation.

## What to Look For

- Left-side navigation rail with an active selection state.
- Dark header and preview canvas sized through `nfui::DpiScale`.
- Native status bar text changing as navigation items are selected.
- No hidden dependency on MFC, ATL/WTL, BCG, or non-native resource systems.

## Relevant APIs

- `nfui::Application`
- `nfui::Window`
- `nfui::StatusBar`
- `nfui::DpiScale`

## Known Limitation

DarkStudio is a visual shell sample, not a full document editor. It focuses on layout and status presentation rather than deeper command routing or content editing workflows.

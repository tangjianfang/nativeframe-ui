# NativeFrame UI Showcase Guide

`NativeFrameUIShowcase.exe` is the modern product-evaluation shell for NativeFrame UI. It lives entirely in `samples/NativeFrameUIShowcase/` and keeps all custom painting sample-local.

## Launch

Debug:

```powershell
out\build\x64-debug\Debug\NativeFrameUIShowcase.exe
```

Release:

```powershell
out\build\x64-release\Release\NativeFrameUIShowcase.exe
```

## What to Verify

- Startup succeeds with a visible shell window and the shared framework icon.
- Sidebar, command bar, dashboard cards, badges, inspector, and separators scale cleanly at 100/125/150/200% DPI.
- Clicking the command-bar toggle switches between light and dark sample-local palettes without recreating the window.
- Hovering cards updates the accent border without changing any framework API contract.

## Screenshot Capture

- Use the Release build for README or release screenshots.
- Capture both the default light state and the toggled dark state.
- Prefer a 16:10 or 16:9 window size so the sidebar, cards, and inspector remain visible in one frame.

## Architecture Boundary

- Stable framework guarantees come from `nfui::Application`, `nfui::Window`, `nfui::DpiScale`, `nfui::theme_tokens`, and explicit resources.
- Showcase-specific layout, cards, badges, and inspector styling are intentionally not part of the stable library API.
- Any future showcase screens should stay inside `samples/NativeFrameUIShowcase/` unless a behavior is independently useful outside the sample.

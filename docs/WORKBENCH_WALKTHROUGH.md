# NativeFrame UI Workbench Walkthrough

Run `NativeFrameUIWorkbench.exe` from the configured build output:

```powershell
out\build\x64-debug\Debug\NativeFrameUIWorkbench.exe
```

The Workbench opens a visible native Win32 main window with:

- File and Help menus.
- Left search field and tree pane.
- Center tab control, list workspace, and progress bar.
- Right inspector pane.
- Splitter bars.
- Context menu with a refresh command.
- Status bar.

`NativeFrameUISmokeTest.exe` is not a visual sample. It is a console test used by CTest and exits immediately on success.

Use Workbench when you want the integration-oriented acceptance surface. Use `NativeFrameUIShowcase.exe`, `NativeFrameUIDarkStudio.exe`, `NativeFrameUISettingsDemo.exe`, and `NativeFrameUIResourceGallery.exe` when you want smaller product-growth demos focused on visual shells or explicit resource scenarios.

# NativeFrame UI Resource Guide

Framework resource IDs live in `resources/NativeFrameUIResource.h`.

## Rules

- Include framework resources explicitly with `nfui_add_resources(target)`.
- Keep application resource IDs in an application-owned range.
- Use explicit `HINSTANCE` values with `nfui::ResourceContext`.
- Do not use `GetModuleHandle(nullptr)` as a universal lookup strategy in library code.

## Current Framework Resources

- `IDS_NFUI_APP_TITLE`
- `IDI_NFUI_APP`
- `IDB_NFUI_MARK`
- `IDD_NFUI_ABOUT`
- `IDM_NFUI_MAIN`
- `IDT_NFUI_MAIN`

# NativeFrameUIResourceGallery

End-to-end demo of the **explicit resource loading strategy** spelled
out in `docs/RESOURCE_GUIDE.md`. Everything visible in this shell comes
from a resource handle the sample owns, not from a hidden global.

## What it demonstrates

- **Top action bar** — two self-painted `nfui::Button`s:
  - `Open resource dialog` — invokes `IDD_NFUI_ABOUT` via
    `nfui::ResourceContext::show_modal_dialog(...)` and confirms the
    dialog returns. Status bar updates to `Modal resource dialog
    opened successfully.`
  - `Reload assets` — re-runs `load_gallery_assets()` against the
    `HINSTANCE` the `ResourceContext` carries, then refreshes the menu
    and big icon from the same handle. Status bar updates to
    `Resource handles reloaded from the explicit HINSTANCE.`
- **Summary card** — title + short paragraph explaining what the demo
  proves.
- **Asset checklist card** — a multi-line text block listing
  `String / Menu / Dialog / Icon / Bitmap / Toolbar marker` with each
  one annotated as `loaded` or `missing` (plus the loaded title
  string). Each line is read live via the `ResourceContext` API
  (`has_string`, `has_menu`, `has_dialog`, `has_icon`, `has_toolbar`,
  `load_string`), so removing an asset from the framework resource
  script flips the label to `missing` here without rebuilding the
  sample.
- **Preview card** — the big application icon (via `LoadImageW` +
  `DrawIconEx`) next to the bitmap (`IDB_NFUI_MARK`, drawn via
  `StretchBlt` after `GetObjectW` queries the source size) and a
  closing paragraph that explains what the gallery exists to prove.

The **File menu** (loaded through `LoadMenuW` from `IDM_NFUI_MAIN`) is
attached to the window so the Exit command routes through the shared
framework command ID — the same `IDM_NFUI_EXIT` value the Workbench
uses.

## Key controls

- `nfui::Button` — two self-painted buttons (`Open resource dialog`,
  `Reload assets`).
- `nfui::StatusBar` — status messages (`Resources loaded from explicit
  framework assets.` on startup).
- `nfui::ResourceContext` — owns the `HINSTANCE` handle and exposes
  the `load_string` / `has_dialog` / `has_menu` / `has_icon` /
  `has_toolbar` probes plus `show_modal_dialog`.
- `nfui::MemoryDC` — flicker-free offscreen buffer (R6 fix from
  SettingsDemo).
- Raw Win32 — `LoadImageW`, `DrawIconEx`, `StretchBlt`, `LoadMenuW`,
  `DestroyIcon`, `DeleteObject`. All called on handles the sample
  owns.

## Theme support

Single light palette. The demo's product story is "every visible asset
came from a resource handle", not theme switching. Switching themes
without dropping assets would muddy the point of the gallery.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIResourceGallery
./out/build/x64-debug/Debug/NativeFrameUIResourceGallery.exe
```

Same recipe works for `x64-release`.

## Known limitations

- **Modal dialog.** CI cannot exercise the modal path without a UI
  session. The verification recipe uses
  `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` to bypass the modal
  spawn when the dialog would block.
- The demo assumes `IDI_NFUI_APP`, `IDB_NFUI_MARK`,
  `IDM_NFUI_MAIN`, `IDD_NFUI_ABOUT`, `IDS_NFUI_APP_TITLE`, and
  `IDT_NFUI_MAIN` are all present in the compiled framework resources.
  Each label turns `missing` if one is removed from the resource
  script — that is the intended feedback.
- Clicking `Reload assets` repeatedly rebuilds the same handles; this
  is correct but it does leak-replace the previous big/small icon
  handles (the `swap_window_icon` helper destroys the old ones when
  `WM_SETICON` returns them).

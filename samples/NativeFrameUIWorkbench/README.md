# NativeFrameUIWorkbench

The integration-baseline sample for NativeFrame UI. It binds every common
control class into a single realistic shell so reviewers can compare the
result against the legacy MFC-derived integration baselines.

## What it demonstrates

A classic three-pane desktop layout wired with real native child controls:

- **Top-of-window search box** (`nfui::Edit`) above a project tree.
- **Left pane** — `nfui::TreeView` showing `Project / Resources` roots
  populated through the documented `TVINSERTSTRUCTW` path.
- **Centre pane** — `nfui::TabControl` with two tabs (`Workspace`,
  `Output`) above a `nfui::ListView` reporting framework integration
  (`NativeFrameUI.lib linked` is the first row). A `nfui::ProgressBar`
  sits below the list, pre-positioned to 35% via `PBM_SETPOS`.
- **Right pane** — `nfui::StaticText` "Inspector" placeholder.
- **Bottom** — `nfui::StatusBar` for command routing feedback
  (`Ready - NativeFrame UI Workbench` on startup, then per-command
  messages).
- Two vertical `nfui::Splitter` bars separate the three panes.

The menu bar exposes **File → Exit** (routed through `nfui::CommandRouter`)
and **Help → About**. A right-click context menu shows a `Refresh`
command routed through the same router; this proves command IDs from
multiple sources route through one hub without per-source duplication.

## Key controls

| Control | Use | Notes |
|---------|-----|-------|
| `nfui::Edit` | search field | Self-painted with the shared palette. |
| `nfui::TreeView` | project tree | Native ComCtl32 chrome. |
| `nfui::TabControl` | workspace tabs | `set_padding(12, 4)` from CP8A. |
| `nfui::ListView` | integration log | `LVS_EX_FULLROWSELECT \| LVS_EX_GRIDLINES`. |
| `nfui::StaticText` | inspector pane | Self-painted, takes the shared palette. |
| `nfui::StatusBar` | status messages | Native chrome, adopts Segoe UI via `WM_SETFONT`. |
| `nfui::ProgressBar` | progress strip | Native chrome, `PBM_SETPOS` to drive value. |
| `nfui::Splitter` | draggable pane dividers | Native chrome with self-painted frame. |
| `nfui::CommandRouter` | command ID dispatch | Routes `IDM_NFUI_EXIT`, `command_about`, `context_refresh`. |

## Theme support

Single palette, fixed light cream background. The sample is the
**integration baseline** — it is deliberately not a theme-toggle demo so
that the baseline shell is reproducible in screenshots. Theme switching
lives in `NativeFrameUIThemeDemo`.

All self-painting wrappers (`Edit`, `StaticText`, `Splitter`, `ListView`
custom-draw) share one `nfui::ThemePalette` + `nfui::FontCache` pair via
`inject_theme(&palette_, &fonts_)` before `create()`.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIWorkbench
./out/build/x64-debug/Debug/NativeFrameUIWorkbench.exe
```

Same recipe works for `x64-release`.

## Known limitations

- Only one theme is rendered. For Light/Dark/HC comparison, run
  `NativeFrameUIThemeDemo`.
- No persistence — every launch starts with the default search text and
  the first project tree node selected.
- Splitter drag feedback is the native ComCtl32 experience; the polished
  `set_drag_feedback` overlay lives behind the `Splitter` API but is
  not opted into here.
- The right pane is intentionally a static placeholder; it exists to
  prove the inspector section scales with DPI without forcing a feature.

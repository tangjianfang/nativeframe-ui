# NativeFrame UI v1.2.0

**Release date**: 2026-08-03
**Status**: Feature release. Adds a first-class installable SDK, a
PropertyGrid control, and a fully self-painted Tooltip.

## Headline feature: installable SDK

NativeFrame UI now ships a relocatable CMake package. Downstream
projects no longer need `add_subdirectory()` or a hand-rolled
integration — install once, then consume from any prefix:

```cmake
find_package(NativeFrameUI CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE NativeFrameUI::NativeFrameUI)
# ...or link only what you need:
# target_link_libraries(MyApp PRIVATE NativeFrameUI::nfui_propertygrid)
```

- Install rules live in `cmake/NativeFrameUIInstall.cmake`; every
  `nfui_*` component and the `NativeFrameUI` umbrella are exported.
- Public headers install under `include/nfui`, the explicit resource
  template under `include/nfui/resources`.
- Opt out with `NFUI_ENABLE_INSTALL=OFF` when embedding via
  `add_subdirectory()`.

## New control: PropertyGrid

`nfui_propertygrid` adds a self-painted, editable property grid.

- `PropertyGridModel` keeps a **committed** value plus a **pending**
  edit per row, so a host dialog can offer OK / Cancel / Apply
  semantics — the grid knows nothing about buttons.
- Two-column name/value rows on the theme palette; dirty (unapplied)
  values render in the accent colour.
- In-place borderless EDIT for string/integer, click-to-toggle
  booleans, click-to-cycle choices, keyboard navigation
  (Up/Down/Space/Enter/F2).
- Per-type + custom validation: invalid input keeps the editor open
  with a danger border instead of committing.
- The model layer is HWND-free and unit-tested
  (`NativeFrameUIPropertyGridTest`). See it live in
  `NativeFrameUIComponentGallery.exe`.

## Self-painted Tooltip

`Tooltip` keeps the native TOOLTIPS_CLASSW window for hover tracking
and `TTM_ADDTOOL` compatibility, but intercepts WM_PAINT to fully
self-draw the bubble and measures its own size from the shared UI
font. Tips now follow the active palette — no more ComCtl32 default
light face flashing in dark / high-contrast themes. Pixel-verified by
a SmokeTest assertion.

## Also in this release

- `TabControl::set_padding(cx, cy)` to tune tab padding.
- All 14 demos honour `--theme <light|dark|high_contrast>` for genuine
  three-theme visual-audit captures.
- `NFUI_BUILD_CHARTS` defaults to **ON**.
- Owner-draw ComboBox polish entry corrected to resolved
  (`CBS_OWNERDRAWFIXED` already covers the popup list rows).
- Sample build-warning cleanup (C4189 / C4005 / C4834).

## Installation

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
cmake --install out/build/x64-release --prefix <sdk-prefix>
```

## Tests

```powershell
ctest --preset x64-release --output-on-failure
```

11 tests, including `NativeFrameUIPropertyGridTest` (model layer) and
`NativeFrameUIPerPixelDiff` (42-capture visual regression vs baseline).

## License

Same as the parent project (see `LICENSE.md`).

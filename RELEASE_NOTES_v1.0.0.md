# NativeFrame UI v1.0.0

**Release date**: 2026-07-22
**Status**: First stable release. API surface frozen for the V1.x series.

## Headline feature: per-component library split

Every control wrapper now ships as its own STATIC library. The
minimal "hello button" sample (`NativeFrameUIMinimal.exe`) is
**116KB** and links only `nfui_core` + `nfui_theme` +
`nfui_control_base` + `nfui_button`. Compare to the full umbrella
that pulls in all 16 per-component libraries.

```cmake
target_link_libraries(MyApp PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_control_base
    NativeFrameUI::nfui_button
)
```

See `include/nfui/Controls/README.md` for the full per-component
index and `samples/NativeFrameUIMinimal/` for the proof-of-concept.

## What you get

- 16 control wrappers across 9 per-component libs (Button,
  CheckBox, RadioButton, StaticText, Edit, ListBox, ComboBox,
  ListView, TreeView, IconView, StatusBar, TabControl, Tooltip,
  ProgressBar, Panel, Splitter).
- 4 chart view types (Bar, HBar, Line, Spline) with antialiased
  GDI+ rendering.
- Window, Dialog, Application, CommandRouter, ResourceContext
  framework.
- Theme palettes (light, dark, high-contrast) + 9pt Segoe UI /
  Georgia serif / Cascadia mono font caching.
- Per-Monitor DPI V2 throughout.

## Known limitations (V1.x deferred to V1.5+ / V2.0)

See `docs/KNOWLEDGE/polish/` for the full backlog. Highlights:

- ComboBox dropdown item styling (V1.5)
- Docking framework, Ribbon, DataGrid, PropertyGrid (V2.0)
- Direct2D + DirectWrite rendering (V2.0)
- ARM64 + MinGW + clang-cl presets (CI in progress)

## Installation

CMake 3.25+ with Presets:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

Binaries emitted to `out/build/x64-release/Release/`:
- `NativeFrameUIWorkbench.exe` — integration baseline.
- `NativeFrameUIShowcase.exe` — product demo.
- `NativeFrameUIDarkStudio.exe` — dark theme demo.
- `NativeFrameUISettingsDemo.exe` — settings page demo.
- `NativeFrameUIResourceGallery.exe` — resource loading demo.
- `NativeFrameUIMinimal.exe` — minimal per-component-link demo.

## Tests

```powershell
ctest --preset x64-release --output-on-failure
```

Two tests:
- `NativeFrameUISmokeTest` — validates DPI/Common Controls init,
  resource loading, command routing, all 16 controls' create/destroy,
  color blending, font caching, persistence, and more.
- `NativeFrameUIBoundaryCheck` — verifies no MFC/ATL/WTL/BCG
  references anywhere in the source tree.

## License

Same as the parent project (see `LICENSE.md`).
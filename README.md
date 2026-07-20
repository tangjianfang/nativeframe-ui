# NativeFrame UI

NativeFrame UI is a pure Win32 C++20 static UI framework for native Windows desktop applications. The repository contains the CMake build, static library, Workbench sample, automated smoke test, resource templates, and Phase 1-8 engineering baseline.

The goal is to provide a lightweight framework around native `HWND`, Windows resources, Common Controls, command routing, DPI, layout, theme, and persistence while avoiding dependencies on MFC, ATL/WTL, BCGControlBar Pro, or BCG-compatible APIs.

## Current Status

The engineering baseline is implemented and validated in Debug and Release. The repository now also includes a product-growth sample portfolio: `NativeFrameUIShowcase`, `NativeFrameUIDarkStudio`, `NativeFrameUISettingsDemo`, and `NativeFrameUIResourceGallery`.

## V1 Baseline

- Product name: NativeFrame UI
- Build target: `NativeFrameUI`
- C++ namespace: `nfui`
- Product shape: pure Win32 C++ static library
- Minimum OS: Windows 10 1809
- Primary validation: Windows 10 22H2 and Windows 11 23H2/24H2
- Toolchain: Visual Studio 2022, MSVC v143, Windows SDK 10.0.22621 or newer
- Build system: CMake 3.25+ with CMake Presets
- Language standard: C++20
- Runtime: `/MD` for Release and `/MDd` for Debug
- Architecture: x64 for V1
- Character set: Unicode
- DPI: Per-Monitor DPI Awareness V2
- Rendering stack: Win32, Common Controls, GDI, UxTheme, DWM, Buffered Paint

## Scope

V1 aims to cover:

- Application initialization and shutdown
- Window class registration and message dispatch
- Owned and borrowed handle wrappers
- Modal and modeless resource dialogs
- Resource loading through explicit `HINSTANCE` contexts
- Command routing across menu, toolbar, accelerator, and context menu
- Common control wrappers for core Win32 controls
- Menu, toolbar, status bar, tab, tooltip, panel, and splitter support
- Basic layout, DPI, theme, and persistence systems
- A sample workbench application used as the acceptance target

V1 explicitly does not include Ribbon, full docking, a visual designer, complete Property Grid, complete Data Grid, a plugin system, MFC dependency, BCG dependency, or BCG-compatible API cloning.

## Documentation

- [Project Plan](docs/PROJECT_PLAN.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Feasibility](docs/FEASIBILITY.md)
- [Roadmap](docs/ROADMAP.md)
- [Product Growth Roadmap](docs/PRODUCT_GROWTH_ROADMAP.md)
- [Validation Matrix](docs/VALIDATION_MATRIX.md)
- [Showcase Guide](docs/SHOWCASE_GUIDE.md)
- [Demo Matrix](docs/DEMO_MATRIX.md)
- [Integration Guide](docs/INTEGRATION.md)
- [Resource Guide](docs/RESOURCE_GUIDE.md)
- [Theme Guide](docs/THEME_GUIDE.md)
- [Workbench Walkthrough](docs/WORKBENCH_WALKTHROUGH.md)
- [DarkStudio Walkthrough](docs/DARK_STUDIO_WALKTHROUGH.md)
- [SettingsDemo Walkthrough](docs/SETTINGS_DEMO_WALKTHROUGH.md)
- [ResourceGallery Walkthrough](docs/RESOURCE_GALLERY_WALKTHROUGH.md)
- [Known Limitations](docs/KNOWN_LIMITATIONS.md)
- [Contributing](docs/CONTRIBUTING.md)
- [Security](docs/SECURITY.md)
- [Support](docs/SUPPORT.md)
- [Governance](docs/GOVERNANCE.md)
- [Release Checklist](docs/RELEASE_CHECKLIST.md)
- [Original planning sources](docs/source/)
- [Agent guide](AGENTS.md)

## Planned Layout

```text
include/
  nfui/
src/
  core/
  resource/
  command/
  controls/
  layout/
  theme/
  dpi/
  persistence/
  diagnostics/
resources/
samples/
  NativeFrameUIWorkbench/
tests/
docs/
  architecture/
  design/
cmake/
```

Generated Visual Studio `.sln` and `.vcxproj` files should not become hand-maintained configuration. CMake should remain the build source of truth.

## Build

Configure, build, and test with CMake Presets:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Use `x64-release` for Release validation:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

The current smoke test validates the initial CMake/static-library path, Per-Monitor DPI V2 and Common Controls initialization, explicit framework resources, diagnostics result types, explicit `HWND` ownership wrappers, safe window `GWLP_USERDATA` binding and `WM_NCDESTROY` cleanup, string resource loading, modeless dialog creation, command routing, `WM_COMMAND`/`WM_NOTIFY` dispatch hooks, basic control wrapper creation, native `HWND` access, and basic USER/GDI resource-count stability.

`NativeFrameUISmokeTest.exe` is an automated console/CTest executable and exits immediately on success. It now also verifies that Showcase and focused-demo targets are emitted by the generated build artifacts. Use the visible sample executables for manual visual validation.

Phase 6 baseline coverage includes pure DPI conversion helpers, splitter and horizontal layout calculations, light/dark/high-contrast theme token selection, and validated workbench state persistence round-tripping.

See [Validation Matrix](docs/VALIDATION_MATRIX.md) for automated and manual compatibility checks.

## Quick Evaluation Path

1. Configure, build, and run `ctest` with the commands above.
2. Launch the visual samples from `out\build\x64-debug\Debug\` or `out\build\x64-release\Release\`.
3. Use the walkthroughs and matrix docs to decide whether you need the integration-focused Workbench or one of the product-growth demos.

| Executable | Purpose | Key evidence |
| --- | --- | --- |
| `NativeFrameUIWorkbench.exe` | Integration baseline for menus, splitters, controls, status, and command routing | [Workbench Walkthrough](docs/WORKBENCH_WALKTHROUGH.md) |
| `NativeFrameUIShowcase.exe` | Modern dashboard shell with light/dark toggle and DPI-aware painting | [Showcase Guide](docs/SHOWCASE_GUIDE.md) |
| `NativeFrameUIDarkStudio.exe` | Dark navigation shell with preview canvas and native status bar | [DarkStudio Walkthrough](docs/DARK_STUDIO_WALKTHROUGH.md) |
| `NativeFrameUISettingsDemo.exe` | Category navigation with native edit/combo/check inputs and save-state feedback | [SettingsDemo Walkthrough](docs/SETTINGS_DEMO_WALKTHROUGH.md) |
| `NativeFrameUIResourceGallery.exe` | Explicit string/menu/dialog/icon/bitmap resource loading gallery | [ResourceGallery Walkthrough](docs/RESOURCE_GALLERY_WALKTHROUGH.md) |
| `NativeFrameUIControls.exe` | Owner-draw/custom-draw control gallery (Button, StaticText, ListBox, ListView, IconView) themed from a shared `ThemePalette` | — |

Showcase, demo, and control-gallery visuals are sample-local evaluation surfaces built on the same shared primitives (`theme_palette`, `FontCache`, `fill_rounded_rect`, `draw_text`, `load_scaled_icon`). Stable framework guarantees remain the documented `nfui` APIs: application startup, HWND-oriented windows and controls, commands, layout, DPI helpers, persistence, and explicit resource handling.

## License

This project is intended to be released as open source. See [LICENSE.md](LICENSE.md).

# NativeFrame UI

NativeFrame UI is a planned pure Win32 C++ static UI framework for native Windows desktop applications. The project is currently in the documentation and architecture-baseline stage: there is no source tree, build system, sample application, or test suite yet.

The goal is to provide a lightweight framework around native `HWND`, Windows resources, Common Controls, command routing, DPI, layout, theme, and persistence while avoiding dependencies on MFC, ATL/WTL, BCGControlBar Pro, or BCG-compatible APIs.

## Current Status

This repository currently contains the project plan and architecture baseline only. Implementation work should start after the V1 baseline and feasibility spike are reviewed.

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

There is no build command yet. After the CMake skeleton is created, the expected validation path should be documented here, for example:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

## License

This project is intended to be released as open source. See [LICENSE.md](LICENSE.md).

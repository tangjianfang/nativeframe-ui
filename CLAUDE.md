# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

NativeFrame UI (`nfui` namespace) is a pure Win32 C++20 **static UI library** — no MFC, ATL/WTL, or BCGControlBar Pro. Build target `NativeFrameUI`; consumers link `NativeFrameUI::NativeFrameUI`. C++20, Unicode, x64, `/MD`|`/MDd`, Per-Monitor DPI Awareness V2. Toolchain: VS 2022 / MSVC v143 / Windows SDK 10.0.22621+. CMake 3.25+ with Presets is the only build entry point — never hand-edit generated `.sln`/`.vcxproj`.

## Build, test, run

```powershell
cmake --preset x64-debug          # or x64-release
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Run a single test:

```powershell
ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure
# or invoke the binary directly:
./out/build/x64-debug/Debug/NativeFrameUISmokeTest.exe
```

CI (`.github/workflows/ci.yml`) runs configure/build/test for both presets on `windows-latest`. There is no separate lint target — MSVC `/W4 /permissive- /EHsc /utf-8` is applied via `nfui_apply_compiler_options` to every target.

Two CTest tests are registered when `BUILD_TESTING=ON`:
- `NativeFrameUISmokeTest` — console exe that validates DPI/Common Controls init, explicit resource loading, `OwnedHwnd`/`BorrowedHwnd`, `GWLP_USERDATA` binding + `WM_NCDESTROY` cleanup, command routing, `WM_COMMAND`/`WM_NOTIFY` dispatch, control wrappers, and a hidden-window create/destroy resource-count check.
- `NativeFrameUIBoundaryCheck` — runs `tools/verify_boundaries.ps1`, which greps `include`, `src`, `samples`, `tests`, `resources`, `cmake`, `CMakeLists.txt` for forbidden markers: `BCGControlBar`, `#include <afx`, `#include <atl`, `#include <wtl`, `BCGCBPro`. Any MFC/ATL/WTL/BCG include or BCG branding fails the build.

Visual samples (manual validation) are emitted to `out/build/<preset>/(Debug|Release)/`: `NativeFrameUIWorkbench.exe` (integration baseline), plus product-growth demos `Showcase`, `DarkStudio`, `SettingsDemo`, `ResourceGallery`. They are `WIN32` executables (no console).

## Adding a new executable target

Every executable must call `nfui_add_resources(<target>)` so `resources/NativeFrameUI.rc` is compiled into the binary. Do **not** rely on the static lib carrying resources — explicit `.rc` inclusion is the deliberate strategy (see `docs/RESOURCE_GUIDE.md`, `docs/INTEGRATION.md`). Link `NativeFrameUI::NativeFrameUI` and call `nfui_apply_compiler_options(<target>)`.

## Architecture

Layered, one-way dependencies. **Core must not depend on controls, command, layout, theme, or business code.** Dependency rules and the forbidden edges are spelled out in `docs/ARCHITECTURE.md` — read it before touching module boundaries. The boundary check enforces the "no BCG/MFC/ATL/WTL" rule mechanically.

Public API surface is the headers in `include/nfui/` (umbrella header `NativeFrameUI.hpp`). Implementation lives in `src/<module>/` (`core`, `resource`, `command`, `controls`, `layout`, `theme`, `dpi`, `persistence`). Modules map one-to-one to headers/sources. Each new module should answer the design-review questions in `docs/ARCHITECTURE.md` (which layer owns it, what it depends on, who owns its resources, which thread may use it, error reporting, callback re-entrancy, testability without a real window, whether it leaks impl details).

Key invariants that cut across the codebase (from `AGENTS.md`/`docs/ARCHITECTURE.md`):

- Every wrapper bound to an `HWND` preserves access via `HWND hwnd() const noexcept` and keeps a **stable object address until `WM_NCDESTROY` finishes** (don't let C++ objects die before the window).
- Handle ownership is explicit and one of: **owned** (`OwnedHwnd` — RAII, calls `DestroyWindow`), **borrowed** (`BorrowedHwnd`), or shared. Public APIs must not accept ambiguous ownership.
- **Never** let C++ exceptions cross `WindowProc`, `DialogProc`, dispatcher, logging, or any system callback boundary.
- Only the UI thread creates/destroys/moves/repaints/themes windows; background work goes through a dispatcher (`dispatcher.post(...)`). Don't invoke user callbacks while holding internal locks.
- Keep **logical units** and **device pixels** distinct in layout/DPI/persistence code; persist logical units/ratios/validated semantic state, never physical pixels. Use `LONG_PTR`/`SetWindowLongPtr`/`GetWindowLongPtr` (never `LONG`/`SetWindowLong` for pointer data).
- Avoid global `HWND`/theme/command/resource singletons; services are owned by an `ApplicationContext`. Centralize `GetLastError`, `GetProcAddress`, and Windows-version checks in platform/adapter/DPI/theme modules.
- Prefer composition over inheritance; inheritance only for stable extension points (window, dialog, command handler, layout node).

## Conventions

- V1 non-goals (do not add without a separate design review): Ribbon, full docking, visual designer, complete Property Grid, complete Data Grid, plugin system, ARM64, Direct2D/DirectWrite (optional later), any BCG-compatible API cloning.
- Source of truth for planning/phases is `docs/PROJECT_PLAN.md`; phases 0–8 and the `v0.1`→`v2.0+` roadmap are in `AGENTS.md`. Link to existing docs rather than duplicating their content.
- Samples are evaluation surfaces; the stable framework guarantees are the documented `nfui` APIs, not sample visuals.
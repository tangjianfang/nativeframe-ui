# NativeFrameUI Agent Guide

This repository is currently in a planning and architecture-baseline state. There is no source tree, CMake project, test project, or CI configuration yet. Treat the Markdown documentation as the project source of truth until implementation artifacts are created.

## Source Documents

- [README.md](README.md): public project overview, current status, V1 baseline, scope, documentation links, and planned layout.
- [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md): complete phase plan from documentation cleanup through V1 acceptance.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): architecture baseline, dependency rules, resource strategy, and review questions.
- [docs/FEASIBILITY.md](docs/FEASIBILITY.md): feasibility analysis, BCGControlBar Pro boundary, and technical risks.
- [docs/ROADMAP.md](docs/ROADMAP.md): version roadmap from `v0.1` through post-V1 work.
- [docs/VALIDATION_MATRIX.md](docs/VALIDATION_MATRIX.md): automated and manual validation matrix.
- [docs/INTEGRATION.md](docs/INTEGRATION.md): CMake integration, initialization, and native handle guidance.
- [docs/RESOURCE_GUIDE.md](docs/RESOURCE_GUIDE.md): explicit resource integration and resource ID rules.
- [docs/THEME_GUIDE.md](docs/THEME_GUIDE.md): current theme token guidance.
- [docs/WORKBENCH_WALKTHROUGH.md](docs/WORKBENCH_WALKTHROUGH.md): manual visual sample walkthrough.
- [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md): current limitations and deferred features.
- [docs/source/](docs/source/): original long-form planning sources retained for traceability.

Link to these documents instead of duplicating their full content in new instructions or plans.

## Project Baseline

- Product name: NativeFrame UI.
- Repository and build target name: `NativeFrameUI`.
- C++ namespace: `nfui`.
- Product shape: independent pure Win32 C++ static UI library.
- Delivery artifacts: static library, public headers, resource templates, CMake package, sample application, integration docs.
- Primary integration boundary: native `HWND`, `HINSTANCE`, resources, Win32 messages, and Common Controls.
- Explicit non-goals for V1: MFC dependency, ATL/WTL dependency, BCG dependency, Ribbon, full docking, visual designer, complete Property Grid, complete Data Grid, plugin system, and BCG-compatible API cloning.

## Technical Baseline

- Minimum OS: Windows 10 1809.
- Primary validation: Windows 10 22H2 and Windows 11 23H2/24H2.
- Toolchain: Visual Studio 2022, MSVC v143, Windows SDK 10.0.22621 or newer.
- Build system: CMake 3.25+ with CMake Presets as the single source of build configuration.
- Language standard: C++20.
- Character set: Unicode.
- V1 architecture: x64.
- Runtime: `/MD` for Release and `/MDd` for Debug.
- DPI: Per-Monitor DPI Awareness V2 from the first implementation stage.
- Rendering stack: Common Controls, Win32, GDI, UxTheme, DWM, and Buffered Paint. Direct2D/DirectWrite are optional later additions.
- Public APIs should avoid leaking implementation details, but must preserve native handle access through methods such as `hwnd() const noexcept`.

## Architecture Rules

- Keep dependencies one-way: public API and application-facing modules depend inward on core services and Win32 adapters; core must not depend on controls, command, layout, theme, or business code.
- Preserve stable object addresses for any wrapper bound to an `HWND` until `WM_NCDESTROY` finishes.
- Make handle ownership explicit: owned, borrowed, or shared. Do not accept ambiguous ownership in public APIs.
- Never allow C++ exceptions to cross `WindowProc`, `DialogProc`, dispatcher, logging, or system callback boundaries.
- Only the UI thread may directly create, destroy, move, repaint, or theme windows. Background work must use a dispatcher such as `dispatcher.post(...)`.
- Do not invoke user callbacks while holding internal locks.
- Keep logical units and device pixels distinct in layout, DPI, and persistence code.
- Centralize platform compatibility, `GetLastError`, `GetProcAddress`, and Windows-version checks in platform, adapter, DPI, or theme modules.
- Prefer composition over inheritance. Use inheritance only for stable extension points such as window, dialog, command handler, or layout node abstractions.
- Avoid global `HWND`, theme, command, and resource singletons. Prefer explicit `ApplicationContext` ownership.

## Planned Repository Layout

When moving from documentation to implementation, prefer this structure unless a reviewed design decision changes it:

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

## Complete Project Plan

### Phase 0: Documentation Cleanup and Baseline

- Normalize existing planning content into a `docs/` structure.
- Preserve original long-form planning sources under `docs/source/` for traceability.
- Create project charter, requirements, non-functional requirements, acceptance criteria, and risk register.
- Freeze the V1 technology baseline before creating implementation code.
- Decide unresolved inputs: exact sample window, resource ownership model, logging API shape, exception policy wording, license policy, and performance targets.

### Phase 1: Feasibility Spike

- Create a minimal pure Win32 executable and static library.
- Prove that a consumer can link the library without MFC, ATL/WTL, or BCG.
- Verify resource loading for Dialog, Menu, Toolbar, String, Icon, and Bitmap.
- Validate the safer resource strategy: application resources plus explicit library resource templates, not implicit reliance on static `.lib` resource extraction.
- Test `/OPT:REF`, Debug, Release, and x64 behavior.
- Initialize Per-Monitor DPI V2 and Common Controls.
- Add basic object-count checks for `HWND`, GDI, and USER leaks.

### Phase 2: Build and Project Skeleton

- Add root `CMakeLists.txt`, `CMakePresets.json`, and `cmake/` helpers.
- Add `include/nfui/` public headers and `src/` module folders.
- Add `NativeFrameUI` static library target.
- Add `NativeFrameUIWorkbench` sample target.
- Add a test target runnable through CTest.
- Establish warnings, Unicode definitions, `_WIN32_WINNT=0x0A00`, MSVC runtime settings, and x64 presets.
- Add CI only after the local CMake build and CTest path are stable.

### Phase 3: Core Runtime and Resource System

- Implement `Application`, `ApplicationContext`, initialization, shutdown, and message loop ownership rules.
- Implement window class registration, message dispatch, and safe `GWLP_USERDATA` binding.
- Implement owned and borrowed handle wrappers with explicit lifecycle semantics.
- Implement modal and modeless resource dialog loading.
- Implement `ResourceContext` for application resources and library resources.
- Add diagnostic `Result`/`Error` infrastructure and Win32 error capture.
- Validate creation, destruction, invalid state handling, and callback exception boundaries.

### Phase 4: Command System and Basic Controls

- Implement command IDs, command handlers, command state, and command router.
- Route Menu, Toolbar, Accelerator, and context-menu actions through the same command path.
- Add Button, CheckBox, RadioButton, Edit, Static, ComboBox, ListBox, TreeView, and ListView wrappers.
- Preserve native `HWND` access for every control wrapper.
- Add `WM_COMMAND` and `WM_NOTIFY` integration tests.

### Phase 5: Frame Controls and Workbench Sample

- Add Menu, Toolbar, StatusBar, TabControl, Tooltip, ProgressBar, Panel, and Splitter.
- Build `NativeFrameUIWorkbench.exe` as the primary integration and acceptance sample.
- The sample should include menu, toolbar, left tree/search pane, center tab/list workspace, right inspector pane, status bar, context menu, and shared command state.
- Keep the right inspector in V1 as normal controls plus layout, not a full Property Grid.

### Phase 6: Layout, DPI, Theme, and Persistence

- Implement Anchor, linear, panel, and Splitter layout primitives.
- Handle `WM_DPICHANGED`, logical-to-pixel conversion, font scaling, icon scaling, and multi-monitor DPI transitions.
- Implement System, Light, Dark, and High Contrast fallback behavior.
- Add theme tokens and cache invalidation for brushes, fonts, icons, and colors.
- Persist main window position, maximized state, Splitter positions, active tab, theme choice, and selected column widths where applicable.
- Make restore logic resilient to missing monitors, corrupted files, DPI changes, and unknown future fields.

### Phase 7: Testing and Hardening

- Unit-test pure logic: DPI conversion, command state, layout calculation, persistence parsing, resource ID validation, and theme token selection.
- Integration-test hidden or sample windows: create/destroy cycles, dialog loading, command routing, menu and toolbar state, DPI change, theme change, and persistence restore.
- Add UI automation coverage with FlaUI, WinAppDriver, or Windows UI Automation when the sample stabilizes.
- Run manual compatibility on Windows 10/11, Debug/Release, common DPI values, single and mixed-DPI displays, System/Light/Dark/High Contrast, Chinese and English resources, mouse and keyboard workflows.
- Track GDI, USER, and memory growth across repeated create/destroy cycles.

### Phase 8: V1 Acceptance and Release

- Freeze public API and document known limitations.
- Produce integration docs, resource ID rules, theme guidance, sample walkthrough, and migration notes for existing Win32 applications.
- Verify that the library does not include copied BCG source, resources, class names, or branding.
- Verify that a consumer project can use headers, resources, CMake package, and static library without modifying library source.
- Cut a V1 release only when sample app, tests, Debug/Release builds, and acceptance matrix pass.

## Version Roadmap

- `v0.1`: engineering skeleton, initialization, window/message core, resource loading.
- `v0.2`: command routing, menu, accelerator, and basic controls.
- `v0.3`: toolbar, status bar, tab, splitter, tooltip, and sample workbench structure.
- `v0.4`: layout, DPI, theme, font/icon scaling, and persistence.
- `v0.9`: real integration pilot, compatibility testing, public API cleanup, and release-candidate docs.
- `v1.0`: accepted first stable release.
- `v1.1+`: Property Grid, advanced Tab, Data Grid, optional ARM64.
- `v2.0+`: Dock Pane, Floating Pane, advanced layout restoration, and separately reviewed Ribbon feasibility.

## Development Pitfalls

- Do not directly wrap or redistribute BCGControlBar Pro. This project is a clean pure Win32 implementation inspired by common desktop UI framework capabilities.
- Do not assume static libraries automatically carry usable resources into the final executable. Prefer explicit `.rc` inclusion or CMake-driven resource integration.
- Do not use `GetModuleHandle(nullptr)` as the universal resource lookup strategy. Use explicit resource module handles.
- Do not mix pointer data with `LONG` or `SetWindowLong`; use `LONG_PTR`, `SetWindowLongPtr`, and `GetWindowLongPtr`.
- Do not persist physical pixels for portable layout state. Store logical units, ratios, or validated semantic state.
- Do not add full docking, Ribbon, complete Property Grid, or complete Data Grid to V1 without a separate design review.

## Current Validation State

The initial CMake skeleton, static library, workbench sample, explicit framework resources, and smoke test exist. The smoke test verifies Per-Monitor DPI V2 initialization, Common Controls initialization, explicit Dialog/Menu/Toolbar/String/Icon/Bitmap resource loading, diagnostics result types, explicit `HWND` ownership wrappers, safe window `GWLP_USERDATA` binding and `WM_NCDESTROY` cleanup, string resource loading, modeless dialog creation, command routing, `WM_COMMAND`/`WM_NOTIFY` dispatch hooks, basic control wrapper creation, native `HWND` access, and a basic hidden-window create/destroy resource-count check.

`NativeFrameUISmokeTest.exe` is an automated console/CTest executable and exits immediately on success. Use `NativeFrameUIWorkbench.exe` for manual visual validation; it opens a visible workbench window with menu commands, left tree/search pane, center tab/list workspace, right inspector pane, splitters, progress bar, context menu, and status bar.

Phase 6 baseline coverage includes pure DPI conversion helpers, splitter and horizontal layout calculations, light/dark/high-contrast theme token selection, and validated workbench state persistence round-tripping.

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

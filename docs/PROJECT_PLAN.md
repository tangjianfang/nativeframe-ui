# NativeFrame UI Project Plan

This plan defines the path from the current documentation baseline to a V1 pure Win32 C++ static UI library.

## Phase 0: Documentation Cleanup and Baseline

- Normalize the planning material into a stable `docs/` structure.
- Preserve the original planning sources under `docs/source/`.
- Maintain a concise `README.md`, architecture summary, feasibility summary, roadmap, and agent guide.
- Freeze the V1 technology baseline before creating implementation code.
- Decide unresolved inputs: sample window details, resource ownership model, logging API shape, exception policy wording, license policy, and performance targets.

## Phase 1: Feasibility Spike

- Create a minimal pure Win32 executable and static library.
- Prove that a consumer can link the library without MFC, ATL/WTL, or BCG.
- Verify resource loading for Dialog, Menu, Toolbar, String, Icon, and Bitmap.
- Validate the safer resource strategy: application resources plus explicit library resource templates, not implicit reliance on static `.lib` resource extraction.
- Test `/OPT:REF`, Debug, Release, and x64 behavior.
- Initialize Per-Monitor DPI V2 and Common Controls.
- Add basic object-count checks for `HWND`, GDI, and USER leaks.

## Phase 2: Build and Project Skeleton

- Add root `CMakeLists.txt`, `CMakePresets.json`, and `cmake/` helpers.
- Add `include/nfui/` public headers and `src/` module folders.
- Add the `NativeFrameUI` static library target.
- Add the `NativeFrameUIWorkbench` sample target.
- Add a test target runnable through CTest.
- Establish warnings, Unicode definitions, `_WIN32_WINNT=0x0A00`, MSVC runtime settings, and x64 presets.
- Add CI only after the local CMake build and CTest path are stable.

## Phase 3: Core Runtime and Resource System

- Implement `Application`, `ApplicationContext`, initialization, shutdown, and message loop ownership rules.
- Implement window class registration, message dispatch, and safe `GWLP_USERDATA` binding.
- Implement owned and borrowed handle wrappers with explicit lifecycle semantics.
- Implement modal and modeless resource dialog loading.
- Implement `ResourceContext` for application resources and library resources.
- Add diagnostic `Result`/`Error` infrastructure and Win32 error capture.
- Validate creation, destruction, invalid state handling, and callback exception boundaries.

## Phase 4: Command System and Basic Controls

- Implement command IDs, command handlers, command state, and command router.
- Route Menu, Toolbar, Accelerator, and context-menu actions through the same command path.
- Add Button, CheckBox, RadioButton, Edit, Static, ComboBox, ListBox, TreeView, and ListView wrappers.
- Preserve native `HWND` access for every control wrapper.
- Add `WM_COMMAND` and `WM_NOTIFY` integration tests.

## Phase 5: Frame Controls and Workbench Sample

- Add Menu, Toolbar, StatusBar, TabControl, Tooltip, ProgressBar, Panel, and Splitter.
- Build `NativeFrameUIWorkbench.exe` as the primary integration and acceptance sample.
- Include menu, toolbar, left tree/search pane, center tab/list workspace, right inspector pane, status bar, context menu, and shared command state.
- Keep the right inspector in V1 as normal controls plus layout, not a full Property Grid.

## Phase 6: Layout, DPI, Theme, and Persistence

- Implement Anchor, linear, panel, and Splitter layout primitives.
- Handle `WM_DPICHANGED`, logical-to-pixel conversion, font scaling, icon scaling, and multi-monitor DPI transitions.
- Implement System, Light, Dark, and High Contrast fallback behavior.
- Add theme tokens and cache invalidation for brushes, fonts, icons, and colors.
- Persist main window position, maximized state, Splitter positions, active tab, theme choice, and selected column widths where applicable.
- Make restore logic resilient to missing monitors, corrupted files, DPI changes, and unknown future fields.

## Phase 7: Testing and Hardening

- Unit-test pure logic: DPI conversion, command state, layout calculation, persistence parsing, resource ID validation, and theme token selection.
- Integration-test hidden or sample windows: create/destroy cycles, dialog loading, command routing, menu and toolbar state, DPI change, theme change, and persistence restore.
- Add UI automation coverage with FlaUI, WinAppDriver, or Windows UI Automation when the sample stabilizes.
- Run manual compatibility on Windows 10/11, Debug/Release, common DPI values, single and mixed-DPI displays, System/Light/Dark/High Contrast, Chinese and English resources, mouse and keyboard workflows.
- Track GDI, USER, and memory growth across repeated create/destroy cycles.

## Phase 8: V1 Acceptance and Release

- Freeze public API and document known limitations.
- Produce integration docs, resource ID rules, theme guidance, sample walkthrough, and migration notes for existing Win32 applications.
- Verify that the library does not include copied BCG source, resources, class names, or branding.
- Verify that a consumer project can use headers, resources, CMake package, and static library without modifying library source.
- Cut a V1 release only when sample app, tests, Debug/Release builds, and acceptance matrix pass.

# Product Growth Demos Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a modern, DPI-aware Showcase and three focused demos without coupling visual experiments to the NativeFrame UI core.

**Architecture:** Each executable is a small Win32 `nfui::Window` consumer. Shared Showcase painting remains in `samples/NativeFrameUIShowcase/ShowcaseView.hpp/.cpp`; the focused demos compose the same library and use only their own scenario data. CMake registers every executable and the existing CTest/boundary path remains the validation source of truth.

**Tech Stack:** C++20, Win32, GDI, Common Controls, CMake 3.25+, MSVC v143, x64 Debug/Release.

---

### Task 1: Add the Showcase target and shared visual surface

**Files:**
- Modify: `CMakeLists.txt`
- Create: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp`
- Create: `samples/NativeFrameUIShowcase/ShowcaseView.hpp`
- Create: `samples/NativeFrameUIShowcase/ShowcaseView.cpp`
- Modify: `docs/VALIDATION_MATRIX.md`
- Create: `docs/SHOWCASE_GUIDE.md`

- [ ] Add a `WIN32` executable linked to `NativeFrameUI::NativeFrameUI`, apply compiler options, and integrate explicit resources.
- [ ] Implement a `ShowcaseView` owning only paint state: `ThemeMode`, hover card, selected navigation item, and DPI-scaled logical dimensions.
- [ ] Paint a sidebar, command bar, dashboard cards, status badges, inspector, separators, and selection using `BeginPaint`, `FillRect`, `RoundRect`, and `DrawTextW`.
- [ ] Handle `WM_PAINT`, `WM_SIZE`, `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_DPICHANGED`, `WM_ERASEBKGND`, and `WM_DESTROY`; keep all failures visible through normal window creation failure.
- [ ] Use `nfui::theme_tokens` for base colors and add only sample-local derived colors; do not expand the core API for showcase-only styling.
- [ ] Document startup, light/dark behavior, screenshot capture, DPI checks, and the distinction between Showcase code and framework APIs.
- [ ] Configure and build the new target in Debug, then run CTest.

### Task 2: Add theme switching and visual smoke coverage

**Files:**
- Modify: `samples/NativeFrameUIShowcase/ShowcaseView.hpp`
- Modify: `samples/NativeFrameUIShowcase/ShowcaseView.cpp`
- Modify: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/nativeframeui_smoke.cpp`

- [ ] Add a command-bar theme toggle that invalidates the view and switches between light and dark tokens.
- [ ] Ensure the window uses `Per-Monitor V2` initialization already provided by `nfui::Application`.
- [ ] Add a smoke-level helper that verifies the Showcase executable is registered as a build target through the generated build artifacts without launching a visible UI during CTest.
- [ ] Build and run Debug and Release CTest; fix compile, resource, and boundary failures before proceeding.

### Task 3: Add focused demo executables

**Files:**
- Modify: `CMakeLists.txt`
- Create: `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp`
- Create: `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp`
- Create: `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp`
- Create: `docs/DEMO_MATRIX.md`
- Create: `docs/DARK_STUDIO_WALKTHROUGH.md`
- Create: `docs/SETTINGS_DEMO_WALKTHROUGH.md`
- Create: `docs/RESOURCE_GALLERY_WALKTHROUGH.md`

- [ ] Register each executable as a separate `WIN32` target linked to the static library with explicit resource integration.
- [ ] Implement DarkStudio as a dark navigation shell with a preview canvas and native status bar.
- [ ] Implement SettingsDemo as category navigation plus native edit/combo/check controls and a saved-state indicator.
- [ ] Implement ResourceGallery as an explicit resource-loading gallery for strings, menu/dialog, icon, and bitmap assets.
- [ ] Keep each sample self-contained and avoid copying the core implementation or sharing hidden global state.
- [ ] Document each demo's purpose, launch command, relevant APIs, visual expectation, and known limitations.
- [ ] Build all demos in Debug and Release and run CTest after each target group.

### Task 4: Complete roadmap and release evidence

**Files:**
- Modify: `README.md`
- Modify: `docs/PRODUCT_GROWTH_ROADMAP.md`
- Modify: `docs/KNOWN_LIMITATIONS.md`
- Modify: `docs/VALIDATION_MATRIX.md`
- Modify: `docs/WORKBENCH_WALKTHROUGH.md`
- Modify: `docs/RELEASE_CHECKLIST.md`

- [ ] Add the Showcase and demo matrix to the README's quick evaluation path.
- [ ] Record which visual behaviors are showcase-only and which are stable core guarantees.
- [ ] Add per-target build, startup, DPI, theme, keyboard, resource, and screenshot checks to the validation matrix.
- [ ] Mark unsupported manual checks as pending rather than claiming unverified Windows coverage.
- [ ] Run `git diff --check`, both CMake presets, CTest, and `tools/verify_boundaries.ps1`.

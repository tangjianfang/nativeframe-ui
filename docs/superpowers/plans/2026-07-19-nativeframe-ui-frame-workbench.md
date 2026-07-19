# NativeFrame UI Frame Controls and Workbench Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 5 frame-control wrappers and replace the message-box sample with a visible `NativeFrameUIWorkbench.exe` window suitable for manual visual validation.

**Architecture:** Frame controls remain thin Win32 wrappers that expose native `HWND` and are composed by the sample. The Workbench is an application-facing integration sample, not framework core: it owns layout decisions and uses the command router, existing basic controls, and new frame wrappers to build the V1 acceptance shape.

**Tech Stack:** C++20, Win32, Common Controls, CMake, CTest.

---

## File Structure

- Modify `include/nfui/Controls.hpp` and `src/controls/Controls.cpp`: add `StatusBar`, `TabControl`, `ProgressBar`, `Panel`, and `Splitter`.
- Modify `tests/nativeframeui_smoke.cpp`: add frame-control creation tests and minimal splitter ratio tests.
- Replace `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`: create a real visible main window with menu, left tree/search pane, center tab/list workspace, right inspector pane, status bar, context menu, and shared command handling.
- Modify `README.md` and `AGENTS.md`: document that `NativeFrameUIWorkbench.exe` is the manual visual validation target and `NativeFrameUISmokeTest.exe` is automated only.

## Task 1: Frame Control Wrappers

**Files:**
- Modify: `include/nfui/Controls.hpp`
- Modify: `src/controls/Controls.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing tests** creating StatusBar, TabControl, ProgressBar, Panel, and Splitter as children of an `nfui::Window`, verifying valid `HWND`, and verifying `Splitter::set_ratio(0.25)` preserves a ratio between 0 and 1.
- [ ] **Step 2: Run `cmake --build --preset x64-debug`** and expect compile failure because the new frame controls do not exist.
- [ ] **Step 3: Implement wrappers** using Common Controls classes where appropriate and a child `STATIC` window for `Panel`/`Splitter`.
- [ ] **Step 4: Run `cmake --build --preset x64-debug; ctest --preset x64-debug`** and expect pass.

## Task 2: Visible Workbench Window

**Files:**
- Replace: `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`

- [ ] **Step 1: Update sample** to create a real overlapped main window instead of showing only a message box.
- [ ] **Step 2: Compose UI** with menu, left tree/search pane, center tab/list workspace, right inspector pane, status bar, context menu, and command routing for File/Exit and Help/About.
- [ ] **Step 3: Build sample** with `cmake --build --preset x64-debug` and expect success.

## Task 3: Documentation and Full Validation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update docs** explaining that `NativeFrameUISmokeTest.exe` is a console/CTest executable and `NativeFrameUIWorkbench.exe` is the visual/manual validation target.
- [ ] **Step 2: Run full validation:**

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

Expected: Debug and Release configure/build/test all pass.

## Self-Review

- Spec coverage: covers Phase 5 frame-control wrappers and visible workbench sample shape.
- Placeholder scan: no unresolved placeholders remain.
- Type consistency: new controls follow existing `create(ControlCreateParams)`, `hwnd()`, and `valid()` patterns.

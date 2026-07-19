# NativeFrame UI Commands and Basic Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the Phase 4 baseline command system and basic Win32 control wrappers with smoke-test coverage for command routing, `WM_COMMAND`, `WM_NOTIFY`, and native `HWND` access.

**Architecture:** Command routing stays independent from concrete menu/toolbar/control classes. Basic controls are thin native wrappers that create child windows with explicit parent/instance ownership and expose `hwnd() const noexcept`; window dispatch gains command/notify hooks so menus, accelerators, toolbars, context menus, and child controls can use one command path.

**Tech Stack:** C++20, Win32, Common Controls, CMake, CTest.

---

## File Structure

- Modify `CMakeLists.txt`: add command and controls implementation files.
- Create `include/nfui/Command.hpp` and `src/command/Command.cpp`: command IDs, command state, and command router.
- Create `include/nfui/Controls.hpp` and `src/controls/Controls.cpp`: Button, CheckBox, RadioButton, Edit, StaticText, ComboBox, ListBox, TreeView, and ListView wrappers.
- Modify `include/nfui/Window.hpp` and `src/core/Window.cpp`: add `on_command` and `on_notify` virtual hooks.
- Modify `include/nfui/NativeFrameUI.hpp`: include new public headers.
- Modify `tests/nativeframeui_smoke.cpp`: add TDD smoke coverage for direct command routing, `WM_COMMAND`, `WM_NOTIFY`, and basic control creation.
- Modify `README.md` and `AGENTS.md`: document Phase 4 smoke-test coverage.

## Task 1: Command Router

**Files:**
- Create: `include/nfui/Command.hpp`
- Create: `src/command/Command.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/nfui/NativeFrameUI.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing tests** for `CommandRouter::set_handler`, `set_state`, `state`, `route`, disabled commands, and unknown commands.
- [ ] **Step 2: Run `cmake --build --preset x64-debug`** and expect compile failure because command types do not exist.
- [ ] **Step 3: Implement command router** with `CommandId`, `CommandState`, and `CommandRouter`.
- [ ] **Step 4: Run `cmake --build --preset x64-debug; ctest --preset x64-debug`** and expect pass.

## Task 2: Window Command and Notify Hooks

**Files:**
- Modify: `include/nfui/Window.hpp`
- Modify: `src/core/Window.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing tests** using a derived window that counts `on_command` and `on_notify` calls after `SendMessageW(..., WM_COMMAND, ...)` and `SendMessageW(..., WM_NOTIFY, ...)`.
- [ ] **Step 2: Run `cmake --build --preset x64-debug`** and expect compile failure because hooks do not exist.
- [ ] **Step 3: Implement hooks** so `Window::handle_message` routes `WM_COMMAND` and `WM_NOTIFY` before falling through to `DefWindowProcW`.
- [ ] **Step 4: Run `cmake --build --preset x64-debug; ctest --preset x64-debug`** and expect pass.

## Task 3: Basic Control Wrappers

**Files:**
- Create: `include/nfui/Controls.hpp`
- Create: `src/controls/Controls.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/nfui/NativeFrameUI.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing tests** creating Button, CheckBox, RadioButton, Edit, StaticText, ComboBox, ListBox, TreeView, and ListView as children of an `nfui::Window`, verifying every wrapper exposes a valid `HWND`.
- [ ] **Step 2: Run `cmake --build --preset x64-debug`** and expect compile failure because control types do not exist.
- [ ] **Step 3: Implement controls** as thin RAII wrappers over `OwnedHwnd`, using Common Controls class names for TreeView/ListView and built-in Win32 class names for the rest.
- [ ] **Step 4: Run `cmake --build --preset x64-debug; ctest --preset x64-debug`** and expect pass.

## Task 4: Documentation and Full Validation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update docs** to mention command router, `WM_COMMAND`/`WM_NOTIFY`, and basic control wrapper smoke coverage.
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

- Spec coverage: covers Phase 4 command IDs, handlers, state, router, unified `WM_COMMAND`/`WM_NOTIFY` entry points, basic controls, and native `HWND` access.
- Placeholder scan: no unresolved placeholders remain.
- Type consistency: all public APIs are under `nfui` and included from `NativeFrameUI.hpp`.

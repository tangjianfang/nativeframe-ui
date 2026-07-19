# NativeFrame UI Core Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the Phase 3 core runtime baseline: diagnostics, explicit handle ownership, safe `HWND` binding/message dispatch, and modeless/modal resource dialog entry points.

**Architecture:** Keep core APIs small and native-first. `Window` owns the safe `GWLP_USERDATA` binding and never lets exceptions cross the system callback boundary; `OwnedHwnd`/`BorrowedHwnd` make handle ownership explicit; `ResourceContext` remains an explicit `HINSTANCE` adapter and gains dialog creation methods without using process-module fallbacks.

**Tech Stack:** C++20, Win32, MSVC v143, CMake, CTest.

---

## File Structure

- Modify `CMakeLists.txt`: add `src/core/Window.cpp`.
- Create `include/nfui/Diagnostics.hpp`: `Error`, `ErrorCode`, and `Result<T>` for non-throwing APIs.
- Create `include/nfui/Handle.hpp`: `BorrowedHwnd` and movable RAII `OwnedHwnd`.
- Create `include/nfui/Window.hpp`: `WindowCreateParams` and `Window` base wrapper.
- Modify `include/nfui/ResourceContext.hpp`: add string and dialog loading APIs.
- Modify `include/nfui/NativeFrameUI.hpp`: include new public headers.
- Create `src/core/Window.cpp`: class registration, creation, message dispatch, and `WM_NCDESTROY` cleanup.
- Modify `src/resource/ResourceContext.cpp`: implement string/modeless/modal dialog helpers with null-module guards.
- Modify `tests/nativeframeui_smoke.cpp`: add tests before implementation for diagnostics, handle ownership, window binding, null resource behavior, string loading, and modeless dialog creation/destruction.

## Task 1: Diagnostics Result Types

**Files:**
- Create: `include/nfui/Diagnostics.hpp`
- Modify: `include/nfui/NativeFrameUI.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing smoke assertions**

Add assertions that construct `nfui::Error::from_win32(ERROR_FILE_NOT_FOUND, L"missing file")`, verify the code and Win32 code are preserved, construct `nfui::Result<int>::success(42)`, and construct `nfui::Result<int>::failure(error)`.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: compile failure because `nfui::Error` and `nfui::Result` do not exist.

- [ ] **Step 3: Implement diagnostics**

Create `Diagnostics.hpp` with `ErrorCode`, `Error`, and a small `Result<T>` backed by `std::variant<T, Error>`. Keep the API non-throwing except for `std::bad_variant_access` impossible paths guarded by `has_value()`.

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: smoke test passes.

## Task 2: Explicit HWND Ownership Wrappers

**Files:**
- Create: `include/nfui/Handle.hpp`
- Modify: `include/nfui/NativeFrameUI.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing smoke assertions**

Add assertions for `BorrowedHwnd` preserving a borrowed handle without destroying it, and `OwnedHwnd` destroying a temporary `STATIC` window when reset.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: compile failure because `nfui::BorrowedHwnd` and `nfui::OwnedHwnd` do not exist.

- [ ] **Step 3: Implement handle wrappers**

Create a header-only `Handle.hpp` with:

- `BorrowedHwnd(HWND)`, `hwnd()`, and `valid()`.
- `OwnedHwnd(HWND)`, destructor calling `DestroyWindow`, deleted copy operations, move operations, `reset`, `release`, `hwnd()`, and `valid()`.

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: smoke test passes.

## Task 3: Window Binding and Message Dispatch

**Files:**
- Create: `include/nfui/Window.hpp`
- Create: `src/core/Window.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/nfui/NativeFrameUI.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing smoke assertions**

Add a `CountingWindow : nfui::Window` in the smoke test. Create a hidden overlapped window, verify `hwnd()` is non-null, verify `GetWindowLongPtrW(hwnd, GWLP_USERDATA)` equals the wrapper address, destroy it, verify `WM_NCDESTROY` was observed and `hwnd()` becomes null.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: compile failure because `nfui::Window` does not exist.

- [ ] **Step 3: Implement window runtime**

Register classes on demand using `RegisterClassExW`, treating `ERROR_CLASS_ALREADY_EXISTS` as success. Pass `this` through `CREATESTRUCTW::lpCreateParams`, store it with `SetWindowLongPtrW(hwnd, GWLP_USERDATA, ...)`, dispatch messages to `handle_message`, clear the binding during `WM_NCDESTROY`, and catch exceptions at the callback boundary with `OutputDebugStringW`.

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: smoke test passes.

## Task 4: Resource String and Dialog Helpers

**Files:**
- Modify: `include/nfui/ResourceContext.hpp`
- Modify: `src/resource/ResourceContext.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write failing smoke assertions**

Add assertions that `resources.load_string(IDS_NFUI_APP_TITLE)` returns `L"NativeFrame UI"`, `missing_resources.load_string(...)` is empty, `resources.create_dialog(IDD_NFUI_ABOUT, nullptr, proc, 0)` returns a non-null modeless dialog, and destroying it succeeds.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: compile failure because these `ResourceContext` methods do not exist.

- [ ] **Step 3: Implement resource helpers**

Add `std::wstring load_string(int) const`, `HWND create_dialog(int, HWND, DLGPROC, LPARAM) const noexcept`, and `INT_PTR show_modal_dialog(int, HWND, DLGPROC, LPARAM) const noexcept`. Every method must return an empty/null failure value when `module_ == nullptr`.

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: smoke test passes.

## Task 5: Final Validation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update docs**

Mention that the current smoke test now covers diagnostics, explicit `HWND` ownership, safe window binding, message dispatch cleanup, explicit resource lookup, and modeless dialog creation.

- [ ] **Step 2: Run full validation**

Run:

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

- Spec coverage: maps to Phase 3 items for diagnostics, handle ownership, message dispatch, `GWLP_USERDATA` binding, callback exception boundary, resource context, and modeless/modal dialog loading entry points.
- Placeholder scan: no unresolved placeholders remain; modal dialog is implemented as an API entry point and not invoked in tests because it would block without user interaction.
- Type consistency: all new public types use the `nfui` namespace and are included by `NativeFrameUI.hpp`.

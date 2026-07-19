# NativeFrame UI Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the first buildable NativeFrame UI foundation: a pure Win32 C++20 static library, explicit resource template integration, a workbench sample, and a CTest smoke test that proves the Phase 1 feasibility assumptions.

**Architecture:** Keep the initial public API small and native: `Application` owns process-level Win32 initialization helpers and message-loop execution, while `ResourceContext` validates explicit `HINSTANCE` resource lookup. CMake remains the single build source of truth and exposes a helper that lets every consumer deliberately include `resources/NativeFrameUI.rc` instead of relying on static-library resource extraction.

**Tech Stack:** C++20, Win32, Common Controls, GDI/User resource APIs, MSVC v143, Windows SDK 10.0.22621+, CMake 3.25+, CTest.

---

## File Structure

- Create `.gitignore`: ignore generated CMake/build output and local IDE artifacts.
- Create `CMakeLists.txt`: root project, static library, sample executable, smoke test, CTest registration.
- Create `CMakePresets.json`: x64 Debug/Release/MSVC presets.
- Create `cmake/NativeFrameUICompilerOptions.cmake`: common target definitions, warnings, Unicode, `_WIN32_WINNT=0x0A00`, and MSVC runtime.
- Create `cmake/NativeFrameUIResources.cmake`: `nfui_add_resources(target)` helper that explicitly attaches the framework `.rc` file to a consumer target.
- Create `include/nfui/Application.hpp`: minimal application/process initialization API.
- Create `include/nfui/ResourceContext.hpp`: explicit resource-module lookup API.
- Create `include/nfui/NativeFrameUI.hpp`: public umbrella header.
- Create `src/core/Application.cpp`: DPI/Common Controls initialization and message loop.
- Create `src/resource/ResourceContext.cpp`: typed resource lookup helpers.
- Create `resources/NativeFrameUIResource.h`: public resource IDs.
- Create `resources/NativeFrameUI.rc`: dialog, menu, toolbar, string, icon, and bitmap resource template.
- Create `resources/NativeFrameUI.ico`: tiny framework icon used by the resource template.
- Create `resources/NativeFrameUI.bmp`: tiny framework bitmap used by the resource template.
- Create `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`: minimal pure Win32 sample linked against the static library.
- Create `tests/nativeframeui_smoke.cpp`: CTest executable validating initialization, explicit resources, and basic USER/GDI object-count stability.
- Modify `README.md`: replace “no build command yet” with the new CMake/CTest validation commands.
- Modify `AGENTS.md`: update current validation state to list the real commands.

## Task 1: Build System Skeleton

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `cmake/NativeFrameUICompilerOptions.cmake`
- Create: `cmake/NativeFrameUIResources.cmake`

- [ ] **Step 1: Add the build files**

Use these exact files:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

project(NativeFrameUI VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(cmake/NativeFrameUICompilerOptions.cmake)
include(cmake/NativeFrameUIResources.cmake)

add_library(NativeFrameUI STATIC
    src/core/Application.cpp
    src/resource/ResourceContext.cpp
)
add_library(NativeFrameUI::NativeFrameUI ALIAS NativeFrameUI)

target_include_directories(NativeFrameUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(NativeFrameUI PUBLIC comctl32)
nfui_apply_compiler_options(NativeFrameUI)

add_executable(NativeFrameUIWorkbench WIN32
    samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp
)
target_link_libraries(NativeFrameUIWorkbench PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIWorkbench)
nfui_add_resources(NativeFrameUIWorkbench)

if(BUILD_TESTING)
    add_executable(NativeFrameUISmokeTest
        tests/nativeframeui_smoke.cpp
    )
    target_link_libraries(NativeFrameUISmokeTest PRIVATE NativeFrameUI::NativeFrameUI psapi)
    nfui_apply_compiler_options(NativeFrameUISmokeTest)
    nfui_add_resources(NativeFrameUISmokeTest)
    add_test(NAME NativeFrameUISmokeTest COMMAND NativeFrameUISmokeTest)
endif()
```

```cmake
# cmake/NativeFrameUICompilerOptions.cmake
function(nfui_apply_compiler_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target}
        PUBLIC
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0A00
    )

    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
endfunction()
```

```cmake
# cmake/NativeFrameUIResources.cmake
function(nfui_add_resources target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/NativeFrameUI.rc
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources
    )
endfunction()
```

```json
// CMakePresets.json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 25,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "x64-debug",
      "displayName": "x64 Debug",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "architecture": {
        "value": "x64",
        "strategy": "external"
      },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_INSTALL_PREFIX": "${sourceDir}/out/install/${presetName}",
        "BUILD_TESTING": "ON"
      }
    },
    {
      "name": "x64-release",
      "displayName": "x64 Release",
      "inherits": "x64-debug",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_INSTALL_PREFIX": "${sourceDir}/out/install/${presetName}",
        "BUILD_TESTING": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "x64-debug",
      "configurePreset": "x64-debug",
      "configuration": "Debug"
    },
    {
      "name": "x64-release",
      "configurePreset": "x64-release",
      "configuration": "Release"
    }
  ],
  "testPresets": [
    {
      "name": "x64-debug",
      "configurePreset": "x64-debug",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      }
    },
    {
      "name": "x64-release",
      "configurePreset": "x64-release",
      "configuration": "Release",
      "output": {
        "outputOnFailure": true
      }
    }
  ]
}
```

```gitignore
# .gitignore
out/
build/
.vs/
.vscode/
CMakeUserPresets.json
```

- [ ] **Step 2: Configure and expect failure because source files do not exist yet**

Run:

```powershell
cmake --preset x64-debug
```

Expected: configuration fails because `src/core/Application.cpp`, `src/resource/ResourceContext.cpp`, sample, and test files have not been created yet. This proves the build system is wired to real source paths.

- [ ] **Step 3: Commit**

```powershell
git add .gitignore CMakeLists.txt CMakePresets.json cmake
git commit -m "build: add cmake foundation"
```

## Task 2: Minimal Public API and Static Library

**Files:**
- Create: `include/nfui/Application.hpp`
- Create: `include/nfui/ResourceContext.hpp`
- Create: `include/nfui/NativeFrameUI.hpp`
- Create: `src/core/Application.cpp`
- Create: `src/resource/ResourceContext.cpp`

- [ ] **Step 1: Add tests that describe the desired public API**

Create `tests/nativeframeui_smoke.cpp` with the version and initialization assertions shown in Task 4, then run:

```powershell
cmake --preset x64-debug
```

Expected: configuration or compilation fails because the public headers and implementations do not exist.

- [ ] **Step 2: Add the public headers**

```cpp
// include/nfui/Application.hpp
#pragma once

#include <string_view>
#include <windows.h>

namespace nfui {

struct ApplicationConfig {
    HINSTANCE instance{};
    int show_command{SW_SHOWNORMAL};
};

class Application {
public:
    explicit Application(ApplicationConfig config) noexcept;

    [[nodiscard]] HINSTANCE instance() const noexcept;
    [[nodiscard]] int show_command() const noexcept;

    [[nodiscard]] static bool initialize_process_dpi() noexcept;
    [[nodiscard]] static bool initialize_common_controls() noexcept;

    int run() noexcept;

private:
    ApplicationConfig config_{};
};

[[nodiscard]] std::wstring_view version() noexcept;

} // namespace nfui
```

```cpp
// include/nfui/ResourceContext.hpp
#pragma once

#include <windows.h>

namespace nfui {

class ResourceContext {
public:
    explicit ResourceContext(HINSTANCE module) noexcept;

    [[nodiscard]] HINSTANCE module() const noexcept;
    [[nodiscard]] bool has_dialog(int resource_id) const noexcept;
    [[nodiscard]] bool has_menu(int resource_id) const noexcept;
    [[nodiscard]] bool has_string(int resource_id) const noexcept;
    [[nodiscard]] bool has_icon(int resource_id) const noexcept;
    [[nodiscard]] bool has_bitmap(int resource_id) const noexcept;
    [[nodiscard]] bool has_toolbar(int resource_id) const noexcept;

private:
    HINSTANCE module_{};
};

} // namespace nfui
```

```cpp
// include/nfui/NativeFrameUI.hpp
#pragma once

#include <nfui/Application.hpp>
#include <nfui/ResourceContext.hpp>
```

- [ ] **Step 3: Add minimal implementations**

```cpp
// src/core/Application.cpp
#include <nfui/Application.hpp>

#include <commctrl.h>

namespace nfui {

Application::Application(ApplicationConfig config) noexcept
    : config_(config) {
}

HINSTANCE Application::instance() const noexcept {
    return config_.instance;
}

int Application::show_command() const noexcept {
    return config_.show_command;
}

bool Application::initialize_process_dpi() noexcept {
    return SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE ||
           GetLastError() == ERROR_ACCESS_DENIED;
}

bool Application::initialize_common_controls() noexcept {
    INITCOMMONCONTROLSEX init{};
    init.dwSize = sizeof(init);
    init.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                 ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    return InitCommonControlsEx(&init) != FALSE;
}

int Application::run() noexcept {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

std::wstring_view version() noexcept {
    return L"0.1.0-dev";
}

} // namespace nfui
```

```cpp
// src/resource/ResourceContext.cpp
#include <nfui/ResourceContext.hpp>

namespace nfui {
namespace {

constexpr auto toolbar_resource_type = MAKEINTRESOURCEW(241);

[[nodiscard]] bool has_resource(HINSTANCE module, int resource_id, LPCWSTR resource_type) noexcept {
    return module != nullptr && FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_type) != nullptr;
}

} // namespace

ResourceContext::ResourceContext(HINSTANCE module) noexcept
    : module_(module) {
}

HINSTANCE ResourceContext::module() const noexcept {
    return module_;
}

bool ResourceContext::has_dialog(int resource_id) const noexcept {
    return has_resource(module_, resource_id, RT_DIALOG);
}

bool ResourceContext::has_menu(int resource_id) const noexcept {
    return LoadMenuW(module_, MAKEINTRESOURCEW(resource_id)) != nullptr;
}

bool ResourceContext::has_string(int resource_id) const noexcept {
    wchar_t buffer[2]{};
    return LoadStringW(module_, resource_id, buffer, static_cast<int>(std::size(buffer))) > 0;
}

bool ResourceContext::has_icon(int resource_id) const noexcept {
    HICON icon = static_cast<HICON>(LoadImageW(module_, MAKEINTRESOURCEW(resource_id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (icon == nullptr) {
        return false;
    }
    DestroyIcon(icon);
    return true;
}

bool ResourceContext::has_bitmap(int resource_id) const noexcept {
    HBITMAP bitmap = static_cast<HBITMAP>(LoadImageW(module_, MAKEINTRESOURCEW(resource_id), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
    if (bitmap == nullptr) {
        return false;
    }
    DeleteObject(bitmap);
    return true;
}

bool ResourceContext::has_toolbar(int resource_id) const noexcept {
    return has_resource(module_, resource_id, toolbar_resource_type);
}

} // namespace nfui
```

- [ ] **Step 4: Build and expect resource-related test failure**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: compilation may still fail until Task 3 adds resource IDs and Task 4 completes the smoke test.

- [ ] **Step 5: Commit**

```powershell
git add include src
git commit -m "feat: add nativeframe ui core api"
```

## Task 3: Explicit Resource Template

**Files:**
- Create: `resources/NativeFrameUIResource.h`
- Create: `resources/NativeFrameUI.rc`
- Create: `resources/NativeFrameUI.ico`
- Create: `resources/NativeFrameUI.bmp`

- [ ] **Step 1: Add resource IDs and resource script**

```cpp
// resources/NativeFrameUIResource.h
#pragma once

#define IDS_NFUI_APP_TITLE 100

#define IDI_NFUI_APP 200
#define IDB_NFUI_MARK 201
#define IDD_NFUI_ABOUT 202
#define IDM_NFUI_MAIN 203
#define IDT_NFUI_MAIN 204

#define IDM_NFUI_EXIT 40001
```

```rc
// resources/NativeFrameUI.rc
#include "NativeFrameUIResource.h"

#include <windows.h>
#include <commctrl.h>

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US

IDS_NFUI_APP_TITLE STRINGTABLE
BEGIN
    IDS_NFUI_APP_TITLE "NativeFrame UI"
END

IDI_NFUI_APP ICON "NativeFrameUI.ico"
IDB_NFUI_MARK BITMAP "NativeFrameUI.bmp"

IDM_NFUI_MAIN MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "E&xit", IDM_NFUI_EXIT
    END
END

IDD_NFUI_ABOUT DIALOGEX 0, 0, 180, 70
STYLE DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "About NativeFrame UI"
FONT 9, "Segoe UI"
BEGIN
    LTEXT "NativeFrame UI resource template", IDC_STATIC, 10, 10, 160, 12
    DEFPUSHBUTTON "OK", IDOK, 65, 45, 50, 14
END

IDT_NFUI_MAIN TOOLBAR 16, 16
BEGIN
    BUTTON IDM_NFUI_EXIT
END
```

- [ ] **Step 2: Add a tiny valid icon and bitmap**

Create a 16x16 icon and 2x2 bitmap in `resources/`. The exact pixel art is unimportant; they only need to compile through `rc.exe` and load with `LoadImageW`.

- [ ] **Step 3: Build resource consumers**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: resource compilation succeeds for both `NativeFrameUIWorkbench` and `NativeFrameUISmokeTest`.

- [ ] **Step 4: Commit**

```powershell
git add resources
git commit -m "feat: add explicit framework resources"
```

## Task 4: Feasibility Smoke Test

**Files:**
- Create: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing smoke test**

```cpp
// tests/nativeframeui_smoke.cpp
#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <psapi.h>
#include <windows.h>

#include <iostream>
#include <string_view>

namespace {

struct GuiResourceCounts {
    DWORD gdi{};
    DWORD user{};
};

[[nodiscard]] GuiResourceCounts capture_gui_resource_counts() noexcept {
    HANDLE process = GetCurrentProcess();
    return GuiResourceCounts{
        GetGuiResources(process, GR_GDIOBJECTS),
        GetGuiResources(process, GR_USEROBJECTS),
    };
}

bool expect(bool condition, std::wstring_view message) {
    if (!condition) {
        std::wcerr << L"FAIL: " << message << L'\n';
        return false;
    }
    return true;
}

} // namespace

int wmain() {
    bool ok = true;

    ok = expect(!nfui::version().empty(), L"version string is available") && ok;
    ok = expect(nfui::Application::initialize_process_dpi(), L"Per-Monitor DPI V2 initialization succeeds or was already set") && ok;
    ok = expect(nfui::Application::initialize_common_controls(), L"Common Controls initialization succeeds") && ok;

    nfui::ResourceContext resources(GetModuleHandleW(nullptr));
    ok = expect(resources.has_dialog(IDD_NFUI_ABOUT), L"dialog resource loads") && ok;
    ok = expect(resources.has_menu(IDM_NFUI_MAIN), L"menu resource loads") && ok;
    ok = expect(resources.has_string(IDS_NFUI_APP_TITLE), L"string resource loads") && ok;
    ok = expect(resources.has_icon(IDI_NFUI_APP), L"icon resource loads") && ok;
    ok = expect(resources.has_bitmap(IDB_NFUI_MARK), L"bitmap resource loads") && ok;
    ok = expect(resources.has_toolbar(IDT_NFUI_MAIN), L"toolbar resource loads") && ok;

    GuiResourceCounts before = capture_gui_resource_counts();
    HWND window = CreateWindowExW(0, L"STATIC", L"NativeFrame UI smoke test", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 320, 200,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ok = expect(window != nullptr, L"hidden smoke-test HWND can be created") && ok;
    if (window != nullptr) {
        DestroyWindow(window);
    }
    GuiResourceCounts after = capture_gui_resource_counts();

    ok = expect(after.gdi <= before.gdi + 2, L"GDI object count remains stable after create/destroy") && ok;
    ok = expect(after.user <= before.user + 2, L"USER object count remains stable after create/destroy") && ok;

    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Run the test to verify it fails before implementation is complete**

Run:

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected before Tasks 2 and 3 are complete: build or test failure caused by missing API/resources.

- [ ] **Step 3: Run the test to verify it passes after implementation**

Run:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 4: Commit**

```powershell
git add tests
git commit -m "test: add nativeframe ui smoke test"
```

## Task 5: Workbench Sample

**Files:**
- Create: `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`

- [ ] **Step 1: Add the minimal sample**

```cpp
// samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp
#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    nfui::ResourceContext resources(instance);
    if (!resources.has_menu(IDM_NFUI_MAIN) ||
        !resources.has_dialog(IDD_NFUI_ABOUT) ||
        !resources.has_toolbar(IDT_NFUI_MAIN)) {
        return 2;
    }

    MessageBoxW(nullptr,
                L"NativeFrame UI workbench sample is linked and resource-ready.",
                L"NativeFrame UI",
                MB_OK | MB_ICONINFORMATION);
    return 0;
}
```

- [ ] **Step 2: Build the sample**

Run:

```powershell
cmake --build --preset x64-debug
```

Expected: `NativeFrameUIWorkbench.exe` builds and links without MFC, ATL/WTL, or BCG.

- [ ] **Step 3: Commit**

```powershell
git add samples
git commit -m "feat: add workbench feasibility sample"
```

## Task 6: Documentation Update and Final Verification

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update validation documentation**

In `README.md`, replace the Build section with:

```markdown
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
```

In `AGENTS.md`, replace the current validation-state paragraph with the same command set and state that the smoke test verifies the initial resource and Win32 initialization feasibility path.

- [ ] **Step 2: Run Debug verification**

Run:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Expected: configure succeeds, build succeeds, and CTest reports 100% passing.

- [ ] **Step 3: Run Release verification**

Run:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

Expected: configure succeeds, build succeeds, and CTest reports 100% passing under Release and `/OPT:REF`.

- [ ] **Step 4: Commit**

```powershell
git add README.md AGENTS.md
git commit -m "docs: document nativeframe ui build validation"
```

## Self-Review

- Spec coverage: Phase 1 feasibility items map to `NativeFrameUISmokeTest`, explicit resource templates, Debug/Release presets, x64 presets, and Common Controls/DPI initialization. Phase 2 skeleton items map to root CMake files, public headers, `src/`, sample target, test target, warnings, Unicode definitions, `_WIN32_WINNT`, runtime settings, and documented validation commands.
- Placeholder scan: no `TBD`, `TODO`, or “implement later” placeholders remain. The icon/bitmap generation step intentionally allows any valid tiny binary because the behavior under test is Win32 resource loading, not artwork.
- Type consistency: `Application`, `ApplicationConfig`, `ResourceContext`, and resource ID names are consistent across headers, implementations, tests, and sample.

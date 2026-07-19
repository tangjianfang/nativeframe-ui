# NativeFrame UI Integration Guide

NativeFrame UI is consumed as a pure Win32 C++ static library.

## CMake

Use the library target and explicitly attach framework resources to executable targets:

```cmake
target_link_libraries(MyApp PRIVATE NativeFrameUI::NativeFrameUI)
nfui_add_resources(MyApp)
```

The resource step is deliberate. Do not rely on static-library resource extraction.

## Initialization

Applications should initialize DPI and Common Controls before creating UI:

```cpp
nfui::Application app({instance, show_command});
nfui::Application::initialize_process_dpi();
nfui::Application::initialize_common_controls();
```

## Native Handles

Wrappers preserve native interop through `hwnd() const noexcept`. Ownership is explicit through `BorrowedHwnd`, `OwnedHwnd`, `Window`, and control wrappers.

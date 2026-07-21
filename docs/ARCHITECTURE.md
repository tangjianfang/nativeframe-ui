# Architecture

NativeFrame UI uses a layered architecture with ports-and-adapters thinking, event-driven runtime flow, explicit ownership, RAII lifecycle management, and native Win32 interop.

## Runtime Positioning

The framework should hide common Win32 lifecycle complexity without closing access to native handles. Every wrapper that owns or borrows a window must preserve access to the underlying `HWND` through a method such as:

```cpp
HWND hwnd() const noexcept;
```

Core services must remain independent from concrete controls, business code, themes, layout policies, and command handlers.

## Module Boundaries

Expected dependency direction:

```text
Public API
  -> Application / Window / Dialog
  -> Core Services
  -> Win32 Adapters
```

Detailed dependency rules:

```text
core        -> Win32 base APIs
resource    -> core, Win32 resource APIs
dpi         -> core, Win32 DPI APIs
theme       -> core, dpi, UxTheme, DWM
layout      -> core, dpi
controls    -> core, resource, theme, dpi, layout
            (owns Control base infra: subclass_proc, create_native, anon helpers)
nfui_button     -> core, theme, controls
nfui_checkbox   -> core, theme, controls
nfui_radio      -> core, theme, controls
nfui_text       -> core, theme, controls   (StaticText + Edit)
nfui_listbox    -> core, theme, controls   (ListBox + ComboBox)
nfui_listview   -> core, theme, controls
nfui_treeview   -> core, theme, controls
nfui_iconview   -> core, theme, controls
nfui_frame      -> core, theme, controls   (StatusBar + TabControl + Tooltip +
                                            ProgressBar + Panel + Splitter)
command     -> core
persistence -> core, file system, JSON parsing
sample      -> public modules (typically NativeFrameUI umbrella)
```

Each per-component library is a self-contained `STATIC` library exposing
`NativeFrameUI::nfui_<name>`. Consumers can link individual components for
minimal footprint, or link the umbrella `NativeFrameUI::NativeFrameUI` for
the full surface. The umbrella `NativeFrameUI::NativeFrameUI` re-exports
all per-component targets via `target_link_libraries(... INTERFACE ...)`.

The base `Control` class (`subclass_proc`, `create_native`, owner-draw
dispatch, hover-state plumbing) lives in `nfui_controls`. Per-component
libraries depend on `nfui_controls` to access the base class vtable; this
is the documented V1.5 dependency until the `Control` base is split into
its own `nfui_control_base` library in a later migration.

Forbidden dependencies:

```text
core        -> controls
core        -> command
layout      -> concrete control implementations
command     -> Menu / Toolbar / TreeView
persistence -> HWND / Window objects
theme       -> business modules
resource    -> business modules
```

## Core Rules

- Keep dependencies one-way.
- Preserve stable object addresses for wrappers bound to `HWND` until `WM_NCDESTROY` finishes.
- Make handle ownership explicit: owned, borrowed, or shared.
- Never allow C++ exceptions to cross `WindowProc`, `DialogProc`, dispatcher, logging, or system callback boundaries.
- Only the UI thread may directly create, destroy, move, repaint, or theme windows.
- Do not invoke user callbacks while holding internal locks.
- Keep logical units and device pixels distinct.
- Centralize platform compatibility, `GetLastError`, `GetProcAddress`, and Windows-version checks.
- Prefer composition over inheritance.
- Avoid global `HWND`, theme, command, and resource singletons.

## Application Context

Framework services should be owned explicitly through an application context rather than hidden global singletons:

```cpp
class ApplicationContext {
public:
    ResourceService& resources() noexcept;
    ThemeService& themes() noexcept;
    Dispatcher& dispatcher() noexcept;
    Diagnostics& diagnostics() noexcept;
};
```

This avoids test pollution, unclear initialization order, shutdown-time dangling references, and future multi-instance limitations.

## Command Flow

Menus, toolbars, accelerators, and context menus should publish command IDs and route them through the command system:

```text
CommandSource
  -> CommandRouter
  -> Focused control
  -> Active page
  -> Parent pane
  -> Main window
  -> Global handler
```

Controls publish intent; business handlers decide behavior.

## Resource Strategy

Do not assume a static library will automatically carry usable resources into the final executable. Prefer explicit resource integration:

```text
NativeFrameUI.lib
include/nfui/
resources/NativeFrameUI.rc
resources/NativeFrameUIResource.h
cmake/NativeFrameUIResources.cmake
```

Applications should include or link framework resources deliberately, and public APIs should accept explicit resource module handles.

## Design Review Questions

Each new module should answer these questions before implementation:

1. Which layer owns it?
2. Which lower modules does it depend on?
3. Who owns the resources it manages?
4. Which thread may use it?
5. What are its states and transitions?
6. How does it report errors?
7. Does it trigger user callbacks?
8. Can callbacks re-enter framework code?
9. Can it be tested without a real window?
10. Does it leak implementation details into the public API?

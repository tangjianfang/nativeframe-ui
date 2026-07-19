# Feasibility

NativeFrame UI is feasible as a clean pure Win32 C++ UI framework. It should not be implemented as a wrapper around BCGControlBar Pro.

## Key Conclusion

The project should be treated as a new lightweight Win32 UI framework, not a repackaging of BCGControlBar Pro.

A direct BCG wrapper does not satisfy the project goals because BCGControlBar Pro depends on MFC, is commercially licensed, and cannot be redistributed or cloned as an independent pure Win32 static library.

A clean implementation can still be inspired by common desktop framework capabilities: command routing, menus, toolbars, status bars, tabs, panes, themes, DPI handling, layout, and persistence.

## Recommended Direction

- Build a pure Win32 C++ static library.
- Use native Windows resources for dialogs, menus, strings, icons, bitmaps, and toolbars.
- Keep integration boundaries native: `HWND`, `HINSTANCE`, `HMENU`, resources, Win32 messages, and Common Controls.
- Avoid dependencies on MFC, ATL/WTL, BCG, or commercial UI frameworks.
- Validate resource integration early because static-library resource behavior is a major risk.

## Main Risks

### Static Library Resources

Resource objects inside static libraries may be removed by the linker if they are not explicitly referenced. The safer strategy is application resources plus explicit framework resource templates, integrated through CMake or direct `.rc` inclusion.

### Docking Scope

Full docking is much larger than simple child-window layout. V1 should use Splitter and panel layout only. Dock panes, floating panes, auto-hide, and advanced layout restoration should remain post-V1 work.

### Theme Coverage

System, Light, Dark, and High Contrast behavior should be designed from V1, but the project should not promise perfect dark rendering for every native control through undocumented system behavior.

### DPI

Per-Monitor DPI V2 must be present from the beginning. Retrofitting DPI later would affect layout, resources, fonts, icons, persistence, and tests.

### Legal Boundary

The repository must not copy BCG source code, resources, icons, theme files, class names, branding, or API surface in a way that implies compatibility.

## Feasibility Spike Acceptance

The first implementation spike should prove:

- A pure Win32 executable can link the static library.
- No MFC, ATL/WTL, or BCG dependency is required.
- Dialog, menu, toolbar, string, icon, and bitmap resources can be loaded reliably.
- Debug, Release, `/OPT:REF`, and x64 configurations behave correctly.
- Per-Monitor DPI V2 and Common Controls initialization work.
- Repeated create/destroy cycles do not leak `HWND`, GDI, or USER objects.

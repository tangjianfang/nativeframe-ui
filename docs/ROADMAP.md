# Roadmap

## v0.1: Engineering Skeleton

- CMake skeleton
- Static library target
- Workbench sample target
- Test target through CTest
- Application initialization
- Window and message core
- Basic resource loading

## v0.2: Command and Basic Controls

- Command IDs, command handlers, command state, and router
- Menu and accelerator routing
- Button, CheckBox, RadioButton, Edit, Static, ComboBox, ListBox
- TreeView and ListView wrappers
- `WM_COMMAND` and `WM_NOTIFY` integration tests

## v0.3: Frame Controls and Workbench Structure

- Toolbar
- StatusBar
- TabControl
- Splitter
- Tooltip
- ProgressBar
- Panel
- Initial `NativeFrameUIWorkbench` sample layout

## v0.4: Layout, DPI, Theme, and Persistence

- Anchor, linear, panel, and Splitter layout primitives
- Per-Monitor DPI V2 runtime handling
- Font and icon scaling
- System, Light, Dark, and High Contrast theme handling
- Window and layout persistence

## v0.9: Release Candidate

- Real integration pilot
- Windows 10/11 compatibility testing
- Debug/Release validation
- Public API cleanup
- Documentation pass
- Known limitations documented

## v1.0: First Stable Release

- Accepted public API
- Passing sample app and test matrix
- Integration docs
- Resource ID rules
- Theme guidance
- Sample walkthrough
- Migration notes for existing Win32 applications

## v1.1+

- Property Grid
- Advanced Tab
- Data Grid
- Optional ARM64 support

## v2.0+

- Dock Pane
- Floating Pane
- Advanced layout restoration
- Ribbon feasibility review

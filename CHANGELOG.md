# Changelog

All notable changes to NativeFrame UI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0] - 2026-08-03

### Added
- **PropertyGrid control (CP43)**: new `nfui_propertygrid` module.
  `PropertyGridModel` keeps a committed value plus a pending edit per
  row so a host dialog can offer OK / Cancel / Apply semantics without
  the grid knowing about buttons. The self-painted grid renders
  two-column name/value rows on the theme palette, with an in-place
  borderless EDIT for string/integer values, click-to-toggle booleans,
  click-to-cycle choices, keyboard navigation (Up/Down/Space/Enter/F2),
  and per-type plus custom validation that keeps the editor open with a
  danger border on bad input. Demonstrated in ComponentGallery; the
  HWND-free model layer is unit-tested.
- **Self-painted Tooltip (CP42)**: `Tooltip` retains the native
  TOOLTIPS_CLASSW window for hover tracking and `TTM_ADDTOOL`
  compatibility but intercepts WM_PAINT to fully self-draw the bubble
  and measures its own size from the shared UI font. Tips now follow
  the active palette instead of flashing the ComCtl32 default light
  face in dark / high-contrast themes. Pixel-verified by a new
  SmokeTest assertion.
- **SDK packaging**: `cmake/NativeFrameUIInstall.cmake` ships install
  rules and a relocatable CMake package. Downstream projects can now
  `find_package(NativeFrameUI CONFIG REQUIRED)` and link
  `NativeFrameUI::NativeFrameUI` (umbrella) or any per-component
  `NativeFrameUI::nfui_*` target from an installed prefix. Public
  headers and the explicit resource template are installed under
  `include/nfui` / `include/nfui/resources`. Opt out via
  `NFUI_ENABLE_INSTALL=OFF` when embedding through `add_subdirectory()`.
- `nfui::TabControl::set_padding(int cx, int cy)` — additive forward of
  `TCM_SETPADDING` so consumers can tune horizontal / vertical padding
  around each tab. Three existing samples (Workbench, ThemeDemo,
  ComponentGallery) now use a DPI-scaled `(12, 4)` rhythm.
  (polish/CP8-tab-control)
- `NativeFrameUIControls` now honours `--theme <light|dark|high_contrast>`
  — the last sample without launch-theme support. All 14 demos now
  produce genuine three-theme visual-audit captures.

### Changed
- `NFUI_BUILD_CHARTS` now defaults to **ON**: the charts module builds
  clean on MSVC v143 and its interaction test passes, so the stale
  "compile errors" opt-out rationale no longer applies.

### Fixed
- Build warnings across samples: unused locals in the Charts grid
  layout (C4189), duplicate `NOMINMAX` definition in ComponentGallery
  (C4005), discarded `[[nodiscard]]` dispatcher in Workbench (C4834).
- Stale polish knowledge entries CP15 / CP16 marked resolved — their
  fixes had already landed in the tree.
- **ComboBox owner-draw status corrected**: the owner-draw ComboBox
  polish entry was documented as deferred to V1.5+ pending a
  `WH_CALLWNDPROCEXT` hook, but `CBS_OWNERDRAWFIXED` already covers the
  popup list rows. The entry is marked resolved; no hook is needed.

## [1.0.0] - 2026-07-22

### Added
- **Per-component library split**: 16 control wrappers now ship as
  individual STATIC libraries (`nfui_button`, `nfui_checkbox`,
  `nfui_radio`, `nfui_text`, `nfui_listbox`, `nfui_listview`,
  `nfui_treeview`, `nfui_iconview`, `nfui_frame`) plus a shared
  `nfui_control_base`. Consumers can link only what they need for
  minimal binary footprint.
- **Architecture layering**: extracted `nfui_layout` (Rect,
  SplitterLayout) and `nfui_window` (Window subclass) as separate
  static libraries. Core no longer depends on layout or window code.
- **`NativeFrameUIMinimal` sample**: a 116KB "hello button" app
  that links only 4 per-component libraries, proving the layered
  architecture.
- **Button disabled-state WCAG AA contrast**: disabled face now
  derives from a lightened border, not pure gray. Verified 4.649:1
  light, 5.643:1 dark.
- **ListBox per-row hover highlight**: `ListBox::set_hovered_row`
  + `hovered_row()` API; OCM_DRAWITEM dispatch wired to
  `ListBox::draw_item`. Hover overlay via alpha_blend 6% tint.
- **Native chrome theme application**: FrameStyle extended with
  `surface_brush`, `chrome_text`, `chrome_bg`, `bar_color`. TreeView,
  ListView, TabControl, Tooltip, Panel, Splitter, ProgressBar all
  honor the Claude palette.
- **Knowledge base**: 6 competitive comparisons (BCG, MFC, Qt, Dear
  ImGui, wxWidgets, WPF, WinForms, Sciter/Ultralight, Slint), 12
  polish backlog entries, 5 historical-pitfall problem entries.
- **V2.0 design specs**: 10 living design specs covering rendering
  engine migration (Direct2D + DirectWrite), advanced controls
  (DataGrid, PropertyGrid, Docking, Ribbon), and meta features
  (plugin system, visual designer, printing).
- **Platform extension specs**: 4 living surveys covering ARM64,
  MinGW, clang-cl, and Wine compatibility.
- **Test & quality specs**: 4 living specs covering visual
  regression, cross-compiler CI, static analysis, and fuzzing.
- **Documentation**: per-component Controls README, Button cooking
  recipe, control creation cost benchmark, hello-window tutorial.

### Changed
- **Per-control subclass dispatch refactored**: `Control::subclass_proc`
  now routes per-component-specific message handling via three
  virtual extension points (`on_reflected_draw_item`,
  `on_subclass_mouse_move`, `on_subclass_mouse_leave`). This removes
  the cross-component symbol dependency between `nfui_control_base`
  and `nfui_listbox`.
- **Compiler options**: `nfui_apply_compiler_options` now applies
  `/FS /MP` to fix parallel PDB lock errors under MSBuild with
  per-target PDB configuration.

### Fixed
- **ListBox::draw_item dead code path**: previously the per-component
  `draw_item` was unreachable because `Control::subclass_proc`
  dispatched `ODT_LISTBOX` to an anonymous-namespace helper. Now
  routes correctly via the virtual dispatch above.
- **Stale P1.8 link to `nfui_controls`**: NativeFrameUIMinimal
  switched to `nfui_control_base` after P2.1's rename (TODO comment
  predicted this).

[1.2.0]: https://github.com/tangjianfang/nativeframe-ui/releases/tag/v1.2.0
[1.0.0]: https://github.com/tangjianfang/nativeframe-ui/releases/tag/v1.0.0
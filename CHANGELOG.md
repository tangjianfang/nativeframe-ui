# Changelog

All notable changes to NativeFrame UI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `nfui::TabControl::set_padding(int cx, int cy)` — additive forward of
  `TCM_SETPADDING` so consumers can tune horizontal / vertical padding
  around each tab. Three existing samples (Workbench, ThemeDemo,
  ComponentGallery) now use a DPI-scaled `(12, 4)` rhythm.
  (polish/CP8-tab-control)

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

[1.0.0]: https://github.com/tangjianfang/nativeframe-ui/releases/tag/v1.0.0
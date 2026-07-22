---
title: Sample component-link audit (per-component split consumer story)
date: 2026-07-22
tags: [samples, linking, layered-architecture, build-size]
severity: cosmetic
effort: small
status: resolved
related:
  - 2026-07-23-cp11-sample-per-component-link.md
---

This entry audits the four product-growth samples — `NativeFrameUIShowcase`,
`NativeFrameUIDarkStudio`, `NativeFrameUISettingsDemo`,
`NativeFrameUIResourceGallery` — and tabulates the component classes actually
referenced in each source against the umbrella link currently in
`CMakeLists.txt`. It is the deliverable for P1.7 of the layered-library
migration; the per-sample link refactor itself is deferred to a follow-up
polish pass.

## Method

For each sample directory under `samples/`, grepped the source for every
control class listed in the per-component split
(`Button`, `CheckBox`, `RadioButton`, `StaticText`, `Edit`, `ComboBox`,
`ListBox`, `TreeView`, `ListView`, `IconView`, `StatusBar`, `TabControl`,
`Tooltip`, `ProgressBar`, `Panel`, `Splitter`, plus chart view classes).
Then read the `ControlCreateParams` + every `.create(params)` call to
corroborate the declared member types are actually instantiated.

Foundation helpers used (`nfui::Window`, `nfui::Application`,
`nfui::ThemePalette`, `nfui::ThemeMode`, `nfui::Color`, `nfui::DpiScale`,
`nfui::FontCache`, `nfui::ResourceContext`, `nfui::MemoryDC`,
`nfui::draw_text`, `nfui::fill_rounded_rect`, `nfui::fill_rect`,
`nfui::darken`, `nfui::alpha_blend`, `nfui::theme_palette`,
`nfui::theme_metrics`, `nfui::dpi_of`, `nfui::ControlCreateParams`)
intentionally omitted from per-sample component counts — every sample
depends on `nfui_core` + `nfui_theme` transitively.

`NativeFrameUIWorkbench` and `NativeFrameUIControls` are gallery / integration
baselines (per `CLAUDE.md`) and are expected to keep the umbrella link;
`NativeFrameUICharts` already opts out of the umbrella and links
`nfui_charts` + `nfui_charts_aa` directly. None are in scope for this
audit.

## Per-sample component usage

### `NativeFrameUIShowcase`

Source files: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp`,
`samples/NativeFrameUIShowcase/ShowcaseView.cpp`,
`samples/NativeFrameUIShowcase/ShowcaseView.hpp`.

Components actually referenced in source: **none**.

Showcase paints its UI entirely through `nfui::draw_text`,
`nfui::fill_rounded_rect`, and `nfui::fill_rect`. No `nfui::Button`,
`nfui::StaticText`, `nfui::Edit`, or any other control class appears in
either source file. The class only uses foundation types:
`nfui::Window`, `nfui::Application`, `nfui::ResourceContext`,
`nfui::FontCache`, `nfui::DpiScale`, `nfui::ThemePalette`, `nfui::Color`,
`nfui::ThemeMode`.

Recommended link set:
- `nfui_core` + `nfui_theme` (both come transitively via any component, but
  Showcase links no components, so both must be named explicitly).
- **No** per-component libs required.

This is the strongest demo of the layered architecture paying off for
consumers: Showcase is the smallest meaningful sample, and post-split it
strips out every per-component lib.

### `NativeFrameUIDarkStudio`

Source files: `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp`.

Components actually referenced in source:
- `nfui::StatusBar` — declared as `nfui::StatusBar status_` and created via
  `nfui::ControlCreateParams` in `create_children()` (1 instance).

Recommended link set:
- `nfui_frame` (for `StatusBar`; transitively brings `nfui_core` +
  `nfui_theme`).

Eligible for an aggressive trim — DarkStudio currently drags in
`nfui_button`, `nfui_checkbox`, `nfui_radio`, `nfui_text`, `nfui_listbox`,
`nfui_listview`, `nfui_treeview`, `nfui_iconview`, `nfui_charts`, and
`nfui_controls` for one `StatusBar` member.

### `NativeFrameUISettingsDemo`

Source files: `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp`.

Components actually referenced in source:
- `nfui::ListBox` (`categories_`) — 1 instance.
- `nfui::Edit` (`profile_name_`, `workspace_root_`) — 2 instances.
- `nfui::ComboBox` (`theme_combo_`) — 1 instance.
- `nfui::CheckBox` (`telemetry_`, `startup_`) — 2 instances.
- `nfui::Button` (`save_button_`) — 1 instance.
- `nfui::StaticText` (`status_label_`, `description_`) — 2 instances.
- `nfui::StatusBar` (`status_bar_`) — 1 instance.

Recommended link set:
- `nfui_text` (for `StaticText` + `Edit`).
- `nfui_listbox` (for `ListBox` + `ComboBox`).
- `nfui_checkbox` (for `CheckBox`).
- `nfui_button` (for `Button`).
- `nfui_frame` (for `StatusBar`).
- (transitively: `nfui_core`, `nfui_theme`, `nfui_controls`).

The heaviest per-sample user of components — but it still does not need
`nfui_listview`, `nfui_treeview`, `nfui_iconview`, `nfui_radio`,
`nfui_charts`, or any of the chart helpers that the umbrella re-exports.

Note: SettingsDemo is the template-candidate that motivates
`nfui_settings_form` (see spec §3.1). Once templates land, SettingsDemo
flips to linking the template header-only and probably drops every
per-component lib.

### `NativeFrameUIResourceGallery`

Source files:
`samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp`.

Components actually referenced in source:
- `nfui::Button` (`open_dialog_`, `reload_assets_`) — 2 instances.
- `nfui::StatusBar` (`status_bar_`) — 1 instance.

Recommended link set:
- `nfui_button` (for `Button`).
- `nfui_frame` (for `StatusBar`).
- (transitively: `nfui_core`, `nfui_theme`).

ResourceGallery currently pulls every component lib for two `Button`s and
one `StatusBar`. This is the second-most wasteful sample after Showcase.

## Current linking

All four samples link `NativeFrameUI::NativeFrameUI`, defined in
`CMakeLists.txt:24-46` as an `INTERFACE` library that re-exports:

| Re-exported lib | Needed by Showcase | Needed by DarkStudio | Needed by SettingsDemo | Needed by ResourceGallery |
|---|:---:|:---:|:---:|:---:|
| `nfui_core` | yes | yes | yes (transitive) | yes (transitive) |
| `nfui_theme` | yes | yes (transitive) | yes (transitive) | yes (transitive) |
| `nfui_controls` | no | no | yes (transitive via `nfui_text`) | no |
| `nfui_button` | no | no | yes | yes |
| `nfui_checkbox` | no | no | yes | no |
| `nfui_radio` | no | no | no | no |
| `nfui_text` | no | no | yes | no |
| `nfui_listbox` | no | no | yes | no |
| `nfui_listview` | no | no | no | no |
| `nfui_treeview` | no | no | no | no |
| `nfui_iconview` | no | no | no | no |
| `nfui_frame` | no | yes | yes | yes |
| `nfui_charts` | no | no | no | no |

Each "yes (transitive)" cell resolves automatically because every
per-component CMake module declares `target_link_libraries(... PUBLIC
NativeFrameUI::nfui_core NativeFrameUI::nfui_theme)` — so naming only the
component lib(s) is sufficient at link time.

## Recommendation

Convert each sample to its recommended link set. Concretely:

```cmake
# Showcase — zero per-component deps.
target_link_libraries(NativeFrameUIShowcase
    PRIVATE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)

# DarkStudio — frame only.
target_link_libraries(NativeFrameUIDarkStudio
    PRIVATE
        NativeFrameUI::NativeFrameUI  # or NativeFrameUI::nfui_frame + transitive core/theme
)

# SettingsDemo — five per-component deps.
target_link_libraries(NativeFrameUISettingsDemo
    PRIVATE
        NativeFrameUI::nfui_text
        NativeFrameUI::nfui_listbox
        NativeFrameUI::nfui_checkbox
        NativeFrameUI::nfui_button
        NativeFrameUI::nfui_frame
)

# ResourceGallery — two per-component deps.
target_link_libraries(NativeFrameUIResourceGallery
    PRIVATE
        NativeFrameUI::nfui_button
        NativeFrameUI::nfui_frame
)
```

Why this matters (spec §2.1 acceptance criteria, §4 call-out):

- **Binary size win**: SettingsDemo currently drags in 10 component libs'
  worth of object code via the umbrella; post-split it pulls exactly five.
  Showcase's binary drops every component. Estimated `.rdata` / `.text`
  reduction scales with the number of unused component `.obj` files the
  static linker can drop.
- **Recompile isolation (spec §1 motivation 1)**: a `Button.cpp` polish
  commit today rebuilds SettingsDemo, DarkStudio, and ResourceGallery; after
  the swap, only SettingsDemo and ResourceGallery rebuild.
- **Public-facing proof**: the per-component split today has no consumer
  story. Refactoring these samples makes the layered architecture visible
  in the repo and gives consumers a clean template to copy.

## Decision

Defer the implementation to a follow-up polish pass. The audit is the
deliverable for P1.7. Implementation cost is ~30 LOC across four
`target_link_libraries` blocks in `CMakeLists.txt` (no source changes),
plus a smoke-test verification that each sample still links and starts.
The deferral frees P1.7 to land exactly one thing (this audit) and lets
the per-component refactor land as P1.8 (or be folded into M12 of the
spec's migration plan). Whoever picks it up should:

1. Open a `polish/P1.8-sample-per-component-link` branch off
   `polish/P1.7-sample-audit` (this branch).
2. Apply the four `target_link_libraries` blocks above.
3. Configure + build both presets. Confirm all four `.exe` files still
   link with no unresolved symbols.
4. Run `NativeFrameUISmokeTest` and `NativeFrameUIBoundaryCheck`.
5. Capture pre/post `.exe` sizes via
   `Get-Item <exe> | Select-Object FullName,Length` and append the delta to
   this entry under `## Resolution`.

## Cross-references

- Spec: `docs/superpowers/specs/2026-07-22-layered-library-architecture-design.md`
  §2.1, §2.3, §4 (sample link lists); §7 Migration plan step M12.
- Existing polish board (style reference for this entry's schema):
  - `docs/KNOWLEDGE/polish/2026-07-22-button-disabled-state-color.md`
  - `docs/KNOWLEDGE/polish/2026-07-22-combo-box-dropdown-item-style.md`
  - `docs/KNOWLEDGE/polish/2026-07-22-listbox-row-hover-not-implemented.md`

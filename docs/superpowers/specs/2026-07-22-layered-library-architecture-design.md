# Layered Library Architecture + Component Split — Design

**Date:** 2026-07-22
**Status:** Draft (awaiting user review)

## Context

`NativeFrameUI` is currently a single-mesh `STATIC` library with five internal CMake modules (`nfui_core`, `nfui_theme`, `nfui_controls`, `nfui_charts`, `nfui_charts_aa`). The split today is **horizontal** (by module kind) but **not vertical** (by component):

- `nfui_controls` is one 377-line `Controls.cpp` carrying 16 component classes (`Button`, `CheckBox`, `RadioButton`, `Edit`, `StaticText`, `IconView`, `ComboBox`, `ListBox`, `TreeView`, `ListView`, `StatusBar`, `TabControl`, `ProgressBar`, `Panel`, `Splitter`, plus the shared `Control` base).
- `nfui_charts` lumps four chart kinds (`BarChart`, `HBarChart`, `LineChart`, `SplineChart`) into one library with `PRIVATE` geometry helpers.
- A consumer links **one** `NativeFrameUI` umbrella and gets **all** of the above. There is no way to ship "just Button + Text" without dragging in `TreeView`, `Splitter`, `ChartView`, etc.

This causes three concrete problems:

1. **Polish iterations are coupled.** Improving `Button` forces a rebuild of every consumer that touches `nfui_controls`, even if the consumer only uses `ListBox`. Recompile noise drowns out the actual fix.
2. **Lessons don't transfer.** A fix landed for `Button` (e.g. `WM_DPICHANGED` font re-bind) is not visibly applicable to `ListBox` until somebody audits the sibling and ports the pattern by hand.
3. **Style depth is global.** Today `ThemePalette` has 14 fields consumed by every control. A control wanting a CSS-like axis — e.g. `Button::corner_radius`, `ListBox::row_height`, `ListView::gridline_color` — has no extension point; it must either globalize the token (polluting every other consumer) or hand-roll a one-off.

A previous subagent conversation sketched a four-layer "foundation + components + templates + products" model but it never made it into a spec; this document captures and ratifies it.

## Goal

Restructure `NativeFrameUI` into a **four-layer architecture** where:

| Layer | Role | Example targets |
|---|---|---|
| **Foundation** | Cross-cutting base types, platform adapters, dependency-free primitives | `nfui_core`, `nfui_theme` |
| **Components** | One `STATIC` lib per UI component (or per closely-related component group) | `nfui_button`, `nfui_text`, `nfui_listbox`, `nfui_listview`, `nfui_iconview`, `nfui_charts_*` |
| **Templates** | Composed UI building blocks that assemble components into reusable shells | `nfui_settings_form`, `nfui_sidebar_shell`, `nfui_chart_dashboard` |
| **Products** | Sample executables that assemble templates + components into finished UIs | `NativeFrameUIWorkbench`, `NativeFrameUIDarkStudio`, `NativeFrameUISettingsDemo`, `NativeFrameUIResourceGallery`, `NativeFrameUICharts`, `NativeFrameUIControls`, `NativeFrameUIShowcase` |

**Why per-component libraries** (the user's primary motivation):

- **Generalization compounds.** When `Button` polishing surfaces a reusable pattern (hover-track via `WM_MOUSEMOVE` + `TrackMouseEvent`, `BufferedPaintContext`, palette-aware `on_paint`), the same pattern can be lifted into `nfui_core` as a `HoverTrackedControl` mixin or into a shared header `controls/internal/HoverState.hpp` and reused across siblings — *not* copied. Today each control re-implements hover tracking inline.
- **Failure isolation.** A regression in `nfui_charts` does not invalidate the `nfui_button` build; consumers that don't use charts keep building. This makes triage faster and CI signals more precise.
- **Per-component CSS-style style slots.** Each component library owns a `*Style` struct layered *over* the global `ThemePalette`. A consumer (or a template) can override `ButtonStyle::corner_radius = 10` without touching `ThemePalette` or breaking other components. See §3.
- **Front-end-style polish.** A single component can be polished in isolation — animation timing, focus ring, press feedback, hover transition — with no risk of destabilizing siblings.

**Knowledge base** (the user's secondary motivation):

Add a persistent `docs/KNOWLEDGE/` tree to accumulate three categories of institutional knowledge:

1. **Problems + lessons** (carried forward from `docs/LEARNINGS/`, expanded). Each entry: trigger, root cause, fix, audit result, applies-to rule, prevention check.
2. **Product polish requirements** — concrete UX / interaction / motion items gathered from visual review, with status (`idea` / `approved` / `in-progress` / `shipped` / `won't-fix`).
3. **Competitive comparisons** — what other Win32 / desktop / web UI systems do for the same interaction (e.g. how does Chrome render its address-bar focus ring; how does Windows 11 Settings draw its combo; how does Firefox paint its toolbar buttons). Captures the "what does good look like" reference set.

The knowledge base is **separate from specs and plans**: specs commit, plans tick, knowledge accumulates over time.

## Non-goals

- **No source-level break of the existing public API** in the migration window. `nfui::Button`, `nfui::ListBox`, etc. keep their current namespaces, headers, and signatures during the split. Only the **library boundary** changes.
- **No new UI components in this cycle.** The split moves code; it does not add `DatePicker` or `TreeView` polish.
- **No template libraries in this cycle's first PR.** Templates land in a follow-up spec once components are split cleanly. This spec designs the component split + foundation + knowledge base only.
- **No build-system rewrite.** We keep CMake Presets + the existing `cmake/nfui_*.cmake` pattern. New per-component libs follow the same template.
- **No ABI guarantees across the split.** This is a build-time rearrangement; consumers that include `nfui/Controls.hpp` still get the same symbols compiled into the same output name (`NativeFrameUI.lib`). Per-component libs are an internal organization tool, not a public surface yet.

## Layer 1 — Foundation (unchanged + extended)

Existing `nfui_core` and `nfui_theme` stay. The split exposes **two new cross-cutting helpers** in `nfui_core` that the components will rely on:

### 1.1 `nfui::HoverState` (new, in `nfui_core`)

```cpp
// include/nfui/HoverState.hpp
namespace nfui {
class HoverState {
public:
    void attach(HWND hwnd) noexcept;
    void detach() noexcept;
    [[nodiscard]] bool hover() const noexcept { return hover_; }
    [[nodiscard]] bool pressed() const noexcept { return pressed_; }
    void on_mouse_move(HWND hwnd) noexcept;
    void on_mouse_leave() noexcept;
    void on_lbutton_down() noexcept;
    void on_lbutton_up() noexcept;
private:
    bool hover_{false};
    bool pressed_{false};
};
}
```

Lifts the `WM_MOUSEMOVE` + `TrackMouseEvent` + `WM_MOUSELEAVE` + `InvalidateRect(FALSE)` pattern out of any specific component. Every per-component control uses this rather than re-implementing it.

### 1.2 `nfui::OwnerDrawDC` (new, in `nfui_core`)

```cpp
// include/nfui/Paint.hpp  (additive)
class OwnerDrawDC {
public:
    OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept;  // wraps BeginPaint + GetClientRect + MemoryDC
    ~OwnerDrawDC() noexcept;
    [[nodiscard]] HDC dc() const noexcept;
    [[nodiscard]] RECT bounds() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
};
```

Replaces the manual `case WM_PAINT { BeginPaint; GetClientRect; { MemoryDC ... } EndPaint; }` block. The scope-before-EndPaint invariant is enforced inside the class.

These two helpers are **the concrete realization** of "polish patterns from one component lift into shared code for siblings to reuse".

## Layer 2 — Components (the new split)

`nfui_controls` is **dismantled** into per-component `STATIC` libraries. Each component owns its header, source, CMake module, and (new) style struct.

### 2.1 Component list and library boundaries

| Library | Header(s) | Component class(es) | Style struct |
|---|---|---|---|
| `nfui_button` | `Controls/Button.hpp` | `Button` | `ButtonStyle` |
| `nfui_checkbox` | `Controls/CheckBox.hpp` | `CheckBox` | `CheckBoxStyle` |
| `nfui_radio` | `Controls/RadioButton.hpp` | `RadioButton` | `RadioButtonStyle` |
| `nfui_text` | `Controls/StaticText.hpp`, `Controls/Edit.hpp` | `StaticText`, `Edit` | `TextStyle` (shared — both are text renderers) |
| `nfui_listbox` | `Controls/ListBox.hpp`, `Controls/ComboBox.hpp` | `ListBox`, `ComboBox` | `ListStyle` (shared) |
| `nfui_listview` | `Controls/ListView.hpp` | `ListView` | `ListViewStyle` |
| `nfui_treeview` | `Controls/TreeView.hpp` | `TreeView` | `TreeViewStyle` |
| `nfui_iconview` | `Controls/IconView.hpp` | `IconView` | `IconViewStyle` |
| `nfui_frame` | `Controls/StatusBar.hpp`, `Controls/TabControl.hpp`, `Controls/Tooltip.hpp`, `Controls/ProgressBar.hpp`, `Controls/Panel.hpp`, `Controls/Splitter.hpp` | `StatusBar`, `TabControl`, `Tooltip`, `ProgressBar`, `Panel`, `Splitter` | `FrameStyle` |
| `nfui_charts` | `Charts.hpp` | `ChartView` (base), `BarChartView`, `HBarChartView`, `LineChartView`, `SplineChartView` | `ChartStyle` |
| `nfui_charts_aa` | (private) | GDI+ AA helpers | (n/a) |

**Decision rationale**:

- `StaticText` + `Edit` share `TextStyle` — both render text with selection / disabled / focus states over a flat surface. Common style abstraction reduces duplication.
- `ListBox` + `ComboBox` share `ListStyle` — combo's drop-down is a themed list. Same row rendering, same selection highlight.
- `StatusBar` + `TabControl` + `Tooltip` + `ProgressBar` + `Panel` + `Splitter` are grouped into `nfui_frame` because they're frame-level chrome, share a low polish bar in V1 (native chrome retained per V1.1 limitations), and individually wouldn't justify a per-component lib. **Re-evaluation criterion**: if any of them later gets owner-draw polish (the V1.4 visual-controls plan covers several), split it out then.
- `nfui_charts` is **kept as one lib** for now (rather than per-chart-kind) because all four chart kinds share one geometry helper library (`compute_chart_layout`, `catmull_rom_to_bezier`) and one paint helper (`draw_legend_column`). Per-chart-kind split is a V2 move if individual chart polish demands it.

### 2.2 Per-component style slot (CSS-style theming)

Each component exposes a `*Style` struct layered over `ThemePalette`. A style struct has three buckets:

```cpp
// Example: ButtonStyle
struct ButtonStyle {
    // Inherited from the active theme — auto-populated from ThemePalette.
    Color face_idle          { /* accent.rgb */ };
    Color face_hover         { /* accent_hover.rgb */ };
    Color face_pressed       { /* accent_hover.rgb */ };
    Color face_disabled      { /* border.rgb */ };
    Color text_idle          { /* accent_text.rgb */ };

    // Component-local overrides — empty == use inherited.
    std::optional<int>   corner_radius;       // px
    std::optional<int>   horizontal_padding;  // px
    std::optional<int>   vertical_padding;    // px
    std::optional<Color> border_color;
    std::optional<FontFace> font_face;        // override the regular/semibold choice
};
```

Resolution at paint time:

1. Start from `ThemePalette` defaults.
2. If a consumer called `set_style(...)`, overlay consumer overrides.
3. Use the resolved values.

This is the **CSS-style axis** the user described: a consumer can hand a `ButtonStyle{corner_radius=10, border_color=Color{...}}` to one specific button without polluting every other button or every other component.

**Rule**: style struct fields are **read at paint time**, not cached on the HFONT / brush. This avoids the DPI-change invalidation problem that already bit the project (see `docs/LEARNINGS/2026-07-fontcache-dpi-audit.md`).

### 2.3 Per-component library CMake template

Each component follows a single template:

```cmake
# cmake/nfui_button.cmake
add_library(nfui_button STATIC
    src/controls/Button.cpp
)
add_library(NativeFrameUI::nfui_button ALIAS nfui_button)
target_include_directories(nfui_button
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_button
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_button)
```

**Required `PUBLIC` deps for every component**: `nfui_core` + `nfui_theme`.

**Allowed `PRIVATE` deps per component**:
- `nfui_button`, `nfui_checkbox`, `nfui_radio`, `nfui_text` — no extra deps.
- `nfui_listbox`, `nfui_listview` — `nfui_text` (for row text rendering — *or* keep text rendering inlined and avoid the dep; **decision pending**).
- `nfui_treeview`, `nfui_iconview` — `nfui_iconview` for tree node icons only if `nfui_treeview` wants a node-icon pattern; otherwise no extra deps.
- `nfui_charts` — `nfui_text` if axis labels go through shared text rendering (likely yes).
- `nfui_charts_aa` — `gdiplus` (PRIVATE).

**Forbidden deps across components**: a component lib cannot `PUBLIC` link another component lib. Cross-component dependencies (e.g. `ListBox` delegates to `StaticText` for labels) flow through `nfui_core` (style helpers) or are inlined. This keeps each component independently consumable.

### 2.4 Component namespace boundary

All existing public symbols (`nfui::Button`, `nfui::StaticText`, etc.) keep their full path. Each component's source files live in `src/controls/<component>/` (new tree) and headers in `include/nfui/Controls/<Component>.hpp` (new sub-namespace). The old `include/nfui/Controls.hpp` umbrella becomes a forwarding header that `#include`s all per-component headers, so existing `#include <nfui/Controls.hpp>` keeps working.

**Header layout**:

```text
include/nfui/
  Controls.hpp                 ← forwarding umbrella (kept for back-compat)
  Controls/
    Button.hpp
    CheckBox.hpp
    RadioButton.hpp
    StaticText.hpp
    Edit.hpp
    ListBox.hpp
    ComboBox.hpp
    ListView.hpp
    TreeView.hpp
    IconView.hpp
    StatusBar.hpp
    TabControl.hpp
    Tooltip.hpp
    ProgressBar.hpp
    Panel.hpp
    Splitter.hpp
```

Source layout:

```text
src/controls/
  Button.cpp
  CheckBox.cpp
  RadioButton.cpp
  StaticText.cpp
  Edit.cpp
  ListBox.cpp
  ComboBox.cpp
  ListView.cpp
  TreeView.cpp
  IconView.cpp
  StatusBar.cpp
  TabControl.cpp
  Tooltip.cpp
  ProgressBar.cpp
  Panel.cpp
  Splitter.cpp
  internal/                    ← shared control internals
    HoverState.cpp             ← promoted to nfui_core (Layer 1)
    OwnerDrawDC.cpp            ← promoted to nfui_core (Layer 1)
    ControlBase.cpp            ← shared `Control` base plumbing
```

The monolithic `src/controls/Controls.cpp` is **deleted** after every component is migrated and tested.

### 2.5 Per-component isolation contract

Each component lib is independently testable. The smoke test gets one new assertion per component:

```cpp
// Each is a hidden-window create/destroy + style slot round-trip.
{
    nfui::Button b; /* create + set_style({corner_radius=10}) + hwnd() + destroy */;
    if (!b.valid()) return 200;
}
{
    nfui::StaticText t; /* create + set_text + set_style + destroy */;
    if (!t.valid()) return 201;
}
// ... one block per component ...
```

## Layer 3 — Templates (designed, not built this cycle)

Templates are pre-composed UI shells built from components. They are the natural unit of "this is a Settings form", "this is a Chart dashboard". A template exposes **a higher-level API** (e.g. `SettingsForm::add_row(label, control)`) and hides the layout + component-creation boilerplate.

### 3.1 Initial template list (V1.5 candidates)

| Template lib | Composes | API surface |
|---|---|---|
| `nfui_settings_form` | `nfui_text` + `nfui_button` + `nfui_checkbox` + `nfui_listbox` (combo) | `add_row(label, control)`, `validate()`, `apply()` |
| `nfui_sidebar_shell` | `nfui_treeview` + `nfui_panel` + `nfui_splitter` | `set_root(node)`, `on_select(callback)` |
| `nfui_chart_dashboard` | `nfui_charts` + `nfui_listbox` (legend list) + `nfui_text` (caption) | `set_chart(kind)`, `set_data(series)`, `set_caption(text)` |
| `nfui_toolbar_shell` | `nfui_button` + `nfui_iconview` (for tool icons) | `add_action(id, label, icon)` |
| `nfui_inspector_panel` | `nfui_text` + `nfui_listview` + `nfui_button` | right-side property-list shell |

### 3.2 Template API rule

Templates are **header-only** in V1 (they're composition + glue, no extra runtime state beyond what the underlying components own). This keeps their build cost zero — a consumer that doesn't link a template pays nothing.

Templates expose **no `HWND` of their own** in V1. They build children inside the parent's HWND namespace; the parent still owns the window.

## Layer 4 — Products (samples — unchanged surface)

The seven sample executables keep their existing top-level files and structure. They switch from linking `NativeFrameUI::NativeFrameUI` (umbrella) to a curated subset:

```cmake
# Example: NativeFrameUICharts sample
target_link_libraries(NativeFrameUICharts
    PRIVATE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_charts
        NativeFrameUI::nfui_charts_aa
)
# No nfui_button / nfui_listbox pulled in.
```

The `NativeFrameUI` umbrella stays as a **convenience target** for samples that genuinely use most components (Workbench, Controls gallery). It is no longer the **only** way to link.

## Knowledge base structure

New `docs/KNOWLEDGE/` tree, organized by category. Each entry is a markdown file with stable frontmatter for filtering:

```text
docs/KNOWLEDGE/
  INDEX.md                     ← searchable index, one line per entry
  problems/                    ← was docs/LEARNINGS/; renamed + expanded
    2026-07-paint-cycle.md    (was 2026-07-paint-cycle-audit.md)
    2026-07-fontcache-dpi.md  (was 2026-07-fontcache-dpi-audit.md)
    YYYY-MM-<slug>.md
  polish/                      ← product polish requirements
    INDEX.md                   ← status board (idea / approved / in-progress / shipped / won't-fix)
    2026-07-button-press-feedback.md
    2026-07-listbox-row-hover.md
    YYYY-MM-<slug>.md
  competitive/                 ← competitive comparisons
    INDEX.md                   ← comparison matrix
    chrome-addressbar.md
    win11-settings-combo.md
    firefox-toolbar-button.md
    YYYY-MM-<product>-<feature>.md
```

### 5.1 `problems/<slug>.md` schema

```markdown
---
title: <one line>
date: YYYY-MM-DD
trigger: <commit-id or symptom description>
status: resolved | open | monitoring
tags: [paint, dpi, layout, ...]
related: [[other-slug]]
---

## Symptom
<observable behavior>

## Root cause
<technical explanation>

## Fix
<commit id + diff summary>

## Audit result
<what was checked across the codebase>

## Applies to
<what other places need the same fix>

## Prevention check
<mechanical or review-time check>
```

The existing two `docs/LEARNINGS/` files move to `docs/KNOWLEDGE/problems/` with the new schema. `docs/LEARNINGS/` is removed (with a forwarding note in `INDEX.md` for one cycle).

### 5.2 `polish/<slug>.md` schema

```markdown
---
title: <one line>
date: YYYY-MM-DD
status: idea | approved | in-progress | shipped | wont-fix
priority: p0 | p1 | p2 | p3
component: nfui_button | nfui_text | ... | cross
related-products: [NativeFrameUIWorkbench, NativeFrameUIControls, ...]
---

## Observation
<what is currently wrong / subpar, with screenshot if relevant>

## Target behavior
<what it should look like / feel like>

## Acceptance criteria
- [ ] <measurable criterion>
- [ ] <measurable criterion>

## Reference
<links to competitive comparisons, prior art, screenshots>

## Status log
- YYYY-MM-DD: idea captured
- YYYY-MM-DD: approved for V1.5
```

The polish `INDEX.md` is a status board listing all open items with priority + status, sorted by priority then date. This becomes the visual-review backlog.

### 5.3 `competitive/<slug>.md` schema

```markdown
---
title: <product feature>
date: YYYY-MM-DD
products: [Chrome, Win11-Settings, Firefox, VSCode, ...]
tags: [focus-ring, press-feedback, ...]
---

## What they do
<observed behavior across the listed products>

## What works well
<specific UX wins>

## What doesn't work
<specific UX failures to avoid>

## Applicable to nfui
<which components + which polish backlog items this informs>
```

The competitive `INDEX.md` is a matrix: rows are UX behaviors (focus ring, press feedback, hover transition, scroll feel, …), columns are products, cells are link to detailed entries.

## Migration plan (commit sequence)

This is the **execution order** for the split. Each commit is independently buildable + tested. No commit breaks the umbrella.

```
M1. Extract HoverState + OwnerDrawDC into nfui_core       (Layer 1)
M2. Split Button → nfui_button                             (Layer 2)
M3. Split CheckBox → nfui_checkbox
M4. Split RadioButton → nfui_radio
M5. Split StaticText + Edit → nfui_text
M6. Split ListBox + ComboBox → nfui_listbox
M7. Split ListView → nfui_listview
M8. Split TreeView → nfui_treeview
M9. Split IconView → nfui_iconview
M10. Split StatusBar + TabControl + Tooltip + ProgressBar + Panel + Splitter → nfui_frame
M11. Move Controls.hpp → forwarding umbrella; delete monolithic Controls.cpp
M12. Switch each sample to per-component link list; keep NativeFrameUI umbrella as convenience
M13. Rename docs/LEARNINGS/ → docs/KNOWLEDGE/problems/ + create polish/ + competitive/
M14. Migrate two existing learnings to new schema; back-fill INDEX.md
```

Each `Mn` is one PR; smoke test gets the new per-component assertions at the matching step. CI boundary check stays green throughout (`tools/verify_boundaries.ps1` does not look at `cmake/` directory layout).

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Per-component lib link-error storm on first try | Add `M0` dry-run: build with new lib files added to existing `nfui_controls` CMake list (no split), prove it compiles, then split. |
| Existing samples that include `<nfui/Controls.hpp>` break | Forwarding umbrella keeps working. Smoke test asserts both `#include <nfui/Controls.hpp>` and `#include <nfui/Controls/Button.hpp>` work. |
| Style-slot round-trip silently ignored when optional is unset | Smoke test asserts `ButtonStyle{}.corner_radius == std::nullopt` and that `Button::style()` round-trips a non-default value. |
| `docs/KNOWLEDGE/problems/` rename breaks existing links | Add 301-style forwarding note in `docs/LEARNINGS/INDEX.md` for one release; add a redirect comment in any cross-doc link. |
| Component lib dependency graph accidentally becomes a DAG with cycles | Mechanical check: a CMake-time script walks `target_link_libraries(... NativeFrameUI::nfui_*)` and rejects any forward edge into another component lib. |

## Acceptance criteria

- [ ] `nfui_core` exposes `HoverState` and `OwnerDrawDC`; both have smoke-test coverage.
- [ ] Each per-component lib (`nfui_button`, `nfui_text`, `nfui_listbox`, `nfui_listview`, `nfui_iconview`, `nfui_charts`, plus the two grouped ones) builds and tests independently.
- [ ] The `NativeFrameUI` umbrella still works for legacy consumers.
- [ ] Each sample links only the components it uses (verified by inspecting `target_link_libraries`).
- [ ] `docs/KNOWLEDGE/` exists with `problems/`, `polish/`, `competitive/` subtrees and `INDEX.md` in each.
- [ ] The two existing `LEARNINGS` entries are migrated to the new schema with frontmatter.
- [ ] At least one new `polish/` entry exists (captured from the V1.4 visual-controls backlog).
- [ ] At least one new `competitive/` entry exists (a reference comparison).
- [ ] Boundary check, both presets, full smoke test, all green.

## Open questions for user review

1. **`nfui_listbox` ↔ `nfui_text` dependency** — does `ListBox` row text render through a shared `draw_text` helper from `nfui_text`, or stay inlined? Affects whether `nfui_listbox` PUBLIC-links `nfui_text` or not. Recommendation: keep inlined in V1.5 to avoid the cross-component edge; promote to shared helper in a later cycle if profiling shows it worth it.
2. **Should `nfui_frame` (StatusBar/Tab/Tooltip/ProgressBar/Panel/Splitter) stay grouped, or split earlier?** Recommendation: stay grouped in this cycle; revisit when V1.4 visual-controls work lands on any of them.
3. **`docs/KNOWLEDGE/problems/` vs. keep `docs/LEARNINGS/`** — rename outright (recommended) or keep `LEARNINGS` as a thin alias. Recommendation: rename, since the new schema is materially different.
4. **Style-slot default resolution** — when a consumer sets `ButtonStyle{corner_radius=10}` and leaves everything else default, are unspecified fields "inherit from ThemePalette" or "inherit from a per-component baked default"? Recommendation: layered (theme → per-component default → consumer override), in that order. Matches CSS cascade intuition.
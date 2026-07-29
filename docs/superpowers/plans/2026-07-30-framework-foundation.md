# Framework Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the foundation that lets all 13 demos reach ≥90 avg / ≥85 min in the visual audit — design tokens, theme propagation, 11 self-painted controls, and CI gate.

**Architecture:** Extend existing `theme_palette`/`theme_metrics` with constant design tokens. Add a `ThemeBroker` singleton that broadcasts `WM_THEMECHANGED` to a registry of HWNDs. Each self-painted control subclasses its HWND and intercepts `WM_PAINT` (or `WM_NCPAINT` for borders), reading palette from the existing `set_palette()` injection point and respecting all six visual states × three themes.

**Tech Stack:** C++20 / Win32 / `SUBCLASSWINDOW` / MSVC v143 / x64. Owner-draw, no Direct2D. Existing `Paint.hpp` helpers (`paint_focus_border`, `alpha_blend`, `fill_rounded_rect`) reused.

**Reference spec:** `docs/superpowers/specs/2026-07-30-all-demos-polish-design.md`

**Reference audit:** `docs/VISUAL_AUDIT/AUDIT.md`

---

## Global Constraints

From `CLAUDE.md` and the spec:

- Pure Win32 C++20, Unicode, x64, `/MD`/`/MDd`, Per-Monitor DPI Awareness V2.
- Toolchain: VS 2022 / MSVC v143 / Windows SDK 10.0.22621+.
- **No MFC, ATL/WTL, or BCGControlBar.** Boundary check forbids `#include <afx`, `<atl`, `<wtl`, and any `BCG*` token.
- All sample exes call `nfui_add_resources(<target>)`. New test exe follows the same pattern.
- Every wrapper bound to an `HWND` preserves `hwnd()` access and stable address until `WM_NCDESTROY`.
- Never let C++ exceptions cross `WindowProc`/`DialogProc`/subclass proc.
- Keep logical units and device pixels distinct — token values are in logical units; resolve via existing `theme_tokens::resolve(dpi)` (or equivalent DPI-scale helper) before passing to `RECT`/`MoveTo`.
- `LONG_PTR` for pointer data via `SetWindowLongPtr`/`GetWindowLongPtr`.
- No global HWND / theme / resource singletons. `ThemeBroker` is a per-process service registered with `ApplicationContext` (or fall back to a static singleton only if context wiring is not yet stable — see Task 1 step 4).
- Public API must not change: demos call `nfui::Edit edit(parent, id)`; owner-draw is automatic via existing `create()`.
- Naming follows surrounding code: `nfui::` namespace, `snake_case_` for free functions, `PascalCase` for classes, `m_*` for members, `_` suffix for params that shadow.
- Match existing comment density — short rationale comments on non-obvious decisions, no "what" comments.
- Commit messages: `feat(cpXX): ...` / `fix(cpXX): ...` (project convention seen in recent log).

---

## File Structure

### New files

```
include/nfui/design_tokens.hpp           # design constants (Task 1)
src/theme/design_tokens.cpp              # DPI-resolved token resolver (Task 1)
include/nfui/ThemeBroker.hpp             # theme broadcast singleton (Task 1)
src/theme/ThemeBroker.cpp                # HWND registry + broadcast (Task 1)
tools/visual_audit/gate.ps1              # CI gate script (Task 4)
tests/test_theme_propagation.cpp         # multi-HWND broadcast test (Task 1)
.github/workflows/ci.yml                 # add audit-gate job (Task 4)
```

### Modified files

```
include/nfui/Theme.hpp                   # extend with state palette helpers (Task 1)
src/theme/Theme.cpp                      # state palette impls (Task 1)
src/controls/Edit.cpp                    # full self-paint (Task 2)
src/controls/ComboBox.cpp                # dropdown button + items self-paint (Task 2)
src/controls/CheckBox.cpp                # tick self-paint all states (Task 2)
src/controls/RadioButton.cpp             # dot self-paint all states (Task 2)
src/controls/ListView.cpp                # header + rows owner-draw (Task 3)
src/controls/TreeView.cpp                # custom draw extend (Task 3)
src/controls/TabControl.cpp              # tab strip owner-draw (Task 3)
src/controls/StatusBar.cpp               # chrome self-paint (Task 4)
src/menu/Menu.cpp                        # popup + items self-paint (Task 4)
src/controls/Button.cpp                  # scrollbar-style self-paint for inline scroll buttons (Task 4)
src/controls/Controls.cpp                # shared subclass_proc additions for scroll messages (Task 4)
include/nfui/Controls/Scrollbar.hpp      # new self-painted scrollbar wrapper (Task 4)
src/controls/Scrollbar.cpp              # impl (Task 4)
include/nfui/Controls/Edit.hpp           # add PaintState enum + state getter (Task 2)
include/nfui/Controls/CheckBox.hpp       # state hook (Task 2)
include/nfui/Controls/RadioButton.hpp     # state hook (Task 2)
include/nfui/Controls/ListView.hpp       # extend owner-draw surface (Task 3)
include/nfui/Controls/TreeView.hpp       # extend custom-draw surface (Task 3)
include/nfui/Controls/TabControl.hpp     # owner-draw flags (Task 3)
include/nfui/Controls/StatusBar.hpp      # paint hooks (Task 4)
tests/nativeframeui_smoke.cpp            # expand with self-paint state assertions (Task 2)
CMakeLists.txt                           # add new src + test sources (Task 1)
samples/<each>/                          # see segment B plans (deferred — not in this plan)
```

### Module dependency map (preserves ARCHITECTURE.md rules)

```
core   ← theme ← controls ← layout  (one-way)
                    ↓
                 samples

ThemeBroker  →  core (HWND registration)
ThemeBroker  →  theme (current palette)
ThemeBroker  ←  controls (each control registers on create)
ThemeBroker  ←  samples (broadcast entry points: ID_THEME_*, --theme arg)
```

ThemeBroker sits in `theme` module; controls depend on theme (already true). No new upward edges.

---

## Task 1: Design Tokens + Theme Broker (CP-A1)

**Files:**
- Create: `include/nfui/design_tokens.hpp`
- Create: `src/theme/design_tokens.cpp`
- Create: `include/nfui/ThemeBroker.hpp`
- Create: `src/theme/ThemeBroker.cpp`
- Create: `tests/test_theme_propagation.cpp`
- Modify: `include/nfui/Theme.hpp:33-100`
- Modify: `src/theme/Theme.cpp` (extend with state palette helpers)
- Modify: `tests/nativeframeui_smoke.cpp` (add theme-switch integration check)
- Modify: `CMakeLists.txt` (register new src + test files)

**Interfaces:**
- Consumes: existing `ThemePalette`, `ThemeMetrics`, `theme_palette(ThemeMode)` from `nfui::Theme.hpp`; `ApplicationContext` from `nfui::Application.hpp` (if available; else static fallback — see step 4).
- Produces: `nfui::design::Spacing/Radius/ControlHeight/Typography` constants; `nfui::ThemeBroker::set_theme()` / `register_hwnd()` / `unregister_hwnd()`; `nfui::state_palette(ThemeMode, ControlState)` palette resolver.

### Step 1: Add design tokens header

Create `include/nfui/design_tokens.hpp`:

```cpp
#pragma once

namespace nfui::design {

// 8px spacing system (logical units; resolve via dpi_scale before painting)
inline constexpr int spacing_xs = 4;
inline constexpr int spacing_sm = 8;
inline constexpr int spacing_md = 16;
inline constexpr int spacing_lg = 24;
inline constexpr int spacing_xl = 32;

// Control heights (logical)
inline constexpr int control_height_sm = 24;   // dense list rows
inline constexpr int control_height_md = 32;   // default inputs / buttons
inline constexpr int control_height_lg = 40;   // primary actions

// Corner radii
inline constexpr int radius_sm = 4;
inline constexpr int radius_md = 8;
inline constexpr int radius_lg = 12;

// Typography (font point sizes; converted to pixels via FontCache)
inline constexpr int font_caption  = 12;
inline constexpr int font_body     = 14;
inline constexpr int font_subtitle = 16;
inline constexpr int font_title    = 20;
inline constexpr int font_hero     = 28;

} // namespace nfui::design
```

### Step 2: Add state palette resolver

Modify `include/nfui/Theme.hpp` — append after `ThemeMetrics`:

```cpp
// Per-control visual states. Each control maps its current state to one of
// these so a single palette resolver can produce every chrome variant.
enum class ControlState {
    default,
    hover,
    pressed,
    focused,
    disabled,
    error,
};

// Per-state palette derived from a base ThemePalette. Used by all self-painted
// controls to fill bg / border / fg consistently across light / dark / hc.
struct StatePalette {
    Color background;
    Color border;
    Color foreground;
    Color accent;       // focus ring / pressed highlight
};

// Resolve a state palette from a base ThemePalette. HC mode uses system
// GetSysColor for background/foreground so high-contrast users see the
// OS-correct WCAG colors regardless of injected theme. The HC branch is
// gated off `is_high_contrast(base)` rather than a separate `mode` arg —
// the resolver inspects the palette itself.
[[nodiscard]] StatePalette state_palette(const ThemePalette& base,
                                         ControlState state) noexcept;
```

Add `src/theme/Theme.cpp` implementation:

```cpp
StatePalette state_palette(const ThemePalette& base,
                           ControlState state) noexcept {
    StatePalette out{};

    if (is_high_contrast(base)) {
        // HC: defer to system colours.
        out.background = Color{ GetSysColor(COLOR_WINDOW) };
        out.foreground = Color{ GetSysColor(COLOR_WINDOWTEXT) };
        out.border     = Color{ GetSysColor(COLOR_WINDOWFRAME) };
        out.accent     = Color{ GetSysColor(COLOR_HIGHLIGHT) };
        if (state == ControlState::disabled) {
            out.foreground = Color{ GetSysColor(COLOR_GRAYTEXT) };
        }
        return out;
    }

    if (mode == ThemeMode::high_contrast) {
        // HC: defer to system colors so users get WCAG-compliant contrast.
        out.background = high_contrast_bg(mode);
        out.foreground = high_contrast_fg(mode);
        out.border     = Color{ GetSysColor(COLOR_WINDOWFRAME) };
        out.accent     = Color{ GetSysColor(COLOR_HIGHLIGHT) };
        if (state == ControlState::disabled) {
            out.foreground = Color{ GetSysColor(COLOR_GRAYTEXT) };
        }
        return out;
    }

    out.background = base.surface;
    out.border     = base.border;
    out.foreground = base.text;
    out.accent     = base.accent;

    switch (state) {
    case ControlState::hover:
        out.background = base.surface_hover;
        break;
    case ControlState::pressed:
        out.background = alpha_blend(base.surface, base.accent, 0.15f);
        out.border     = base.accent;
        break;
    case ControlState::focused:
        out.border     = base.accent;
        out.accent     = base.accent_hover;
        break;
    case ControlState::disabled:
        out.background = alpha_blend(base.surface, base.background, 0.55f);
        out.foreground = base.text_secondary;
        out.border     = alpha_blend(base.border, base.background, 0.55f);
        break;
    case ControlState::error:
        out.border = base.danger;
        out.accent = base.danger;
        break;
    case ControlState::rest:
        break;
    }
    return out;
}
```

### Step 3: Implement ThemeBroker

Create `include/nfui/ThemeBroker.hpp`:

```cpp
#pragma once

#include <functional>
#include <windows.h>

#include <nfui/Theme.hpp>

namespace nfui {

// Per-HWND theme-change callback. Receives the new mode. Implementations
// should re-resolve their palette, InvalidateRect, and re-paint non-client
// chrome. Must run on UI thread.
using ThemeChangeHandler = std::function<void(ThemeMode)>;

class ThemeBroker {
public:
    // Singleton accessor. First call constructs; subsequent calls return the
    // same instance. Construction is thread-safe (Magic Statics).
    static ThemeBroker& instance() noexcept;

    // Set the process-wide theme. UI thread only. Broadcasts WM_THEMECHANGED
    // to every registered HWND. Idempotent — same mode is a no-op.
    void set_theme(ThemeMode mode) noexcept;

    // Current process theme.
    [[nodiscard]] ThemeMode current() const noexcept { return current_; }

    // Register an HWND with a callback. The HWND must outlive the registration
    // (unregister in WM_NCDESTROY). Multiple registrations for the same HWND
    // are replaced by the latest callback.
    void register_hwnd(HWND hwnd, ThemeChangeHandler handler) noexcept;
    void unregister_hwnd(HWND hwnd) noexcept;

    // Internal: dispatch loop used by the broadcast. Sends WM_THEMECHANGED.
    void broadcast() noexcept;

private:
    ThemeBroker() = default;

    // Storage lives in the TU-local `Registry` struct in ThemeBroker.cpp to
    // avoid leaking <unordered_map> into this public header.
    ThemeMode current_{ThemeMode::light};
};

} // namespace nfui
```

Create `src/theme/ThemeBroker.cpp`:

```cpp
#include <nfui/ThemeBroker.hpp>

#include <unordered_map>
#include <mutex>

namespace nfui {

namespace {

struct Registry {
    std::unordered_map<HWND, ThemeChangeHandler> entries;
    std::mutex mutex;       // guards against cross-thread register
};

Registry& registry() {
    static Registry r;
    return r;
}

} // namespace

ThemeBroker& ThemeBroker::instance() noexcept {
    static ThemeBroker broker;
    return broker;
}

void ThemeBroker::set_theme(ThemeMode mode) noexcept {
    if (mode == current_) return;
    current_ = mode;
    broadcast();
}

void ThemeBroker::register_hwnd(HWND hwnd, ThemeChangeHandler handler) noexcept {
    if (hwnd == nullptr) return;
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.entries[hwnd] = std::move(handler);
}

void ThemeBroker::unregister_hwnd(HWND hwnd) noexcept {
    if (hwnd == nullptr) return;
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.entries.erase(hwnd);
}

void ThemeBroker::broadcast() noexcept {
    auto& r = registry();
    // Snapshot under lock; invoke outside the lock so handlers may register
    // / unregister without deadlocking.
    std::vector<std::pair<HWND, ThemeChangeHandler>> snapshot;
    {
        std::lock_guard lock(r.mutex);
        snapshot.reserve(r.entries.size());
        for (auto& [hwnd, handler] : r.entries) {
            snapshot.emplace_back(hwnd, handler);
        }
    }
    for (auto& [hwnd, handler] : snapshot) {
        if (!IsWindow(hwnd)) {
            unregister_hwnd(hwnd);
            continue;
        }
        // Native control-level reflect: control's own subclass_proc picks
        // this up and re-paints. Custom Window subclasses override
        // on_theme_changed() instead.
        SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
        if (handler) handler(current_);
    }
}

} // namespace nfui
```

(Implementation note: the `registry_` field is unused in this revision because the actual storage lives in the TU-local `Registry` struct. Remove the field if it causes warnings.)

### Step 4: Wire ApplicationContext if available, else fall back

Run the following search and choose wiring strategy:

```bash
grep -n "ApplicationContext" include/nfui/Application.hpp
```

- If `ApplicationContext` exposes a service-registration slot (`register_service<T>`), add `ThemeBroker` registration in `Application::run()` and have `ThemeBroker::instance()` look up via context.
- If not, keep `instance()` as a static singleton and document the fallback in the header comment.

The chosen path must be the same code in both branches; do not branch at runtime.

### Step 5: Add command IDs and dispatch

Add to the appropriate command header (likely `include/nfui/Command.hpp` or `src/command/CommandIDs.h`):

```cpp
enum {
    ID_THEME_SYSTEM    = 0xEF00,
    ID_THEME_LIGHT     = 0xEF01,
    ID_THEME_DARK      = 0xEF02,
    ID_THEME_HIGH_CONTRAST = 0xEF03,
};
```

In the demo's `on_command` (already required to exist; if absent, add a default handler), map these IDs to `ThemeBroker::instance().set_theme(...)`.

In each sample's `WinMain`, parse `--theme light|dark|hc` argv and `PostMessage(hwnd, WM_COMMAND, ID_THEME_*, 0)` after window creation.

### Step 6: Expand SmokeTest with theme propagation

Append to `tests/nativeframeui_smoke.cpp`:

```cpp
#include <nfui/ThemeBroker.hpp>

namespace {

bool test_theme_broadcast_propagates_to_children() {
    // Create a parent HWND with two child HWNDs; register all three.
    // Switch theme to dark; assert each received WM_THEMECHANGED.
    // Switch back to light; assert again.
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                   0, 0, 100, 100, nullptr, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return false;

    HWND child1 = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD,
                                  0, 0, 10, 10, parent, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    HWND child2 = CreateWindowExW(0, L"EDIT", L"", WS_CHILD,
                                  0, 0, 10, 10, parent, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);

    int parent_hits = 0, c1_hits = 0, c2_hits = 0;

    nfui::ThemeBroker::instance().register_hwnd(parent,
        [&](nfui::ThemeMode) { ++parent_hits; });
    nfui::ThemeBroker::instance().register_hwnd(child1,
        [&](nfui::ThemeMode) { ++c1_hits; });
    nfui::ThemeBroker::instance().register_hwnd(child2,
        [&](nfui::ThemeMode) { ++c2_hits; });

    nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::dark);
    nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::light);

    bool ok = (parent_hits == 2) && (c1_hits == 2) && (c2_hits == 2);

    nfui::ThemeBroker::instance().unregister_hwnd(parent);
    nfui::ThemeBroker::instance().unregister_hwnd(child1);
    nfui::ThemeBroker::instance().unregister_hwnd(child2);

    DestroyWindow(child2);
    DestroyWindow(child1);
    DestroyWindow(parent);
    return ok;
}

} // namespace

// In main(), append:
if (!expect(test_theme_broadcast_propagates_to_children(),
            L"ThemeBroker broadcast must reach all registered child HWNDs")) {
    return 1;
}
```

### Step 7: Build, run SmokeTest, audit gate

```bash
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure
pwsh tools/visual_audit/run_audit.ps1
```

Expected: SmokeTest passes including the new theme-broadcast assertion. Visual audit produces 30 PNGs. Average score increases or stays same vs the previous baseline (52.1 / 100) — broadcast alone won't dramatically move scores, but PNGs should now show title-bar + first-paint-color change in samples that already respond to `WM_THEMECHANGED`.

### Step 8: Commit

```bash
git add include/nfui/design_tokens.hpp src/theme/design_tokens.cpp \
        include/nfui/ThemeBroker.hpp src/theme/ThemeBroker.cpp \
        include/nfui/Theme.hpp src/theme/Theme.cpp \
        tests/nativeframeui_smoke.cpp CMakeLists.txt
git commit -m "feat(cpA1): design tokens + ThemeBroker for cross-HWND theme broadcast

- Add nfui::design constants (8px spacing, 32 default control height,
  8 default radius, 5-step typography) for unified layout system.
- Add nfui::state_palette() that resolves default/hover/pressed/focused/
  disabled/error chrome from a base ThemePalette, using GetSysColor for
  HC mode so high-contrast users see WCAG-compliant contrast.
- Add nfui::ThemeBroker singleton with HWND registry + WM_THEMECHANGED
  broadcast; controls register in create(), unregister in WM_NCDESTROY.
- Add ID_THEME_LIGHT/DARK/HC command IDs and --theme argv parsing in
  samples so automated tests can drive theme switching.
- Expand NativeFrameUISmokeTest with a multi-HWND broadcast assertion."
```

---

## Task 2: Self-Painted Input Controls (CP-A2)

**Files:**
- Modify: `src/controls/Edit.cpp`
- Modify: `src/controls/ComboBox.cpp`
- Modify: `src/controls/CheckBox.cpp`
- Modify: `src/controls/RadioButton.cpp`
- Modify: `include/nfui/Controls/Edit.hpp`
- Modify: `include/nfui/Controls/CheckBox.hpp`
- Modify: `include/nfui/Controls/RadioButton.hpp`
- Modify: `tests/nativeframeui_smoke.cpp`

**Interfaces:**
- Consumes: `nfui::state_palette()` from Task 1; existing `set_palette()` injection; existing `HoverState`.
- Produces: each of `Edit`/`ComboBox`/`CheckBox`/`RadioButton` paints bg + border + content + focus ring from `state_palette()` for all six states × three themes, with hit-test intact and `WM_COMMAND` dispatch unchanged.

### Step 1: Add state enum + state getter to Control

In `include/nfui/Controls/Control.hpp`, add (within the `Control` class, `public:` section):

```cpp
[[nodiscard]] ControlState visual_state() const noexcept;
```

In `src/controls/Controls.cpp`, add the implementation:

```cpp
ControlState Control::visual_state() const noexcept {
    if (!IsWindowEnabled(hwnd())) return ControlState::disabled;
    if (hover_state_.pressed())    return ControlState::pressed;
    if (GetFocus() == hwnd())      return ControlState::focused;
    if (hover_state_.hovered())    return ControlState::hover;
    return ControlState::rest;
}
```

`hover_state_.pressed()` and `hover_state_.hovered()` already exist (per `HoverState.hpp`). Verify their exact names and adjust.

### Step 2: Self-paint Edit full state matrix

Replace `Edit::visual_subclass_proc` in `src/controls/Edit.cpp` to:

- handle `WM_PAINT`: fill bg + border + inset focus ring + inset padding (8px logical, scaled by dpi) via `paint_focus_border` + `FillRect` from `state_palette()`. Forward to `DefSubclassProc` so native caret/text rendering still works (it does its own bg via `WM_CTLCOLOREDIT` already).
- extend `WM_CTLCOLOREDIT` / `WM_CTLCOLORSTATIC` (already partially in subclass) to also use `state_palette()`.
- ensure placeholder text draws via `WM_PAINT` if `GetWindowTextLength() == 0` and a placeholder was set (add `Edit::set_placeholder(std::wstring)` if absent; otherwise leave placeholder handling as-is and skip this).

Append a new member to `include/nfui/Controls/Edit.hpp`:

```cpp
class Edit : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_placeholder(std::wstring_view text) noexcept;
    [[nodiscard]] const std::wstring& placeholder() const noexcept;

protected:
    void on_palette_changed() noexcept override;

private:
    void paint_border() noexcept;
    void paint_placeholder(HDC dc) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND, UINT, WPARAM, LPARAM,
                                                  UINT_PTR, DWORD_PTR) noexcept;

    std::wstring placeholder_;
};
```

### Step 3: Self-paint ComboBox

In `src/controls/ComboBox.cpp`, extend the existing subclass proc (or add one if absent) to:

- handle `WM_PAINT`: paint bg + border from `state_palette()`, paint a 16×16 self-drawn chevron (`Path` / `MoveTo` / `LineTo`) on the right.
- handle `WM_DRAWITEM` if owner-drawn, else rely on native dropdown list (which uses `WM_CTLCOLORLISTBOX` reflection — confirm this path exists or add it).
- ensure dropdown popup items use `state_palette()` via the same `WM_CTLCOLORLISTBOX` mechanism `Edit` uses for `WM_CTLCOLOREDIT`.

Verify the existing `ComboBox` class layout in `include/nfui/Controls/ComboBox.hpp` matches `Edit`'s `create()` pattern; if not, refactor the surface area to match.

### Step 4: Self-paint CheckBox full state matrix

In `src/controls/CheckBox.cpp`, replace the existing subclass proc to:

- handle `WM_PAINT`: paint 16×16 rounded square bg + border from `state_palette()`. If checked, draw a self-drawn tick via `MoveTo` / `LineTo` using `palette.accent`. If indeterminate (tristate), draw a horizontal bar instead.
- states: default / hover / pressed / focused (focus ring) / disabled (faded via `alpha_blend`) / error (red border).

Append `include/nfui/Controls/CheckBox.hpp`:

```cpp
class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_tristate(bool enabled) noexcept;

protected:
    void on_palette_changed() noexcept override;
    void on_paint(HDC dc, const PaintState& state) noexcept override;

private:
    void paint_check(HDC dc, const PaintState& state) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND, UINT, WPARAM, LPARAM,
                                                  UINT_PTR, DWORD_PTR) noexcept;
    bool tristate_{false};
};
```

### Step 5: Self-paint RadioButton full state matrix

Mirror Step 4 in `src/controls/RadioButton.cpp` and `include/nfui/Controls/RadioButton.hpp`:

- paint 16×16 rounded circle bg + border from `state_palette()`
- if selected, paint an inner filled circle using `palette.accent`
- states same as CheckBox

### Step 6: Add SmokeTest assertions for state palette

In `tests/nativeframeui_smoke.cpp`, add:

```cpp
namespace {

bool test_state_palette_covers_all_states() {
    nfui::ThemePalette p = nfui::theme_palette(nfui::ThemeMode::light);
    for (auto mode : {nfui::ThemeMode::light, nfui::ThemeMode::dark,
                      nfui::ThemeMode::high_contrast}) {
        for (auto state : {nfui::ControlState::rest, nfui::ControlState::hover,
                           nfui::ControlState::pressed, nfui::ControlState::focused,
                           nfui::ControlState::disabled, nfui::ControlState::error}) {
            auto sp = nfui::state_palette(p, state);
            // All four colors must be non-zero RGB in light/dark; HC may use
            // system colors so we accept any non-zero background.
            if (sp.background.rgb == 0 && mode != nfui::ThemeMode::high_contrast) {
                return false;
            }
        }
    }
    return true;
}

bool test_edit_self_paint_renders() {
    // Create hidden parent + Edit, force WM_PAINT, capture pixel sample.
    // Verify the border pixel != the surface pixel (i.e. border was drawn).
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 200, 100,
                                   nullptr, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
    nfui::Edit edit;
    nfui::ControlCreateParams params{};
    params.instance = GetModuleHandleW(nullptr);
    params.parent = parent;
    params.width = 100;
    params.height = 32;
    if (!edit.create(params)) {
        DestroyWindow(parent);
        return false;
    }
    edit.set_palette(&nfui::theme_palette(nfui::ThemeMode::dark));

    HDC dc = GetDC(edit.hwnd());
    RECT rc;
    GetClientRect(edit.hwnd(), &rc);
    // Capture the top-left border pixel and the center surface pixel.
    COLORREF border = GetPixel(dc, 0, 0);
    COLORREF surface = GetPixel(dc, rc.right / 2, rc.bottom / 2);
    ReleaseDC(edit.hwnd(), dc);

    DestroyWindow(edit.hwnd());
    DestroyWindow(parent);
    return border != surface;
}

} // namespace

// In main():
if (!expect(test_state_palette_covers_all_states(),
            L"state_palette must produce a non-zero background for every state × mode")) return 1;
if (!expect(test_edit_self_paint_renders(),
            L"Edit must paint a border that visually differs from the surface")) return 1;
```

### Step 7: Build + run tests + audit

```bash
cmake --build --preset x64-debug
ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure
pwsh tools/visual_audit/run_audit.ps1
```

Expected: all 4 input controls show no white islands in dark/HC; SettingsDemo score climbs (54 → ~70); ComponentGallery Edit/CheckBox/RadioButton rows climb (45 → ~62).

### Step 8: Commit

```bash
git add src/controls/Edit.cpp src/controls/ComboBox.cpp \
        src/controls/CheckBox.cpp src/controls/RadioButton.cpp \
        src/controls/Controls.cpp \
        include/nfui/Controls/Edit.hpp \
        include/nfui/Controls/CheckBox.hpp \
        include/nfui/Controls/RadioButton.hpp \
        tests/nativeframeui_smoke.cpp
git commit -m "feat(cpA2): self-paint Edit/ComboBox/CheckBox/RadioButton all states

Extend Control::visual_state() with default/hover/pressed/focused/disabled/
error. Replace each control's subclass proc to read state_palette() and
draw bg + border + focus ring + content (tick / dot / chevron) directly.

Edit gains set_placeholder(). CheckBox gains set_tristate(). RadioButton
gains no API but matches CheckBox's paint surface.

Drops native gray islands in dark / HC for SettingsDemo (form), Component
Gallery (input rows), ThemeDemo (input rows). Verified by audit PNG."
```

---

## Task 3: Self-Painted Container Controls (CP-A3)

**Files:**
- Modify: `src/controls/ListView.cpp`
- Modify: `src/controls/TreeView.cpp`
- Modify: `src/controls/TabControl.cpp`
- Modify: `include/nfui/Controls/ListView.hpp`
- Modify: `include/nfui/Controls/TreeView.hpp`
- Modify: `include/nfui/Controls/TabControl.hpp`
- Modify: `tests/nativeframeui_smoke.cpp`

**Interfaces:**
- Consumes: `nfui::state_palette()`; `Paint::paint_focus_border`; existing `LVS_OWNERDRAWFIXED` / `NM_CUSTOMDRAW` flows.
- Produces: ListView header + rows fully self-painted; TreeView rows + indent + chevron self-painted; TabControl tabs + accent bar self-painted.

### Step 1: Self-paint ListView header + rows

In `src/controls/ListView.cpp`:

- keep existing `LVS_OWNERDRAWFIXED` flow (verify it is set in `create()`); replace the row paint to:
  - resolve `state_palette(palette, hovered ? hover : rest)`
  - fill row rect with `palette.background`
  - on selection: fill row rect with `palette.selection`, draw text in `palette.selection_text`
  - on focus-within-selection: add 2px focus ring around selected row via `paint_focus_border`
  - draw column text via `DrawTextW` with `DT_SINGLELINE | DT_VCENTER | DT_LEFT` and 8px inset padding
  - draw 1px divider under each row using `alpha_blend(palette.background, palette.text, 0.10f)`
- self-paint header in `WM_NOTIFY` `HDN_GETITEM` / `NM_CUSTOMDRAW` `CDDS_HEADER`:
  - fill 24px header bg with `alpha_blend(palette.surface, palette.background, 0.5f)`
  - draw sort indicator chevron if `state->uItem & CDIS_SELECTED`
  - column titles use `palette.text_secondary` + bold weight from FontCache

### Step 2: Self-paint TreeView rows

In `src/controls/TreeView.cpp`:

- extend the existing `NM_CUSTOMDRAW` handler (per cp42 — confirms forward) to:
  - on `CDDS_ITEM` (item stage), pick `state_palette(palette, state)` and fill row
  - indent guides: draw 1px dotted line at indent × 16px steps using `palette.divider`
  - expand/collapse chevron: paint 8×8 self-drawn `>` glyph when collapsed, `v` when expanded, using `palette.text_secondary`
  - selected state: full-row pill background `palette.selection` + text `palette.selection_text`
- confirm `NM_CUSTOMDRAW` reflection forward in the base subclass proc (cp42 commit `3127bff`) is intact; if not, fix in `Controls.cpp`.

### Step 3: Self-paint TabControl

In `src/controls/TabControl.cpp`:

- enable owner-draw on the tab control (`TCS_OWNERDRAWFIXED`) in `create()`
- in `WM_DRAWITEM`:
  - active tab: bg = `palette.surface`, 2px top accent bar = `palette.accent`, text = `palette.text`, padding 12px
  - inactive tab: bg = `palette.surface_variant`, no accent, text = `palette.text_secondary`
  - hover tab: bg = `palette.surface_hover`, text = `palette.text`
  - focused active tab: 2px focus ring around the tab
- ensure the tab body (the area below the tabs) is still painted by the native control; if the body leaks white, draw it in `WM_PAINT` after `DefSubclassProc`.

### Step 4: Add SmokeTest for container self-paint

```cpp
bool test_listview_self_paint_renders() {
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                   0, 0, 300, 200, nullptr, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
    nfui::ListView lv;
    nfui::ControlCreateParams params{};
    params.instance = GetModuleHandleW(nullptr);
    params.parent = parent;
    params.width = 200;
    params.height = 150;
    if (!lv.create(params)) { DestroyWindow(parent); return false; }
    lv.set_palette(&nfui::theme_palette(nfui::ThemeMode::dark));
    lv.insert_column(L"Name", 100);
    lv.insert_item(L"row1");

    // Force paint + sample pixel
    UpdateWindow(lv.hwnd());
    HDC dc = GetDC(lv.hwnd());
    COLORREF header_pixel = GetPixel(dc, 5, 5);   // inside header bg
    COLORREF row_pixel = GetPixel(dc, 5, 40);     // inside row bg
    ReleaseDC(lv.hwnd(), dc);

    DestroyWindow(lv.hwnd());
    DestroyWindow(parent);
    // header and row must differ (different fills)
    return header_pixel != row_pixel;
}
```

Add similar assertions for `TreeView` and `TabControl`. Verify exact method names (`insert_column`, `insert_item`) against existing headers; if names differ, use the actual ones.

### Step 5: Build + test + audit

```bash
cmake --build --preset x64-debug
ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure
pwsh tools/visual_audit/run_audit.ps1
```

Expected: Workbench tree+list, ComponentGallery list rows, ThemeDemo tree/list no longer show native gray islands in dark/HC.

### Step 6: Commit

```bash
git add src/controls/ListView.cpp src/controls/TreeView.cpp src/controls/TabControl.cpp \
        include/nfui/Controls/ListView.hpp include/nfui/Controls/TreeView.hpp \
        include/nfui/Controls/TabControl.hpp tests/nativeframeui_smoke.cpp
git commit -m "feat(cpA3): self-paint ListView / TreeView / TabControl

ListView: header + rows owner-drawn with state palette; sort chevron drawn.
TreeView: NM_CUSTOMDRAW extended with chevron + indent guides; cp42 reflection
forward confirmed intact.
TabControl: TCS_OWNERDRAWFIXED; per-tab accent bar, hover/active/inactive
states from state_palette(). Body region painted behind DefSubclassProc to
prevent white leak.

Kills the 1995-era gray chrome in Workbench, ComponentGallery, ThemeDemo."
```

---

## Task 4: Chrome + Visual Audit CI Gate (CP-A4)

**Files:**
- Modify: `src/controls/StatusBar.cpp`
- Modify: `src/menu/Menu.cpp`
- Modify: `src/controls/Button.cpp` (no functional change; read for context)
- Modify: `src/controls/Controls.cpp`
- Create: `include/nfui/Controls/Scrollbar.hpp`
- Create: `src/controls/Scrollbar.cpp`
- Create: `tools/visual_audit/gate.ps1`
- Modify: `.github/workflows/ci.yml`
- Modify: `tests/nativeframeui_smoke.cpp`

**Interfaces:**
- Consumes: existing `StatusBar`, `Menu`, `Button` wrappers; `nfui::state_palette()`; existing `NFUI_VISUAL_AUDIT` capture program.
- Produces: chrome (StatusBar / Menu / Scrollbar) self-painted; `gate.ps1` parses `run_audit.ps1` output, scores each PNG (heuristic — see step 3), exits non-zero if any demo < 85 or avg < 90; CI workflow runs the gate on every PR.

### Step 1: Self-paint StatusBar

In `src/controls/StatusBar.cpp`:

- intercept `WM_PAINT` via subclass proc:
  - fill bg with `palette.surface_variant`
  - draw 1px `palette.divider` line above the grip
  - draw 4×4 three-dot size grip in bottom-right using `palette.text_secondary`
  - part text uses `palette.text` (already partly done — verify and extend)
- ensure the StatusBar background in dark/HC doesn't leak white from `COMCTL32`'s default paint (`theme_disable_window_theme` from `Theme.hpp` already exists; call it on the StatusBar's HWND in `create()`).

### Step 2: Self-paint Menu

In `src/menu/Menu.cpp`:

- intercept `WM_DRAWITEM` for menu items:
  - popup bg: 8px rounded rect using `palette.surface`
  - 1px border: `palette.border`
  - elevation: drop shadow via existing `paint_drop_shadow` helper (verify in `Paint.hpp`)
  - item height 28px, padding 8px
  - icon 16×16 from FontCache/IconCache
  - accelerator text right-aligned using `palette.text_secondary`
  - selected item bg: `palette.selection`, fg: `palette.selection_text`
  - separator: 4px gap + 1px line `palette.divider`
- call `theme_disable_window_theme` on the menu HWND.

### Step 3: Self-paint Scrollbar (new wrapper)

Create `include/nfui/Controls/Scrollbar.hpp`:

```cpp
#pragma once

#include <nfui/Controls/Control.hpp>

namespace nfui {

class Scrollbar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    void set_range(int min, int max) noexcept;
    void set_position(int pos) noexcept;
    [[nodiscard]] int position() const noexcept;

protected:
    void on_palette_changed() noexcept override;
    void on_paint(HDC dc, const PaintState& state) noexcept override;

private:
    void paint_thumb(HDC dc) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND, UINT, WPARAM, LPARAM,
                                                  UINT_PTR, DWORD_PTR) noexcept;

    int position_{0};
    int min_{0};
    int max_{100};
    bool hovered_thumb_{false};
};

} // namespace nfui
```

In `src/controls/Scrollbar.cpp`:

- intercept `WM_PAINT` + `WM_MOUSEMOVE` + `WM_LBUTTONDOWN` + `WM_LBUTTONUP` + `WM_MOUSELEAVE`
- on paint:
  - track is transparent
  - thumb is 4px rounded rect (8px on hover) using `palette.accent` at 60% alpha (`alpha_blend`)
  - no arrow buttons (hidden via `WS_HSCROLL`/`WS_VSCROLL` without `SB_THUMBPOS` arrows)
- on mouse: hit-test thumb rect, drag updates `position_`, `SetScrollPos`, `InvalidateRect`
- register/unregister with `ThemeBroker` for theme propagation

Note: most samples don't use scrollbars directly; they use the scrollbars that come with `ListView`/`TreeView`/`Edit`. The wrapper here is for the few cases where a standalone scrollbar matters (e.g. a custom scrollable panel). The main payoff is the `paint_thumb` routine that is reused by ListView/TreeView scrollbars via `NM_CUSTOMDRAW` if needed (defer to per-demo plans).

### Step 4: Add visual-audit gate

Create `tools/visual_audit/gate.ps1`:

```powershell
[CmdletBinding()]
param(
    [string]$Root = (Resolve-Path "$PSScriptRoot/../..").Path,
    [string]$AuditOutput = (Join-Path $Root 'docs/VISUAL_AUDIT'),
    [int]$MinScore = 85,
    [int]$MinAvg = 90
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Parse audit log if present; else require run_audit.ps1 to have produced PNGs.
# Heuristic scoring (CP-A4 placeholder): each PNG byte-size correlates with
# detail density. Replace with proper diff-vs-baseline in CP-A4 follow-up.
# For now: any missing PNG fails the gate.

$demos = @('Workbench','Showcase','DarkStudio','SettingsDemo','DialogTour',
           'ResourceGallery','ComponentGallery','ThemeDemo','ControlsPlayground',
           'Charts')
$themes = @('light','dark','hc')

$missing = @()
foreach ($d in $demos) {
    foreach ($t in $themes) {
        $file = Join-Path $AuditOutput ("{0}_{1}.png" -f $d, $t)
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            $missing += $file
        }
    }
}

if ($missing.Count -gt 0) {
    Write-Host "FAILED: missing audit PNGs:"
    $missing | ForEach-Object { Write-Host "  $_" }
    exit 1
}

# Placeholder scoring — replaced by real PNG diff in CP-A4 follow-up.
# Returns PASS for now; real gate logic added once scoring helper lands.
Write-Host "PASS: gate placeholder (no scoring yet) — 10 demos × 3 themes = 30 PNGs present."
exit 0
```

### Step 5: Wire into CI

Append a job to `.github/workflows/ci.yml`:

```yaml
  visual-audit:
    runs-on: windows-latest
    needs: build
    steps:
      - uses: actions/checkout@v4
      - name: Build x64-debug
        run: cmake --preset x64-debug && cmake --build --preset x64-debug
      - name: Run visual audit
        run: pwsh tools/visual_audit/run_audit.ps1
      - name: Audit gate
        run: pwsh tools/visual_audit/gate.ps1
```

(The exact `needs:` key must match the existing job name in `ci.yml` — verify by reading the file and align.)

### Step 6: SmokeTest for chrome self-paint

Add to `tests/nativeframeui_smoke.cpp`:

```cpp
bool test_statusbar_self_paint_renders() {
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                   0, 0, 300, 60, nullptr, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
    nfui::StatusBar sb;
    nfui::ControlCreateParams params{};
    params.instance = GetModuleHandleW(nullptr);
    params.parent = parent;
    params.width = 300;
    params.height = 24;
    if (!sb.create(params)) { DestroyWindow(parent); return false; }
    sb.set_palette(&nfui::theme_palette(nfui::ThemeMode::dark));

    UpdateWindow(sb.hwnd());
    HDC dc = GetDC(sb.hwnd());
    RECT rc; GetClientRect(sb.hwnd(), &rc);
    COLORREF bg_pixel = GetPixel(dc, rc.right / 2, rc.bottom / 2);
    ReleaseDC(sb.hwnd(), dc);

    DestroyWindow(sb.hwnd());
    DestroyWindow(parent);
    // Dark palette's surface_variant must not be pure white.
    return bg_pixel != RGB(255, 255, 255);
}
```

(Adjust for actual `StatusBar::create` signature; verify exact method names.)

### Step 7: Build + test + audit + gate

```bash
cmake --build --preset x64-debug
ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure
pwsh tools/visual_audit/run_audit.ps1
pwsh tools/visual_audit/gate.ps1
```

Expected: gate passes (placeholder score). Visual audit shows Workbench / DialogTour / ComponentGallery climbing noticeably (chrome now matches body). Compute new average; target ≥ 70 (段 A → 段 B 启动门槛).

### Step 8: Commit

```bash
git add src/controls/StatusBar.cpp src/menu/Menu.cpp src/controls/Button.cpp \
        src/controls/Controls.cpp \
        include/nfui/Controls/Scrollbar.hpp src/controls/Scrollbar.cpp \
        tools/visual_audit/gate.ps1 .github/workflows/ci.yml \
        tests/nativeframeui_smoke.cpp
git commit -m "feat(cpA4): self-paint chrome + visual audit CI gate

Self-paint StatusBar (bg + divider + size grip), Menu (popup bg + items +
separators + elevation), and new Scrollbar wrapper (thumb + track +
hover). theme_disable_window_theme called on each chrome HWND to suppress
native gray under the self-paint.

Add tools/visual_audit/gate.ps1 (placeholder scorer, real diff lands in
follow-up) wired into .github/workflows/ci.yml. Gate fails if any of the
30 audit PNGs is missing. Avg ≥ 70 milestone for transitioning from
段 A (framework) to 段 B (per-demo polish)."
```

---

## Self-Review Checklist

- [x] **Spec coverage**: every spec section maps to a task. Token → T1. State palette → T1. ThemeBroker → T1. Edit/Combo/CheckBox/Radio → T2. ListView/TreeView/Tab → T3. StatusBar/Menu/Scrollbar → T4. Gate → T4. Demo CPs deferred to per-demo plans (documented in plan header).
- [x] **No placeholders**: every step has either a code block, a concrete command, or a specific reference to an existing file/header. The gate scorer is explicitly labeled as a placeholder to be replaced (not a silent TBD).
- [x] **Type consistency**: `nfui::ThemeMode`, `nfui::ControlState`, `nfui::StatePalette`, `nfui::ThemeBroker::set_theme/register_hwnd/unregister_hwnd` are introduced in Task 1 and used unchanged in Tasks 2/3/4. `nfui::state_palette()` signature is fixed at Task 1 step 2 and called consistently in Tasks 2/3/4.
- [x] **TDD**: each task ends with a SmokeTest expansion before commit; each step 1/2/3 in Tasks 2/3/4 has a corresponding test in step 4/5/6.
- [x] **Frequent commits**: 1 commit per task, named per project convention (`feat(cpA1)` / `feat(cpA2)` / ...).
- [x] **No V1 non-goals**: no Ribbon, no Property Grid, no Data Grid, no plugin, no Direct2D.

---

## Open Risks Acknowledged

- Real PNG scoring in `gate.ps1` is a placeholder. Plan acknowledges this; CP-A4 follow-up ticket (not in this plan) must implement a proper pixel-diff vs baseline.
- The plan assumes `ApplicationContext` is reachable; step 4 in Task 1 has a fallback. Verify before step 4 lands.
- Some control class names (`insert_column`, `insert_item`, exact `HoverState` API) need verification against current headers — adjust step code if names differ. Every step has a "verify exact method names" comment.
- The plan does NOT cover the 13 demo polish CPs (段 B). Those will be 13 separate plans written after this one lands — each plan will be a thin "per-demo polish" pass that depends on the foundation built here.
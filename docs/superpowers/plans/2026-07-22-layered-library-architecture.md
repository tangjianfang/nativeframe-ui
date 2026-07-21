# Layered Library Architecture + Per-Component Split — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Rulebook:** `docs/superpowers/specs/2026-07-22-layered-library-architecture-design.md` (approved). All architectural choices in this plan derive from that spec. Any conflict between this plan and the spec is resolved in favor of the spec.

**Goal:** Migrate `NativeFrameUI` from a horizontal split (`nfui_core` + `nfui_theme` + `nfui_controls` + `nfui_charts`) to a four-layer architecture where each UI component lives in its own `STATIC` library, shareable polish patterns are extracted into `nfui_core`, and institutional knowledge accumulates under `docs/KNOWLEDGE/`.

**Architecture:** Per spec §1–4. Foundation stays as-is and gains two new helpers (`HoverState`, `OwnerDrawDC`); `nfui_controls` is dismantled into 10 per-component libs; `nfui_charts` stays as-is (already split-correct per V1.2/1.3); sample link lists change to pull only the components they need; `NativeFrameUI` umbrella becomes forwarding-only; knowledge base is created under `docs/KNOWLEDGE/{problems,polish,competitive}/`.

**Tech Stack:** C++20, Win32, CMake 3.25+ Presets, MSVC v143, `comctl32`, optional `gdiplus` for charts_aa. Boundary check via `tools/verify_boundaries.ps1`.

---

## File Structure

**Modified (existing tree stays, content reshuffled):**
- `include/nfui/Controls.hpp` → forwarding umbrella (back-compat)
- `src/controls/Controls.cpp` → deleted after migration
- `CMakeLists.txt` — adds per-component `cmake/nfui_<component>.cmake` includes; umbrella `NativeFrameUI` rebuilt from per-component libs

**Created (per-component headers + sources + cmake modules):**
- `include/nfui/Controls/Button.hpp`, `CheckBox.hpp`, `RadioButton.hpp`, `StaticText.hpp`, `Edit.hpp`, `ListBox.hpp`, `ComboBox.hpp`, `ListView.hpp`, `TreeView.hpp`, `IconView.hpp`, `StatusBar.hpp`, `TabControl.hpp`, `Tooltip.hpp`, `ProgressBar.hpp`, `Panel.hpp`, `Splitter.hpp`
- `src/controls/Button.cpp`, `CheckBox.cpp`, `RadioButton.cpp`, `StaticText.cpp`, `Edit.cpp`, `ListBox.cpp`, `ComboBox.cpp`, `ListView.cpp`, `TreeView.cpp`, `IconView.cpp`, `StatusBar.cpp`, `TabControl.cpp`, `Tooltip.cpp`, `ProgressBar.cpp`, `Panel.cpp`, `Splitter.cpp`
- `cmake/nfui_button.cmake`, `nfui_checkbox.cmake`, `nfui_radio.cmake`, `nfui_text.cmake`, `nfui_listbox.cmake`, `nfui_listview.cmake`, `nfui_treeview.cmake`, `nfui_iconview.cmake`, `nfui_frame.cmake`

**Created (foundation helpers extracted from `Controls.cpp`):**
- `include/nfui/HoverState.hpp`
- `src/core/HoverState.cpp`
- `include/nfui/Paint.hpp` — additive; adds `OwnerDrawDC` class

**Created (knowledge base):**
- `docs/KNOWLEDGE/INDEX.md`
- `docs/KNOWLEDGE/problems/INDEX.md` + migrated entries
- `docs/KNOWLEDGE/polish/INDEX.md` + ≥1 entry
- `docs/KNOWLEDGE/competitive/INDEX.md` + ≥1 entry

**Dependency direction after the split (single-direction, verified mechanically):**
```
nfui_core    → (Win32)
nfui_theme   → nfui_core
nfui_button  → nfui_core, nfui_theme
nfui_checkbox → nfui_core, nfui_theme
nfui_radio   → nfui_core, nfui_theme
nfui_text    → nfui_core, nfui_theme
nfui_listbox → nfui_core, nfui_theme           (no nfui_text dep per decision §6.1)
nfui_listview → nfui_core, nfui_theme
nfui_treeview → nfui_core, nfui_theme
nfui_iconview → nfui_core, nfui_theme
nfui_frame   → nfui_core, nfui_theme
nfui_charts  → nfui_core, nfui_theme           (unchanged)
nfui_charts_aa → nfui_charts, gdiplus          (unchanged)
```

No component library PUBLIC-links another component library. No circular edges.

---

## Task 1: Extract `HoverState` to `nfui_core`

**Files:**
- Create: `include/nfui/HoverState.hpp`
- Create: `src/core/HoverState.cpp`
- Modify: `cmake/nfui_core.cmake` (add `src/core/HoverState.cpp`)
- Test: `tests/nativeframeui_smoke.cpp` (append)

- [ ] **Step 1: Write the failing test.** Append to `tests/nativeframeui_smoke.cpp` inside `main()` before the final success return:

```cpp
{
    // Pure-logic: state transitions without an HWND.
    nfui::HoverState h;
    if (h.hover() || h.pressed()) return 101;
    h.on_lbutton_down();
    if (!h.pressed()) return 102;
    h.on_lbutton_up();
    if (h.pressed()) return 103;
    h.on_mouse_move(nullptr); // null hwnd tolerated
    h.on_mouse_leave();
    if (h.hover()) return 104;
}
```

- [ ] **Step 2: Run test to verify it fails.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest --output-on-failure` → exit 101 (build succeeds, test fails).

- [ ] **Step 3: Write minimal implementation.** Create `include/nfui/HoverState.hpp`:

```cpp
#pragma once

#include <windows.h>

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
    bool tracking_{false};
};

} // namespace nfui
```

Create `src/core/HoverState.cpp`:

```cpp
#include <nfui/HoverState.hpp>

namespace nfui {

void HoverState::attach(HWND) noexcept {
    tracking_ = false;
    hover_ = false;
    pressed_ = false;
}

void HoverState::detach() noexcept {
    tracking_ = false;
    hover_ = false;
    pressed_ = false;
}

void HoverState::on_mouse_move(HWND hwnd) noexcept {
    if (!hover_) {
        hover_ = true;
        if (hwnd != nullptr && !tracking_) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = static_cast<DWORD>(sizeof(tme));
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            tracking_ = true;
        }
        if (hwnd != nullptr) InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void HoverState::on_mouse_leave() noexcept {
    if (hover_) {
        hover_ = false;
        tracking_ = false;
    }
}

void HoverState::on_lbutton_down() noexcept { pressed_ = true; }
void HoverState::on_lbutton_up() noexcept   { pressed_ = false; }

} // namespace nfui
```

Add `src/core/HoverState.cpp` to `cmake/nfui_core.cmake` `add_library(nfui_core STATIC ...)` list (alphabetic position).

- [ ] **Step 4: Run test to verify it passes.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (exit 0).

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/HoverState.hpp src/core/HoverState.cpp cmake/nfui_core.cmake tests/nativeframeui_smoke.cpp
git commit -m "feat(core): extract HoverState helper for owner-draw controls"
```

---

## Task 2: Add `OwnerDrawDC` to `nfui::Paint`

**Files:**
- Modify: `include/nfui/Paint.hpp`
- Modify: `src/paint/Paint.cpp`
- Test: `tests/nativeframeui_smoke.cpp` (append)

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    // OwnerDrawDC requires an HWND; verify it tolerates the hidden smoke window.
    // Reuse the harness's `hidden` HWND variable (see existing smoke test).
    RECT rc{0, 0, 100, 20};
    nfui::OwnerDrawDC odc(hidden, rc);
    if (!odc.valid()) return 111;
    if (odc.dc() == nullptr) return 112;
    RECT got = odc.bounds();
    if (got.right - got.left != 100 || got.bottom - got.top != 20) return 113;
}
```

> If the harness variable is named differently (e.g. `hwndHidden`, `g_hidden`), use that name.

- [ ] **Step 2: Run test to verify it fails.** Build → undeclared identifier `OwnerDrawDC`.

- [ ] **Step 3: Write minimal implementation.** Add to `include/nfui/Paint.hpp` (append after `MemoryDC`):

```cpp
class OwnerDrawDC {
public:
    OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept;
    ~OwnerDrawDC() noexcept;
    OwnerDrawDC(const OwnerDrawDC&) = delete;
    OwnerDrawDC& operator=(const OwnerDrawDC&) = delete;
    [[nodiscard]] HDC dc() const noexcept { return target_; }
    [[nodiscard]] RECT bounds() const noexcept { return bounds_; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
private:
    HWND hwnd_{};
    RECT bounds_{};
    MemoryDC mem_;
    HDC target_{};
    bool valid_{false};
};
```

Append to `src/paint/Paint.cpp`:

```cpp
OwnerDrawDC::OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept
    : hwnd_(hwnd), bounds_(bounds), mem_(nullptr, RECT{0,0,0,0}) {
    if (hwnd == nullptr) return;
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    if (dc == nullptr) return;
    mem_ = MemoryDC(dc, bounds);
    target_ = mem_.valid() ? mem_.dc() : dc;
    valid_ = true;
}

OwnerDrawDC::~OwnerDrawDC() noexcept {
    if (valid_ && hwnd_ != nullptr) {
        EndPaint(hwnd_, nullptr); // ps reused via destructor ordering; see note
    }
}
```

> **Implementation note:** `OwnerDrawDC` MUST own the `BeginPaint`/`EndPaint` pair. Replace the test body's `BeginPaint` order: the constructor calls `BeginPaint` and stores `hwnd_`; the destructor calls `EndPaint(hwnd_, &ps)` — to do this correctly, store a `PAINTSTRUCT` member:

```cpp
class OwnerDrawDC {
public:
    OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept;
    ~OwnerDrawDC() noexcept;
    OwnerDrawDC(const OwnerDrawDC&) = delete;
    OwnerDrawDC& operator=(const OwnerDrawDC&) = delete;
    [[nodiscard]] HDC dc() const noexcept { return target_; }
    [[nodiscard]] RECT bounds() const noexcept { return bounds_; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
private:
    HWND hwnd_{};
    RECT bounds_{};
    PAINTSTRUCT ps_{};
    MemoryDC mem_;
    HDC target_{};
    bool valid_{false};
};

OwnerDrawDC::OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept
    : hwnd_(hwnd), bounds_(bounds) {
    if (hwnd == nullptr) return;
    HDC dc = BeginPaint(hwnd, &ps_);
    if (dc == nullptr) return;
    mem_ = MemoryDC(dc, bounds);
    target_ = mem_.valid() ? mem_.dc() : dc;
    valid_ = true;
}

OwnerDrawDC::~OwnerDrawDC() noexcept {
    if (hwnd_ != nullptr && valid_) {
        EndPaint(hwnd_, &ps_);
    }
}
```

- [ ] **Step 4: Run test to verify it passes.** Build + smoke → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Paint.hpp src/paint/Paint.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(paint): add OwnerDrawDC RAII wrapper for WM_PAINT lifecycle"
```

---

## Task 3: Migrate `Controls.cpp` to use `HoverState` + `OwnerDrawDC`

**Files:**
- Modify: `src/controls/Controls.cpp`
- Modify: `include/nfui/Controls.hpp`
- Test: `tests/nativeframeui_smoke.cpp` (no new assertions — pure refactor)

- [ ] **Step 1: Run baseline test.** `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (regression gate).

- [ ] **Step 2: Refactor `Control` to host a `HoverState`.** Add `#include <nfui/HoverState.hpp>` to `Controls.hpp` and add a `protected: HoverState hover_state_;` member on `Control`. Replace the `bool hover_{false};` member — all reads of `control->hover_` inside `subclass_proc` become `control->hover_state_.hover()`, and all `WM_MOUSEMOVE` / `WM_MOUSELEAVE` handling becomes `control->hover_state_.on_mouse_move(hwnd)` / `on_mouse_leave()`. Pressed-state via `WM_LBUTTONDOWN` / `WM_LBUTTONUP` becomes `on_lbutton_down()` / `on_lbutton_up()`. The `control_is_owner_draw(hwnd)` gate is dropped — hover tracking now applies to **all** subclassed controls (cheaper than the gate; visible change is that ListView / TreeView rows now refresh on hover, which is intentional V1.5 polish).

- [ ] **Step 3: Refactor `Button::on_paint` + `StaticText::on_paint` + `IconView::on_paint`** to use `nfui::OwnerDrawDC` instead of the manual `BeginPaint` / `MemoryDC` / `EndPaint` block. The `subclass_proc` `case WM_PAINT` no longer needs the explicit handler — `wants_self_paint()` controls already self-paint; **keep** the existing `case WM_PAINT` branch but route through `OwnerDrawDC` so the destructor ordering invariant is enforced by the type, not by reviewer discipline.

- [ ] **Step 4: Run smoke + visual regression.** Build, smoke → PASS. Launch `NativeFrameUIControls.exe` and `NativeFrameUIWorkbench.exe` — buttons render identically, hover/press feedback intact. (If visuals differ, stop and fix before committing.)

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp
git commit -m "refactor(controls): use HoverState + OwnerDrawDC helpers in Controls.cpp"
```

---

## Task 4: Per-component header directory + split `Button` → `nfui_button`

**Files:**
- Create: `include/nfui/Controls/Button.hpp`
- Create: `src/controls/Button.cpp`
- Create: `cmake/nfui_button.cmake`
- Modify: `CMakeLists.txt` (add `include(cmake/nfui_button.cmake)`; remove `src/controls/Controls.cpp` Button code)
- Modify: `include/nfui/Controls.hpp` (remove `class Button` forward; the new `Controls/Button.hpp` includes it)
- Test: `tests/nativeframeui_smoke.cpp` (append)

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    nfui::Button b;
    nfui::ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr);
    p.parent = hidden;
    p.control_id = 9001;
    p.text = L"OK";
    p.width = 80; p.height = 28;
    if (!b.create(p)) return 121;
    if (!b.valid() || b.hwnd() == nullptr) return 122;
    if ((GetWindowLongPtrW(b.hwnd(), GWL_STYLE) & BS_OWNERDRAW) == 0) return 123;
}
```

- [ ] **Step 2: Run test to verify it fails.** Build fails: `nfui::Button` re-declared (current `Controls.hpp` has it; after deletion it goes away). Or the test passes with current code but the `nfui_button` lib doesn't exist yet — confirm.

- [ ] **Step 3: Create the per-component directory + Button header.** Create `include/nfui/Controls/Button.hpp`:

```cpp
#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

struct ButtonStyle {
    std::optional<int>   corner_radius;
    std::optional<Color> border_color;
    std::optional<int>   horizontal_padding;
    std::optional<int>   vertical_padding;
    std::optional<bool>  use_semibold;
};

class Button : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(ButtonStyle style) noexcept { style_ = style; }
    [[nodiscard]] const ButtonStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    ButtonStyle style_{};
};

} // namespace nfui
```

`std::optional` requires `<optional>` include at the top of the header.

- [ ] **Step 4: Create `src/controls/Button.cpp`** by moving the `Button::create` and `Button::on_paint` bodies out of `Controls.cpp`. The `on_paint` body must also incorporate the style slot:

```cpp
void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* palette_ptr = palette();
    const ThemePalette& p = palette_ptr ? *palette_ptr : theme_palette(ThemeMode::light);
    const int theme_radius = theme_metrics().corner_radius_control;
    const int radius = style_.corner_radius.value_or(theme_radius);
    const Color border = style_.border_color.value_or(p.border);
    Color face = p.accent;
    Color text_color = p.accent_text;
    if (!state.enabled) {
        face = p.border;
        text_color = p.text_secondary;
    } else if (state.pressed || state.hover) {
        face = p.accent_hover;
    }
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rounded_rect(target, paint_bounds, radius, face, border);
    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        const int dpi = dpi_of(hwnd());
        font = style_.use_semibold.value_or(false)
            ? cache->semibold(dpi, 9)
            : cache->regular(dpi, 9);
    }
    draw_text(target, paint_bounds, caption(), font, text_color,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
```

Delete the `Button::create` and `Button::on_paint` definitions from `Controls.cpp`. Leave `class Button` declaration in `Controls.hpp` (still the umbrella) — the per-component header is the **authoritative** declaration and `Controls.hpp` will be replaced by the forwarding umbrella in Task 13.

- [ ] **Step 5: Create `cmake/nfui_button.cmake`:**

```cmake
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

- [ ] **Step 6: Wire up the new lib in `CMakeLists.txt`.** Add `include(cmake/nfui_button.cmake)` after `include(cmake/nfui_controls.cmake)`. Modify `target_link_libraries(NativeFrameUI INTERFACE ...)` to **also** include `NativeFrameUI::nfui_button` so the umbrella still exposes Button. Do NOT remove `nfui_controls` yet — that happens in Task 13.

- [ ] **Step 7: Build + smoke.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 8: Commit.**
```bash
git add include/nfui/Controls/Button.hpp src/controls/Button.cpp cmake/nfui_button.cmake CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "refactor(controls): split Button into nfui_button library"
```

---

## Task 5: Split `CheckBox` → `nfui_checkbox`

**Files:**
- Create: `include/nfui/Controls/CheckBox.hpp`
- Create: `src/controls/CheckBox.cpp`
- Create: `cmake/nfui_checkbox.cmake`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    nfui::CheckBox c;
    nfui::ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr);
    p.parent = hidden;
    p.control_id = 9010;
    p.text = L"Enable";
    p.width = 120; p.height = 24;
    if (!c.create(p) || !c.valid()) return 131;
    if ((GetWindowLongPtrW(c.hwnd(), GWL_STYLE) & BS_AUTOCHECKBOX) == 0) return 132;
}
```

- [ ] **Step 2-7: same pattern as Task 4.** Move `CheckBox::create` body to `src/controls/CheckBox.cpp`. `CheckBox` has no `on_paint` override (native chrome retained for V1) and no `*Style` struct yet — leave header minimal.

- [ ] **Step 8: Commit.**
```bash
git add include/nfui/Controls/CheckBox.hpp src/controls/CheckBox.cpp cmake/nfui_checkbox.cmake CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "refactor(controls): split CheckBox into nfui_checkbox library"
```

---

## Task 6: Split `RadioButton` → `nfui_radio`

Same pattern as Task 5. `control_id` in test: `9020`. Style `BS_AUTORADIOBUTTON`. Return code: `141`, `142`.

Commit: `"refactor(controls): split RadioButton into nfui_radio library"`.

---

## Task 7: Split `StaticText` + `Edit` → `nfui_text`

**Files:**
- Create: `include/nfui/Controls/StaticText.hpp`, `include/nfui/Controls/Edit.hpp`
- Create: `src/controls/StaticText.cpp`, `src/controls/Edit.cpp`
- Create: `cmake/nfui_text.cmake`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    nfui::StaticText t;
    nfui::ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr);
    p.parent = hidden;
    p.control_id = 9030;
    p.text = L"Hello";
    p.width = 120; p.height = 20;
    if (!t.create(p) || !t.valid()) return 151;
    if ((GetWindowLongPtrW(t.hwnd(), GWL_STYLE) & SS_TYPEMASK) != SS_LEFT) return 152;
}
{
    nfui::Edit e;
    nfui::ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr);
    p.parent = hidden;
    p.control_id = 9031;
    p.text = L"editable";
    p.width = 120; p.height = 24;
    if (!e.create(p) || !e.valid()) return 153;
    if ((GetWindowLongPtrW(e.hwnd(), GWL_STYLE) & ES_LEFT) == 0) return 154;
}
```

- [ ] **Step 2-7: move `StaticText::create` + `StaticText::on_paint` + `Edit::create` bodies to per-component sources.** Shared `TextStyle` lives in `StaticText.hpp` (the primary renderer):

```cpp
// include/nfui/Controls/StaticText.hpp (excerpt)
struct TextStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<int>   horizontal_padding;
    std::optional<int>   vertical_padding;
    std::optional<bool>  use_semibold;
};
```

`StaticText::on_paint` resolves `TextStyle` over the theme palette the same way `Button::on_paint` resolves `ButtonStyle`. `Edit::on_paint` is left as native (V1 retains native chrome on Edit) — but if `Edit` ever self-paints in V1.4+, it uses the same `TextStyle`.

- [ ] **Step 8: Commit.**
```bash
git add include/nfui/Controls/StaticText.hpp include/nfui/Controls/Edit.hpp src/controls/StaticText.cpp src/controls/Edit.cpp cmake/nfui_text.cmake CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "refactor(controls): split StaticText + Edit into nfui_text library"
```

---

## Task 8: Split `ListBox` + `ComboBox` → `nfui_listbox`

Same pattern. The `draw_list_item` helper currently lives in `Controls.cpp`'s anonymous namespace — move it into `src/controls/ListBox.cpp` (or extract to `src/controls/internal/ListItemDraw.cpp` if both `ListBox` and `ComboBox` need it; they do, so do the extraction).

**Shared `ListStyle`** in `ListBox.hpp`:

```cpp
struct ListStyle {
    std::optional<int>   row_height_extra;  // padding added to font-pixel row
    std::optional<int>   horizontal_padding;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
    std::optional<bool>  rounded_selection; // V1.5: apply fill_rounded_rect to selected row
};
```

`draw_list_item(di, pal, fonts, style)` accepts the optional style; if not set, falls back to current defaults. `ComboBox::on_paint` defers its drop-down rendering to `ListBox`'s path internally (or stays native for V1.5 — decide at implementation time: **decision**: `ComboBox` stays native chrome in V1.5; `ListStyle` is consumed only by `ListBox`. If `ComboBox` later self-paints, it adopts the same helper.)

Test return codes: `161`, `162` (ListBox create + LBS_OWNERDRAWFIXED style), `163`, `164` (ComboBox create + CBS_DROPDOWNLIST style).

Commit: `"refactor(controls): split ListBox + ComboBox into nfui_listbox library"`.

---

## Task 9: Split `ListView` → `nfui_listview`

The `WM_NOTIFY` + `NM_CUSTOMDRAW` handling currently lives in `subclass_proc`. Move it to a private member `ListView::on_custom_draw(NMLVCUSTOMDRAW*)` in `src/controls/ListView.cpp`, and route `OCM_NOTIFY` calls in the **future** `subclass_proc` (kept shared in `nfui_core` per Task 14) by dispatching on a virtual `on_notify_reflected(OCM_NOTIFY, lparam)` hook.

For now (Task 9 only), keep the existing `subclass_proc` routing for ListView's `OCM_NOTIFY` (don't move the switch yet — that lands in Task 14 when `subclass_proc` itself moves). Add `ListViewStyle`:

```cpp
struct ListViewStyle {
    std::optional<Color> row_background;
    std::optional<Color> row_foreground;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
    std::optional<bool>  full_row_select;  // LVS_EX_FULLROWSELECT toggle
    std::optional<bool>  show_gridlines;
};
```

Test return codes: `171`, `172`.

Commit: `"refactor(controls): split ListView into nfui_listview library"`.

---

## Task 10: Split `TreeView` → `nfui_treeview`

Native chrome in V1.5. No `*Style` yet (placeholder for V1.4 polish). Test return codes: `181`, `182`.

Commit: `"refactor(controls): split TreeView into nfui_treeview library"`.

---

## Task 11: Split `IconView` → `nfui_iconview`

Move `IconView::create`, `IconView::set_icon`, `IconView::on_paint` bodies. Add `IconViewStyle`:

```cpp
struct IconViewStyle {
    std::optional<int> pixel_size;  // overrides bounds-derived size
    std::optional<bool> preserve_aspect;
};
```

Test return codes: `191`, `192` (create + `SS_OWNERDRAW` style).

Commit: `"refactor(controls): split IconView into nfui_iconview library"`.

---

## Task 12: Split frame controls → `nfui_frame`

`StatusBar`, `TabControl`, `Tooltip`, `ProgressBar`, `Panel`, `Splitter` all stay native chrome in V1.5 (per V1.1 KNOWN_LIMITATIONS — `Workbench` chrome). One source file per control, one header per control, all in `nfui_frame`. Add a single shared `FrameStyle`:

```cpp
struct FrameStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> accent;
};
```

That gets used by `TabControl::on_paint` once it self-paints (future). For now, the style is set / get but ignored.

Test append:

```cpp
{
    nfui::StatusBar sb; nfui::ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr); p.parent = hidden;
    p.control_id = 9040; p.width = 200; p.height = 20;
    if (!sb.create(p) || !sb.valid()) return 201;
}
{
    nfui::Splitter sp; p.control_id = 9041; p.width = 4; p.height = 100;
    if (!sp.create(p) || !sp.valid()) return 202;
    sp.set_ratio(0.3); if (sp.ratio() != 0.3) return 203;
}
```

Commit: `"refactor(controls): split frame controls into nfui_frame library"`.

---

## Task 13: Replace `Controls.hpp` with forwarding umbrella + delete monolithic `Controls.cpp`

**Files:**
- Modify: `include/nfui/Controls.hpp` → forwarding umbrella only
- Delete: `src/controls/Controls.cpp`
- Modify: `cmake/nfui_controls.cmake` → empty sources list, become a meta-package
- Modify: `CMakeLists.txt` → drop the `nfui_controls` source entry

- [ ] **Step 1: Run baseline.** Smoke + visual: every existing demo still compiles + renders.

- [ ] **Step 2: Rewrite `include/nfui/Controls.hpp`:**

```cpp
#pragma once

// Forwarding umbrella for back-compat. Prefer including the specific per-component header.
#include <nfui/Controls/Button.hpp>
#include <nfui/Controls/CheckBox.hpp>
#include <nfui/Controls/RadioButton.hpp>
#include <nfui/Controls/StaticText.hpp>
#include <nfui/Controls/Edit.hpp>
#include <nfui/Controls/ListBox.hpp>
#include <nfui/Controls/ComboBox.hpp>
#include <nfui/Controls/ListView.hpp>
#include <nfui/Controls/TreeView.hpp>
#include <nfui/Controls/IconView.hpp>
#include <nfui/Controls/StatusBar.hpp>
#include <nfui/Controls/TabControl.hpp>
#include <nfui/Controls/Tooltip.hpp>
#include <nfui/Controls/ProgressBar.hpp>
#include <nfui/Controls/Panel.hpp>
#include <nfui/Controls/Splitter.hpp>
```

The shared `Control` base + `ControlCreateParams` lives in **`nfui_core`** — move `include/nfui/Controls.hpp`'s `Control` base declarations into a new `include/nfui/ControlBase.hpp` and `#include` it from each per-component header.

- [ ] **Step 3: Move `Control` base + `ControlCreateParams` to `nfui_core`.**

```cpp
// include/nfui/ControlBase.hpp
#pragma once
#include <nfui/Handle.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Font.hpp>
#include <nfui/HoverState.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <windows.h>

namespace nfui {
struct ControlCreateParams { /* unchanged */ };
class Control { /* unchanged, with HoverState member per Task 3 */ };
}
```

- [ ] **Step 4: Extract `subclass_proc` to a shared helper in `nfui_core`.** Move `Control::subclass_proc` to a free function in `src/core/ControlSubclass.cpp` that takes the `Control*` and a virtual hook table (`on_paint`, `on_notify_reflected`, `wants_self_paint`). Each component's `subclass_id` becomes unique per-component. This is the trickiest step — keep behavior identical to current `subclass_proc`; the refactor is mechanical.

- [ ] **Step 5: Delete `src/controls/Controls.cpp`** and the `nfui_controls` source line from `cmake/nfui_controls.cmake`. Make `nfui_controls` an INTERFACE target that PUBLIC-links all 10 per-component libs (for back-compat consumers that want the umbrella).

- [ ] **Step 6: Update `CMakeLists.txt`.** Drop `include(cmake/nfui_controls.cmake)`; the `NativeFrameUI` umbrella now PUBLIC-links the 10 per-component libs directly:

```cmake
target_link_libraries(NativeFrameUI
    INTERFACE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_button
        NativeFrameUI::nfui_checkbox
        NativeFrameUI::nfui_radio
        NativeFrameUI::nfui_text
        NativeFrameUI::nfui_listbox
        NativeFrameUI::nfui_listview
        NativeFrameUI::nfui_treeview
        NativeFrameUI::nfui_iconview
        NativeFrameUI::nfui_frame
        NativeFrameUI::nfui_charts
)
```

- [ ] **Step 7: Build + smoke + boundary check + visual regression.** All green. Boundary check stays green (no forbidden markers).

- [ ] **Step 8: Commit.**
```bash
git add include/nfui/Controls.hpp include/nfui/ControlBase.hpp src/controls/Controls.cpp cmake/nfui_controls.cmake CMakeLists.txt
git commit -m "refactor(controls): forwarding umbrella + delete monolithic Controls.cpp"
```

---

## Task 14: Switch each sample to per-component link list

**Files:**
- Modify: each sample's `CMakeLists.txt` link list (or per-target override)

- [ ] **Step 1: Inventory each sample's component usage.** Grep each sample for `nfui::Button`, `nfui::ListBox`, etc. to determine the minimum set. For each sample, derive the minimum link list.

- [ ] **Step 2: Update `CMakeLists.txt` per sample.**

```cmake
# NativeFrameUICharts — only nfui_charts
target_link_libraries(NativeFrameUICharts
    PRIVATE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_charts
        NativeFrameUI::nfui_charts_aa
)

# NativeFrameUISettingsDemo — text + button + checkbox + listbox + radio + frame
target_link_libraries(NativeFrameUISettingsDemo
    PRIVATE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_text
        NativeFrameUI::nfui_button
        NativeFrameUI::nfui_checkbox
        NativeFrameUI::nfui_radio
        NativeFrameUI::nfui_listbox
        NativeFrameUI::nfui_frame
)

# NativeFrameUIWorkbench — most components; keep umbrella
target_link_libraries(NativeFrameUIWorkbench
    PRIVATE
        NativeFrameUI::NativeFrameUI
)
```

Use the umbrella (`NativeFrameUI::NativeFrameUI`) only for samples that genuinely use most components. Samples that use a small subset (Charts, SettingsDemo) link per-component to prove the value.

- [ ] **Step 3: Build all + smoke.** All green.

- [ ] **Step 4: Commit.**
```bash
git add CMakeLists.txt
git commit -m "refactor(samples): link only the components each sample uses"
```

---

## Task 15: Rename `docs/LEARNINGS/` → `docs/KNOWLEDGE/problems/`

**Files:**
- Move: `docs/LEARNINGS/2026-07-paint-cycle-audit.md` → `docs/KNOWLEDGE/problems/2026-07-paint-cycle.md`
- Move: `docs/LEARNINGS/2026-07-fontcache-dpi-audit.md` → `docs/KNOWLEDGE/problems/2026-07-fontcache-dpi.md`
- Create: `docs/KNOWLEDGE/INDEX.md`
- Create: `docs/KNOWLEDGE/problems/INDEX.md`
- Delete: `docs/LEARNINGS/` directory
- Modify: any cross-doc link references (search via grep)

- [ ] **Step 1: Migrate the two entries to the new schema.** Update each entry to add frontmatter:

```markdown
---
title: MemoryDC Scope-Before-EndPaint Audit
date: 2026-07-21
trigger: commit dd4768f + R6 spec gap
status: resolved
tags: [paint, lifecycle]
related: [[2026-07-fontcache-dpi]]
---
```

Keep the body content; rename file slugs from `-audit` suffix to bare slug. Update any internal cross-references.

- [ ] **Step 2: Create `docs/KNOWLEDGE/INDEX.md`** with one-line-per-entry format, links to each entry:

```markdown
# Knowledge Base

## problems/ — Issues encountered + how they were fixed
- [MemoryDC Scope-Before-EndPaint](problems/2026-07-paint-cycle.md) — 2026-07-21
- [FontCache Consumer DPI Audit](problems/2026-07-fontcache-dpi.md) — 2026-07-21

## polish/ — Product detail polish requirements
- (none yet — see [INDEX](polish/INDEX.md))

## competitive/ — How other products handle the same interactions
- (none yet — see [INDEX](competitive/INDEX.md))
```

- [ ] **Step 3: Create `docs/KNOWLEDGE/problems/INDEX.md`** — just a status board with the two entries listed.

- [ ] **Step 4: Delete `docs/LEARNINGS/`.** Verify nothing in the repo links to it (`grep -r "docs/LEARNINGS"`). Add a forwarding note in `docs/KNOWLEDGE/INDEX.md` if any link breaks.

- [ ] **Step 5: Commit.**
```bash
git add docs/KNOWLEDGE/ docs/LEARNINGS/
git commit -m "docs(knowledge): migrate LEARNINGS to KNOWLEDGE/problems with new schema"
```

---

## Task 16: Create `docs/KNOWLEDGE/polish/` board + first entry

**Files:**
- Create: `docs/KNOWLEDGE/polish/INDEX.md`
- Create: `docs/KNOWLEDGE/polish/2026-07-button-press-feedback.md`
- Modify: `docs/KNOWLEDGE/INDEX.md` (link to the new entry)

- [ ] **Step 1: Capture a polish requirement from V1.4 visual-controls backlog.** Example: `Button` press feedback could animate `accent → accent_hover` over ~80ms instead of instant. Write the entry per spec §5.2 schema:

```markdown
---
title: Button press feedback — animate face color transition
date: 2026-07-22
status: idea
priority: p1
component: nfui_button
related-products: [NativeFrameUIControls, NativeFrameUIShowcase]
---

## Observation
Press feedback on `Button` snaps instantly from `accent` to `accent_hover`. Feels abrupt at 60Hz+ on modern panels.

## Target behavior
80ms ease-out transition from `accent` to `accent_hover` on press, mirrored on release. Total visible feel: "soft settle" instead of "click".

## Acceptance criteria
- [ ] Press-down transitions face color over ~80ms
- [ ] Release transitions back over ~80ms
- [ ] No effect when `ButtonStyle::press_animation_ms = 0`
- [ ] Disabled state skips animation

## Reference
- See [[2026-07-button-press-feedback-competitive]] for cross-product comparison

## Status log
- 2026-07-22: idea captured during V1.5 layered-architecture work
```

- [ ] **Step 2: Create `docs/KNOWLEDGE/polish/INDEX.md`** as a status board (table: title | status | priority | component | date).

- [ ] **Step 3: Update `docs/KNOWLEDGE/INDEX.md`** to reference the new section.

- [ ] **Step 4: Commit.**
```bash
git add docs/KNOWLEDGE/polish/ docs/KNOWLEDGE/INDEX.md
git commit -m "docs(knowledge): create polish/ board + Button press feedback entry"
```

---

## Task 17: Create `docs/KNOWLEDGE/competitive/` board + first entry

**Files:**
- Create: `docs/KNOWLEDGE/competitive/INDEX.md`
- Create: `docs/KNOWLEDGE/competitive/2026-07-button-press-feedback.md`
- Modify: `docs/KNOWLEDGE/INDEX.md`

- [ ] **Step 1: Capture a competitive comparison.** Example: how Chrome's URL bar button, Win11 Settings' button, and VSCode's toolbar button each handle press feedback. Write per spec §5.3 schema:

```markdown
---
title: Button press feedback across desktop products
date: 2026-07-22
products: [Chrome, Win11-Settings, VSCode, Firefox]
tags: [press-feedback, button, animation]
---

## What they do
- **Chrome** (URL bar buttons): instant snap on press, instant snap on release. No animation. Reads as "snappy / mechanical".
- **Win11 Settings**: ~120ms fade on press (subtle), instant on release. Reads as "soft / composed".
- **VSCode** (toolbar): ~50ms quick fade on press, instant on release. Reads as "responsive".
- **Firefox** (toolbar buttons): no animation; relies on background color change only.

## What works well
- Win11's asymmetric timing (slow-in, instant-out) communicates "commit happened" without lag perception.
- VSCode's symmetric ~50ms is the shortest timing that still reads as a transition.

## What doesn't work
- Chrome's instant snap on press feels mechanical / dated.
- Firefox's lack of any visual change (just background) misses the moment entirely.

## Applicable to nfui
- [[2026-07-button-press-feedback]] should target ~80ms symmetric with subtle ease-out. Faster than Win11, slower than VSCode, and applies to both press and release.
```

- [ ] **Step 2: Create `docs/KNOWLEDGE/competitive/INDEX.md`** as a matrix.

- [ ] **Step 3: Update `docs/KNOWLEDGE/INDEX.md`**.

- [ ] **Step 4: Commit.**
```bash
git add docs/KNOWLEDGE/competitive/ docs/KNOWLEDGE/INDEX.md
git commit -m "docs(knowledge): create competitive/ board + Button press feedback entry"
```

---

## Task 18: Final validation pass + push

- [ ] **Step 1:** Run full matrix:
```bash
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug --output-on-failure
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release --output-on-failure
```
Expected: all CTest tests PASS, boundary check PASS, every sample builds.

- [ ] **Step 2:** Manual visual: launch each sample (Controls, Workbench, SettingsDemo, DarkStudio, ResourceGallery, Showcase, Charts). Verify no visual regression.

- [ ] **Step 3:** Inspect `target_link_libraries` of each sample to confirm Task 14 link-list changes.

- [ ] **Step 4:** Update `docs/KNOWN_LIMITATIONS.md`:
- Note that `nfui_controls` has been dismantled into per-component libs (mention the new `nfui_button` / `nfui_text` etc.).
- Note the new `docs/KNOWLEDGE/` tree.

- [ ] **Step 5:** Update `README.md` demo matrix to reflect the per-component split if the matrix mentions `nfui_controls`.

- [ ] **Step 6:** Commit + push:
```bash
git add docs/KNOWN_LIMITATIONS.md README.md
git commit -m "docs: V1.5 layered architecture — limitations + README matrix update"
git push origin main
```

---

## Loop Verification (run after every task)

```bash
cmake --build --preset x64-debug
ctest --preset x64-debug --output-on-failure
```
- If smoke test fails: stop, fix, re-run. Do not advance.
- If boundary check fails: a forbidden marker slipped in (MFC/ATL/WTL/BCG). Remove before continuing.
- If any sample fails to build: the link-list change in that task is wrong. Fix before advancing.

## V1.5 Exit Criteria

- [ ] All 17 numbered tasks above complete and committed.
- [ ] Each per-component lib builds and tests independently.
- [ ] `NativeFrameUI` umbrella still works for legacy consumers.
- [ ] `docs/KNOWLEDGE/` exists with all three subtrees + INDEX per section.
- [ ] Both `docs/KNOWLEDGE/polish/` and `docs/KNOWLEDGE/competitive/` have at least one entry.
- [ ] Boundary check, both presets, full smoke test, all green.
- [ ] Pushed to `origin/main`.
- [ ] No demo visually regresses.

## Self-Review

**1. Spec coverage.** All four layers (foundation / components / templates / products) addressed: foundation in Tasks 1–3, components in Tasks 4–12, products in Task 14, templates explicitly deferred to V1.6 per spec §4 "designed, not built this cycle". Knowledge base in Tasks 15–17. Style slots in Tasks 4–12 (`ButtonStyle`, `TextStyle`, `ListStyle`, `ListViewStyle`, `IconViewStyle`, `FrameStyle`) with the locked cascade order.

**2. Placeholder scan.** No "TBD". The one ambiguous step — `Tooltip::create` style in Task 12 — is specified by reference to the V1.1 KNOWN_LIMITATIONS line that says tooltip stays native. The `ComboBox::on_paint` decision is called out explicitly in Task 8 ("stays native chrome in V1.5").

**3. Type consistency.** `ButtonStyle`, `TextStyle`, `ListStyle`, `ListViewStyle`, `IconViewStyle`, `FrameStyle` all use `std::optional<T>` for per-field overrides; resolution rule (theme → baked default → consumer override) stated in Tasks 3–4 and respected throughout. `HoverState` API (5 methods + 2 getters) used uniformly in Tasks 3 onward. `OwnerDrawDC` (3 methods + 2 getters) used in Tasks 2–3.
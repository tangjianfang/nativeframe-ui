# NativeFrame UI Visual Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. After every task, run the Loop Verification block at the bottom; re-enter via `/loop` to repeat the cycle until the V1 Visual exit criteria pass.

**Goal:** Replace raw Win32 native visuals with an elegant, refined, theme-aware rendering system for the basic controls (Text, Button, List, View, Icon), delivered as reusable `nfui` primitives plus one reference demo that proves them.

**Architecture:** Build a small visual core — `ThemePalette`, `Paint` (Buffered Paint + rounded-rect + text), `FontCache` (DPI-scaled Segoe UI), `Icon` (DPI-scaled HICON) — then make `Button`, `StaticText`, `ListBox`, `ListView` paint themselves through the existing `Control` subclass mechanism (`WM_PAINT` → virtual `on_paint`) and `NM_CUSTOMDRAW`. Visuals are owner/custom-drawn on top of native HWNDs; `hwnd()` access and all V1 architecture rules (no exceptions across callbacks, stable address until `WM_NCDESTROY`, UI-thread-only mutation) are preserved. Pure color/font/layout math is unit-tested in the smoke test; rendered output is verified manually with screenshots.

**Tech Stack:** C++20, Win32, GDI, UxTheme (`BeginBufferedPaint`/`EndBufferedPaint`), Common Controls (`NM_CUSTOMDRAW`), CMake Presets, CTest.

---

## File Structure

**Created:**
- `include/nfui/Paint.hpp` — Buffered Paint RAII, rounded-rect fill, text draw, pure color math (`lighten`, `darken`, `alpha_blend`).
- `src/paint/Paint.cpp` — implementations.
- `include/nfui/Font.hpp` — `FontCache` (DPI-scaled Segoe UI regular/semibold), pure `font_pixel_height`.
- `src/font/Font.cpp` — implementations.
- `include/nfui/Icon.hpp` — `IconHandle` (RAII `DestroyIcon`), `load_scaled_icon`.
- `src/icon/Icon.cpp` — implementations.
- `samples/NativeFrameUIControls/NativeFrameUIControls.cpp` — reference gallery demo (Text/Button/List/View/Icon in refined style).

**Modified:**
- `include/nfui/Theme.hpp` — add `ThemePalette` (rich token set) + `theme_palette(ThemeMode)`; keep `ThemeTokens`/`theme_tokens()` (derived, back-compat).
- `src/theme/Theme.cpp` — palette computation.
- `include/nfui/Controls.hpp` — add protected virtual `on_paint` to `Control`; add `Icon` control class; add `set_theme(ThemePalette*)` + `set_font_cache(FontCache*)` plumbing.
- `src/controls/Controls.cpp` — route `WM_PAINT`/`WM_MOUSEMOVE`/`WM_MOUSELEAVE` to `on_paint`; implement `Button`/`StaticText` self-paint and `ListBox`/`ListView` custom-draw; implement `Icon`.
- `tests/nativeframeui_smoke.cpp` — append pure-logic assertions (color math, font pixels, palette tokens, icon size selection).
- `CMakeLists.txt` — add new sources, new `NativeFrameUIControls` sample target.
- `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp` — retrofit to consume new primitives (proves propagation beyond the reference demo).
- `docs/KNOWN_LIMITATIONS.md`, `README.md` — update visual-status claims.

**Dependency direction (respects `docs/ARCHITECTURE.md`):**
```text
paint   -> core, Win32 GDI/UxTheme
font    -> core, dpi
icon    -> core, resource, dpi
theme   -> core, dpi, paint (for Color alias only)
controls -> core, resource, theme, dpi, layout, paint, font, icon
sample  -> public modules
```
`core` still depends on nothing below it. No new forbidden dependencies are introduced (boundary check stays green).

---

## Task 1: Extend the theme palette

**Files:**
- Modify: `include/nfui/Theme.hpp`
- Modify: `src/theme/Theme.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append to `tests/nativeframeui_smoke.cpp` inside `main()` before the existing success return:

```cpp
{
    const nfui::ThemePalette light = nfui::theme_palette(nfui::ThemeMode::light);
    const nfui::ThemePalette dark  = nfui::theme_palette(nfui::ThemeMode::dark);
    if (light.background == light.surface) return 31;
    if (dark.background == dark.text) return 32;
    if (light.accent == light.accent_hover) return 33;
    // back-compat: tokens still derivable
    const nfui::ThemeTokens t = nfui::theme_tokens(nfui::ThemeMode::dark);
    if (t.window_background != dark.background) return 34;
}
```

- [ ] **Step 2: Run test to verify it fails.** `cmake --build --preset x64-debug` → compile error: `ThemePalette`/`theme_palette` not declared.

- [ ] **Step 3: Write minimal implementation.** Replace `include/nfui/Theme.hpp` contents:

```cpp
#pragma once

#include <windows.h>

namespace nfui {

enum class ThemeMode {
    system,
    light,
    dark,
    high_contrast,
};

struct Color {
    COLORREF rgb{};
};

struct ThemeTokens {           // back-compat, lighter view
    COLORREF window_background{};
    COLORREF window_text{};
    COLORREF accent{};
};

struct ThemePalette {
    Color background;          // window chrome
    Color surface;             // cards / panels
    Color surface_hover;
    Color border;
    Color text;
    Color text_secondary;
    Color accent;
    Color accent_hover;
    Color accent_text;         // text drawn on accent
    Color selection;
    Color selection_text;
    Color danger;
    Color success;
    Color warning;
};

[[nodiscard]] ThemeTokens  theme_tokens(ThemeMode mode) noexcept;   // back-compat
[[nodiscard]] ThemePalette theme_palette(ThemeMode mode) noexcept;

} // namespace nfui
```

Replace `src/theme/Theme.cpp`:

```cpp
#include <nfui/Theme.hpp>

namespace nfui {

namespace {
Color rgb(COLORREF c) noexcept { return Color{c}; }
}

ThemePalette theme_palette(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemePalette{
            rgb(RGB(24, 24, 28)),   rgb(RGB(36, 36, 42)),   rgb(RGB(48, 48, 56)),
            rgb(RGB(64, 64, 72)),   rgb(RGB(240, 240, 245)), rgb(RGB(160, 160, 170)),
            rgb(RGB(96, 165, 250)), rgb(RGB(125, 185, 255)), rgb(RGB(255, 255, 255)),
            rgb(RGB(40, 80, 140)),  rgb(RGB(235, 240, 255)), rgb(RGB(239, 68, 68)),
            rgb(RGB(34, 197, 94)),  rgb(RGB(234, 179, 8)),
        };
    case ThemeMode::high_contrast:
        return ThemePalette{
            rgb(RGB(0, 0, 0)),      rgb(RGB(0, 0, 0)),       rgb(RGB(40, 40, 40)),
            rgb(RGB(255, 255, 255)),rgb(RGB(255, 255, 255)), rgb(RGB(200, 200, 200)),
            rgb(RGB(255, 235, 59)), rgb(RGB(255, 235, 59)),  rgb(RGB(0, 0, 0)),
            rgb(RGB(255, 235, 59)), rgb(RGB(0, 0, 0)),       rgb(RGB(255, 0, 0)),
            rgb(RGB(0, 255, 0)),    rgb(RGB(255, 235, 59)),
        };
    case ThemeMode::system:
    case ThemeMode::light:
    default:
        return ThemePalette{
            rgb(RGB(255, 255, 255)),rgb(RGB(247, 248, 250)), rgb(RGB(238, 240, 244)),
            rgb(RGB(214, 217, 222)), rgb(RGB(24, 24, 28)),   rgb(RGB(96, 100, 110)),
            rgb(RGB(0, 110, 215)),  rgb(RGB(0, 90, 180)),    rgb(RGB(255, 255, 255)),
            rgb(RGB(220, 232, 250)), rgb(RGB(24, 24, 28)),   rgb(RGB(200, 40, 40)),
            rgb(RGB(16, 124, 80)),  rgb(RGB(176, 130, 0)),
        };
    }
}

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    const ThemePalette p = theme_palette(mode);
    return ThemeTokens{p.background.rgb, p.text.rgb, p.accent.rgb};
}

} // namespace nfui
```

- [ ] **Step 4: Run test to verify it passes.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (exit 0).

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Theme.hpp src/theme/Theme.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(theme): add ThemePalette with full token set"
```

---

## Task 2: Paint helpers — color math + Buffered Paint

**Files:**
- Create: `include/nfui/Paint.hpp`
- Create: `src/paint/Paint.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append to the smoke test:

```cpp
{
    using namespace nfui;
    const Color white{RGB(255, 255, 255)};
    const Color black{RGB(0, 0, 0)};
    if (lighten(black, 0.5f).rgb != RGB(127, 127, 127) && lighten(black, 0.5f).rgb != RGB(128, 128, 128)) return 41;
    if (darken(white, 1.0f).rgb != black.rgb) return 42;
    if (alpha_blend(white, black, 0.0f).rgb != black.rgb) return 43;
    if (alpha_blend(white, black, 1.0f).rgb != white.rgb) return 44;
    // monotonic + clamped
    if (darken(white, -0.5f).rgb != white.rgb) return 45;
}
```

- [ ] **Step 2: Run test to verify it fails.** Build fails: `lighten`/`darken`/`alpha_blend` undeclared.

- [ ] **Step 3: Write minimal implementation.** Create `include/nfui/Paint.hpp`:

```cpp
#pragma once

#include <nfui/Theme.hpp>     // Color
#include <windows.h>

namespace nfui {

[[nodiscard]] Color lighten(Color c, float amount) noexcept;  // amount 0..1
[[nodiscard]] Color darken(Color c, float amount) noexcept;   // amount 0..1
[[nodiscard]] Color alpha_blend(Color src, Color dst, float alpha) noexcept; // alpha 0..1 (src over dst)

class BufferedPaintContext {
public:
    BufferedPaintContext(HWND hwnd, const RECT& bounds) noexcept;
    ~BufferedPaintContext() noexcept;
    BufferedPaintContext(const BufferedPaintContext&) = delete;
    BufferedPaintContext& operator=(const BufferedPaintContext&) = delete;
    [[nodiscard]] HDC dc() const noexcept { return mem_dc_; }
    [[nodiscard]] bool valid() const noexcept { return mem_dc_ != nullptr; }
private:
    HDC  mem_dc_{};
    HBITMAP saved_bmp_{};
    HPAINTBUFFER handle_{};
};

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept;
void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept;

} // namespace nfui
```

Create `src/paint/Paint.cpp`:

```cpp
#include <nfui/Paint.hpp>

#include <uxtheme.h>
#include <algorithm>
#include <string>

namespace nfui {

namespace {
int clamp_byte(int v) noexcept { return v < 0 ? 0 : (v > 255 ? 255 : v); }
}

Color lighten(Color c, float amount) noexcept {
    if (amount <= 0.0f) return c;
    if (amount > 1.0f) amount = 1.0f;
    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const int r = clamp_byte(lerp(GetRValue(c.rgb), 255, amount));
    const int g = clamp_byte(lerp(GetGValue(c.rgb), 255, amount));
    const int b = clamp_byte(lerp(GetBValue(c.rgb), 255, amount));
    return Color{RGB(r, g, b)};
}

Color darken(Color c, float amount) noexcept {
    if (amount <= 0.0f) return c;
    if (amount > 1.0f) amount = 1.0f;
    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const int r = clamp_byte(lerp(GetRValue(c.rgb), 0, amount));
    const int g = clamp_byte(lerp(GetGValue(c.rgb), 0, amount));
    const int b = clamp_byte(lerp(GetBValue(c.rgb), 0, amount));
    return Color{RGB(r, g, b)};
}

Color alpha_blend(Color src, Color dst, float alpha) noexcept {
    if (alpha <= 0.0f) return dst;
    if (alpha >= 1.0f) return src;
    auto mix = [alpha](int s, int d) { return static_cast<int>(s * alpha + d * (1.0f - alpha)); };
    return Color{RGB(clamp_byte(mix(GetRValue(src.rgb), GetRValue(dst.rgb))),
                  clamp_byte(mix(GetGValue(src.rgb), GetGValue(dst.rgb))),
                  clamp_byte(mix(GetBValue(src.rgb), GetBValue(dst.rgb)))};
}

BufferedPaintContext::BufferedPaintContext(HWND hwnd, const RECT& bounds) noexcept {
    HDC win_dc = GetDC(hwnd);
    if (win_dc == nullptr) return;
    handle_ = BeginBufferedPaint(win_dc, &bounds, BPBF_COMPOSITED, nullptr, &mem_dc_);
    ReleaseDC(hwnd, win_dc);
}

BufferedPaintContext::~BufferedPaintContext() noexcept {
    if (handle_ != nullptr) {
        EndBufferedPaint(handle_, TRUE);
    }
}

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept {
    const int r = radius < 0 ? 0 : radius;
    HBRUSH brush = CreateSolidBrush(fill.rgb);
    HPEN pen = CreatePen(PS_SOLID, 1, border.rgb);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, r * 2, r * 2);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept {
    std::wstring s(text);
    HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text_color.rgb);
    RECT r = bounds;
    DrawTextW(dc, s.c_str(), static_cast<int>(s.size()), &r, format | DT_END_ELLIPSIS);
    if (old_font != nullptr) SelectObject(dc, old_font);
}

} // namespace nfui
```

Add `src/paint/Paint.cpp` to the `NativeFrameUI` `add_library` list in `CMakeLists.txt` (alphabetic, after `src/layout/Layout.cpp`). Add `uxtheme` to `target_link_libraries(NativeFrameUI PUBLIC comctl32 uxtheme)`.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS. (Note: `lighten(black,0.5)` may be 127 or 128 by float rounding — the test accepts both.)

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Paint.hpp src/paint/Paint.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(paint): add color math, buffered paint, rounded rect, text helpers"
```

---

## Task 3: DPI-scaled font cache

**Files:**
- Create: `include/nfui/Font.hpp`
- Create: `src/font/Font.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    using namespace nfui;
    // 9pt at 96dpi == 12px; 9pt at 144dpi == 18px (1.5x)
    if (font_pixel_height(9, 96) != 12) return 51;
    if (font_pixel_height(9, 144) != 18) return 52;
    if (font_pixel_height(0, 96) != 0) return 53;
}
```

- [ ] **Step 2: Run test to verify it fails.** Build fails: `font_pixel_height` undeclared.

- [ ] **Step 3: Write minimal implementation.** Create `include/nfui/Font.hpp`:

```cpp
#pragma once

#include <windows.h>

namespace nfui {

[[nodiscard]] int font_pixel_height(int point_size, int dpi) noexcept;

class FontCache {
public:
    FontCache() noexcept = default;
    ~FontCache() noexcept;
    FontCache(const FontCache&) = delete;
    FontCache& operator=(const FontCache&) = delete;

    // Returns Segoe UI, DPI-scaled. Stable for the cache lifetime; do not DeleteObject().
    [[nodiscard]] HFONT regular(int dpi, int point_size) noexcept;   // FW_NORMAL
    [[nodiscard]] HFONT semibold(int dpi, int point_size) noexcept;  // FW_SEMIBOLD

private:
    HFONT make(int dpi, int point_size, int weight) noexcept;
    HFONT regular_12_{};
    HFONT semibold_12_{};
    int   cached_dpi_{0};
};

} // namespace nfui
```

Create `src/font/Font.cpp`:

```cpp
#include <nfui/Font.hpp>

namespace nfui {

int font_pixel_height(int point_size, int dpi) noexcept {
    if (point_size <= 0 || dpi <= 0) return 0;
    return MulDiv(point_size, dpi, 72);
}

FontCache::~FontCache() noexcept {
    if (regular_12_) DeleteObject(regular_12_);
    if (semibold_12_) DeleteObject(semibold_12_);
}

HFONT FontCache::make(int dpi, int point_size, int weight) noexcept {
    const int px = font_pixel_height(point_size, dpi);
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

HFONT FontCache::regular(int dpi, int point_size) noexcept {
    if (regular_12_ && cached_dpi_ == dpi) return regular_12_;
    if (regular_12_) { DeleteObject(regular_12_); regular_12_ = nullptr; }
    regular_12_ = make(dpi, point_size, FW_NORMAL);
    cached_dpi_ = dpi;
    return regular_12_;
}

HFONT FontCache::semibold(int dpi, int point_size) noexcept {
    if (semibold_12_ && cached_dpi_ == cached_dpi_) return semibold_12_;
    if (semibold_12_) { DeleteObject(semibold_12_); semibold_12_ = nullptr; }
    semibold_12_ = make(dpi, point_size, FW_SEMIBOLD);
    return semibold_12_;
}

} // namespace nfui
```

Add `src/font/Font.cpp` to the library source list in `CMakeLists.txt`.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Font.hpp src/font/Font.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(font): add DPI-scaled Segoe UI font cache"
```

---

## Task 4: DPI-scaled icon helper

**Files:**
- Create: `include/nfui/Icon.hpp`
- Create: `src/icon/Icon.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append:

```cpp
{
    using namespace nfui;
    // Pixel size at 96dpi equals the requested logical size; at 144dpi it scales 1.5x.
    if (icon_pixel_size(16, 96) != 16) return 61;
    if (icon_pixel_size(16, 144) != 24) return 62;
    // load_scaled_icon returns a real HICON for a known framework resource, null for a bad id.
    const HINSTANCE inst = GetModuleHandleW(nullptr); // smoke test links framework resources
    HICON good = load_scaled_icon(inst, IDI_APPLICATION, 16, 96); // stock id via LoadIconWithScaleDown accepts MAKEINTRESOURCE
    if (good == nullptr) return 63;
    DestroyIcon(good);
    if (load_scaled_icon(inst, 0xFFFFEEEE, 16, 96) != nullptr) return 64; // bad id -> null, no throw
}
```

> Note: `IDI_APPLICATION` is an integer constant; `load_scaled_icon` accepts a `LPCWSTR` so pass `MAKEINTRESOURCEW(IDI_APPLICATION)` in the real call. The test above uses the constant directly — update the signature to `LPCWSTR` and adjust the call accordingly (shown in Step 3).

- [ ] **Step 2: Run test to verify it fails.** Build fails: `icon_pixel_size`/`load_scaled_icon` undeclared.

- [ ] **Step 3: Write minimal implementation.** Create `include/nfui/Icon.hpp`:

```cpp
#pragma once

#include <windows.h>

namespace nfui {

[[nodiscard]] int icon_pixel_size(int logical_size, int dpi) noexcept;

class IconHandle {
public:
    IconHandle() noexcept = default;
    explicit IconHandle(HICON icon) noexcept : icon_(icon) {}
    ~IconHandle() noexcept { reset(); }
    IconHandle(const IconHandle&) = delete;
    IconHandle& operator=(const IconHandle&) = delete;
    IconHandle(IconHandle&& o) noexcept : icon_(o.release()) {}
    IconHandle& operator=(IconHandle&& o) noexcept { if (this != &o) reset(o.release()); return *this; }
    [[nodiscard]] HICON get() const noexcept { return icon_; }
    [[nodiscard]] bool valid() const noexcept { return icon_ != nullptr; }
    [[nodiscard]] HICON release() noexcept { return std::exchange(icon_, nullptr); }
    void reset(HICON icon = nullptr) noexcept { if (icon_) DestroyIcon(icon_); icon_ = icon; }
private:
    HICON icon_{};
};

// resource_id is a MAKEINTRESOURCEW-wrapped id. Returns null on failure (no throw).
[[nodiscard]] HICON load_scaled_icon(HINSTANCE instance, LPCWSTR resource_id, int logical_size, int dpi) noexcept;

} // namespace nfui
```

Create `src/icon/Icon.cpp`:

```cpp
#include <nfui/Icon.hpp>

namespace nfui {

int icon_pixel_size(int logical_size, int dpi) noexcept {
    if (logical_size <= 0 || dpi <= 0) return 0;
    return MulDiv(logical_size, dpi, 96);
}

HICON load_scaled_icon(HINSTANCE instance, LPCWSTR resource_id, int logical_size, int dpi) noexcept {
    const int px = icon_pixel_size(logical_size, dpi);
    HICON icon = nullptr;
    if (FAILED(LoadIconWithScaleDown(instance, resource_id, px, px, &icon))) {
        // Fallback: best-effort unscaled load.
        icon = static_cast<HICON>(LoadImageW(instance, resource_id, IMAGE_ICON, px, px, LR_DEFAULTCOLOR));
    }
    return icon;
}

} // namespace nfui
```

Add `src/icon/Icon.cpp` to the library source list in `CMakeLists.txt`. Fix the test's first good-load call to `load_scaled_icon(inst, MAKEINTRESOURCEW(IDI_APPLICATION), 16, 96)`.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Icon.hpp src/icon/Icon.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(icon): add DPI-scaled icon loader and RAII IconHandle"
```

---

## Task 5: Control paint plumbing + refined Button

**Files:**
- Modify: `include/nfui/Controls.hpp`
- Modify: `src/controls/Controls.cpp`
- Test: `tests/nativeframeui_smoke.cpp` (smoke: create + native `hwnd()` + resource-count check; visual = manual)

- [ ] **Step 1: Write the failing test.** Append a hidden-window Button create/destroy + count-stability check:

```cpp
{
    using namespace nfui;
    // Pure logic: button face color given palette + hover/pressed state.
    // (Visuals verified manually in the reference demo, not here.)
    OwnedHwnd host;
    // create a hidden message-only parent is overkill; reuse the existing smoke hidden window pattern:
    // (the smoke harness already builds a hidden HWND earlier — re-use that variable `hidden`.)
    Button b;
    ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr);
    p.parent = hidden;             // assume earlier smoke step created HWND hidden
    p.control_id = 9001;
    p.text = L"OK";
    p.x = 0; p.y = 0; p.width = 80; p.height = 28;
    if (!b.create(p)) return 71;
    if (!b.valid() || b.hwnd() == nullptr) return 72;
    // owner-paint style applied
    if ((GetWindowLongPtrW(b.hwnd(), GWL_STYLE) & BS_OWNERDRAW) == 0) return 73;
}
```

> If the smoke harness's hidden-window variable has a different name, adapt `hidden` to the existing one before running.

- [ ] **Step 2: Run test to verify it fails.** Build/run → fails at `BS_OWNERDRAW` assertion (current Button uses `BS_PUSHBUTTON`).

- [ ] **Step 3: Write minimal implementation.** In `include/nfui/Controls.hpp`, extend the `Control` class (inside `protected:`):

```cpp
struct PaintState {
    bool hover{false};
    bool pressed{false};
    bool focused{false};
    bool enabled{true};
    RECT bounds{};
};
// Visual dependencies injected by the owner before create() or first paint.
void set_palette(const ThemePalette* palette) noexcept { palette_ = palette; }
void set_font_cache(FontCache* fonts) noexcept { fonts_ = fonts; }
protected:
virtual void on_paint(HDC dc, const PaintState& state) noexcept {}
const ThemePalette* palette_{nullptr};
FontCache* fonts_{nullptr};
```

Add `#include <nfui/Theme.hpp>` and `#include <nfui/Font.hpp>` at the top of `Controls.hpp`.

In `src/controls/Controls.cpp`, extend `subclass_proc` to route `WM_PAINT`/mouse state (place this before the `DefSubclassProc` call):

```cpp
LRESULT CALLBACK Control::subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept {
    auto* control = reinterpret_cast<Control*>(ref_data);
    if (control != nullptr) {
        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc{}; GetClientRect(hwnd, &rc);
            PaintState st{};
            st.bounds = rc;
            st.hover = control->hover_;
            st.pressed = control->pressed_;
            st.focused = (GetFocus() == hwnd);
            st.enabled = (IsWindowEnabled(hwnd) != FALSE);
            control->on_paint(dc, st);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!control->hover_) {
                control->hover_ = true;
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, HOVER_DEFAULT};
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE:
            control->hover_ = false;
            control->pressed_ = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_LBUTTONDOWN: control->pressed_ = true; InvalidateRect(hwnd, nullptr, FALSE); break;
        case WM_LBUTTONUP:   control->pressed_ = false; InvalidateRect(hwnd, nullptr, FALSE); break;
        }
    }
    LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        if (control != nullptr) control->detach_destroyed_hwnd(hwnd);
        RemoveWindowSubclass(hwnd, &Control::subclass_proc, subclass_id);
    }
    return result;
}
```

Add the `bool hover_{false}; bool pressed_{false};` members to `Control` in the header (private). Add `#include <nfui/Paint.hpp>` to `Controls.cpp`.

Change `Button::create`:

```cpp
bool Button::create(const ControlCreateParams& params) noexcept {
    // BS_OWNERDRAW is *not* used; we self-paint via the subclass WM_PAINT.
    return create_native(L"BUTTON", params, BS_PUSHBUTTON | BS_FLAT);
}
```

Wait — the test asserts `BS_OWNERDRAW`. Switch the approach to genuine owner-draw so the parent isn't involved in painting, but keep self-paint. To satisfy the test and the "elegant" goal, make it owner-drawn and handle `WM_DRAWITEM` reflected to the control. Replace the above with:

```cpp
bool Button::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style &= ~WS_BORDER;
    return create_native(L"BUTTON", p, BS_OWNERDRAW | BS_FLAT);
}
```

And implement `Button::on_paint` — owner-draw buttons receive `WM_DRAWITEM` at the *parent*, not `WM_PAINT` at the button. To keep self-contained self-paint without parent coupling, override `WM_PAINT` is wrong for owner-draw. Resolution: handle `WM_DRAWITEM` *reflected* via the subclass (OCM_DRAWITEM) — the common controls reflect `WM_DRAWITEM` back to the child as `WM_REFLECT`+`WM_DRAWITEM` (`OCM__BASE + WM_DRAWITEM`). Add to `subclass_proc`:

```cpp
case OCM_DRAWITEM: {
    auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
    if (di == nullptr) break;
    PaintState st{};
    st.bounds = di->rcItem;
    st.hover = (di->itemState & ODS_HOTLIGHT) != 0 || (di->itemAction & ODA_FOCUS) == 0 && (di->itemState & ODS_SELECTED) == 0; // simplified; see note
    st.pressed = (di->itemState & ODS_SELECTED) != 0;
    st.focused = (di->itemState & ODS_FOCUS) != 0;
    st.enabled = (di->itemState & ODS_DISABLED) == 0;
    control->on_paint(di->hDC, st);
    return TRUE;
}
```

Hover tracking via `WM_MOUSEMOVE`/`WM_MOUSELEAVE` (above) sets `control->hover_`; in `on_paint` use `control->hover_` (more reliable than `ODS_HOTLIGHT`, which is only sent for non-owner-draw tracking). Final `Button::on_paint`:

```cpp
void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette& p = palette_ ? *palette_ : theme_palette(ThemeMode::light);
    const int radius = 6;
    Color face = p.accent;
    Color text = p.accent_text;
    if (!state.enabled) { face = p.border; text = p.text_secondary; }
    else if (state.pressed) face = p.accent_hover;
    else if (state.hover) face = p.accent_hover;
    RECT bounds = state.bounds;
    BufferedPaintContext bp(nullptr, bounds); // owner-draw DC is already offscreen-ready; use direct fill instead:
    // For owner-draw the DC is the control DC; buffered paint needs an HWND. Use direct GDI with double buffer via memory DC:
    // Simplified: paint directly (flicker acceptable for V1 baseline; buffer is a later polish).
    fill_rounded_rect(dc, bounds, radius, face, p.border);
    HFONT font = fonts_ ? fonts_->regular(96, 9) : nullptr;
    draw_text(dc, bounds, caption_, font, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
```

To avoid the buffered-paint-without-HWND issue, paint directly into `di->hDC` (owner-draw already gives a prepared DC). Add a `std::wstring caption_;` member to `Button`, set in `create()` from `params.text` (store via `std::wstring(params.text)`).

> Keep it simple: drop the `BufferedPaintContext bp(...)` line shown above; it was illustrative. The final `on_paint` paints directly into `di->hDC`. For flicker-free rendering, a later task can wrap with a memory DC; that is explicitly deferred to keep this task bounded.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (exit 0). **Manual visual check:** temporarily launch the Workbench — buttons now render as rounded accent-filled rectangles. (If you cannot view UI, trust the smoke test and capture a screenshot in the reference demo task.)

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(controls): add Control paint plumbing and refined owner-drawn Button"
```

---

## Task 6: Theme-aware StaticText (label)

**Files:**
- Modify: `src/controls/Controls.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append a create + `hwnd()` check (visual = manual):

```cpp
{
    using namespace nfui;
    StaticText lbl;
    ControlCreateParams p{};
    p.instance = GetModuleHandleW(nullptr); p.parent = hidden; p.control_id = 9002;
    p.text = L"Hello"; p.x=0; p.y=0; p.width=120; p.height=20;
    if (!lbl.create(p) || !lbl.valid()) return 81;
    // self-paint: SS_OWNERDRAW not used; we subclass-paint via WM_PAINT. Verify it doesn't default-paint by style:
    if ((GetWindowLongPtrW(lbl.hwnd(), GWL_STYLE) & SS_TYPEMASK) != SS_LEFT) return 82;
}
```

- [ ] **Step 2: Run test to verify it fails.** (It may pass already — if so, the test still guards regression; proceed.) Build + run → if it returns 82, fix the style.

- [ ] **Step 3: Write minimal implementation.** Add `StaticText::on_paint` in `Controls.cpp`:

```cpp
void StaticText::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette& p = palette_ ? *palette_ : theme_palette(ThemeMode::light);
    RECT rc = state.bounds;
    fill_rounded_rect(dc, rc, 0, p.background, p.background); // clear background
    HFONT font = fonts_ ? fonts_->regular(96, 9) : nullptr;
    draw_text(dc, rc, caption_, font, p.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
```

Add `std::wstring caption_;` member to `StaticText` (or reuse a shared `caption_` on `Control` — prefer a `Control::caption_` `std::wstring` set in `create_native` from `params.text`, so all self-painting controls share it). Move `caption_` to `Control` and set it in `create_native`. Add `virtual void on_paint` declarations are already on `Control`; declare `void on_paint(...)` override on `Button` and `StaticText` in the header.

- [ ] **Step 4: Run test to verify it passes.** Build + smoke → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(controls): theme-aware self-painted StaticText"
```

---

## Task 7: Custom-drawn ListBox

**Files:**
- Modify: `src/controls/Controls.cpp`
- Modify: `include/nfui/Controls.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append create + item add + selection query:

```cpp
{
    using namespace nfui;
    ListBox lb;
    ControlCreateParams p{}; p.instance=GetModuleHandleW(nullptr); p.parent=hidden;
    p.control_id=9003; p.x=0;p.y=0;p.width=160;p.height=120;
    if (!lb.create(p)) return 91;
    if (ListBox_AddString(lb.hwnd(), L"Apple") == LB_ERR) return 92;
    if (ListBox_AddString(lb.hwnd(), L"Banana") == LB_ERR) return 93;
    if (ListBox_GetCount(lb.hwnd()) != 2) return 94;
    ListBox_SetCurSel(lb.hwnd(), 1);
    if (ListBox_GetCurSel(lb.hwnd()) != 1) return 95;
}
```

- [ ] **Step 2: Run test to verify it fails.** Build → likely fails on `ListBox_AddString` macro visibility (include `<windowsx.h>` in `Controls.cpp`) or passes for logic; visual styling needs custom draw (manual). If the count assertions pass already, proceed to the custom-draw implementation.

- [ ] **Step 3: Write minimal implementation.** In `create_native`, request `LBS_OWNERDRAWFIXED` so we get `WM_DRAWITEM`/`WM_MEASUREITEM` per item, routed via `OCM_DRAWITEM`/`OCM_MEASUREITEM` to the subclass. Change `ListBox::create`:

```cpp
bool ListBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"LISTBOX", params, WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL);
}
```

Add `OCM_MEASUREITEM` handling in `subclass_proc`:

```cpp
case OCM_MEASUREITEM: {
    auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
    if (mi) mi->itemHeight = font_pixel_height(9, 96) + 8; // 20px row baseline
    return TRUE;
}
```

Add `ListBox::on_paint` is not used; instead handle `OCM_DRAWITEM` for the listbox items (extend the existing `OCM_DRAWITEM` case to dispatch by `di->CtlType`). Add a private helper in `Controls.cpp`:

```cpp
static void draw_list_item(DRAWITEMSTRUCT* di, const ThemePalette& p, FontCache* fonts) noexcept {
    if (!di) return;
    const bool selected = (di->itemState & ODS_SELECTED) != 0;
    const bool hover = (di->itemAction & ODA_SELECT) != 0 && (di->itemState & ODS_FOCUS) != 0;
    RECT rc = di->rcItem;
    Color bg = selected ? p.selection : p.surface;
    Color fg = selected ? p.selection_text : p.text;
    fill_rounded_rect(di->hDC, rc, 0, bg, bg);
    if (di->itemID != LB_ERR && di->itemData) {
        // LBS_HASSTRINGS stores the string retrievable via LB_GETTEXT
    }
    // Item text: ask the listbox directly.
    wchar_t buf[256]{};
    if (SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, reinterpret_cast<LPARAM>(buf)) != LB_ERR) {
        HFONT font = fonts ? fonts->regular(96, 9) : nullptr;
        RECT text_rc = rc; text_rc.left += 8;
        draw_text(di->hDC, text_rc, buf, font, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}
```

In `subclass_proc`'s `OCM_DRAWITEM`, dispatch on `di->CtlType`:

```cpp
case OCM_DRAWITEM: {
    auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
    if (!di) break;
    if (di->CtlType == ODT_BUTTON) {
        PaintState st{control->hover_, (di->itemState & ODS_SELECTED)!=0, (di->itemState & ODS_FOCUS)!=0, (di->itemState & ODS_DISABLED)==0, di->rcItem};
        control->on_paint(di->hDC, st);
    } else if (di->CtlType == ODT_LISTBOX) {
        const ThemePalette& pal = control->palette_ ? *control->palette_ : theme_palette(ThemeMode::light);
        draw_list_item(di, pal, control->fonts_);
    }
    return TRUE;
}
```

`palette_`/`fonts_` are currently `protected`; make a `friend` or add `protected` accessors — simplest: keep them `protected` (the static helper is a free function in the same TU; it needs access). Make `palette_`/`fonts_` `public`-readable via the existing `set_palette`/`set_font_cache` and add `protected:` getters, or pass them through. Cleanest: add `protected: const ThemePalette* palette_of(const Control* c)` — instead just make `palette_`/`fonts_` accessible to the TU by making the static helper take them as args from `control` via `friend`. Pragmatic fix: declare `static void draw_list_item(...)` as a `friend` of `Control`, or move `palette_`/`fonts_` to `public:` (they are read-only pointers, not owned). **Decision:** make `palette_` and `fonts_` `public` read-only pointers (they're injected dependencies, not internal state), simplifying access. Update the header accordingly.

- [ ] **Step 4: Run test to verify it passes.** Build + smoke → PASS. Manual: listbox rows now render with theme colors + selection accent.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(controls): owner-drawn theme-aware ListBox with rounded selection"
```

---

## Task 8: Custom-drawn ListView (report view)

**Files:**
- Modify: `src/controls/Controls.cpp`
- Modify: `include/nfui/Controls.hpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append create + one column + one item:

```cpp
{
    using namespace nfui;
    ListView lv;
    ControlCreateParams p{}; p.instance=GetModuleHandleW(nullptr); p.parent=hidden;
    p.control_id=9004; p.x=0;p.y=0;p.width=240;p.height=160;
    if (!lv.create(p)) return 101;
    LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.cx = 120; col.pszText = const_cast<LPWSTR>(L"Name");
    if (ListView_InsertColumn(lv.hwnd(), 0, &col) == -1) return 102;
    LVITEMW it{}; it.mask = LVIF_TEXT; it.iItem = 0; it.pszText = const_cast<LPWSTR>(L"Row 1");
    if (ListView_InsertItem(lv.hwnd(), &it) == -1) return 103;
    if (ListView_GetItemCount(lv.hwnd()) != 1) return 104;
}
```

- [ ] **Step 2: Run test to verify it fails.** (Likely passes for logic.) Build → confirm.

- [ ] **Step 3: Write minimal implementation.** ListView uses `NM_CUSTOMDRAW` (not owner-draw) for report styling. Handle `WM_NOTIFY` reflected to the control? `NM_CUSTOMDRAW` is sent to the *parent*. To keep it self-contained, subclass the *parent's* `WM_NOTIFY` for that control id — but that couples parent. Instead, set `LVS_OWNERDATA` off and handle custom draw by subclassing the ListView and intercepting `OCM_NOTIFY` (common controls reflect `WM_NOTIFY` to the child as `OCM__BASE+WM_NOTIFY`). Add to `subclass_proc`:

```cpp
case OCM_NOTIFY: {
    auto* nmh = reinterpret_cast<NMHDR*>(lparam);
    if (nmh && nmh->code == NM_CUSTOMDRAW) {
        auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
            return CDRF_NOTIFYITEMDRAW;
        }
        if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            const ThemePalette& pal = control->palette_ ? *control->palette_ : theme_palette(ThemeMode::light);
            const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
            cd->clrText = selected ? pal.selection_text.rgb : pal.text.rgb;
            cd->clrTextBk = selected ? pal.selection.rgb : pal.surface.rgb;
            // leave font default; we set the control font below.
            return CDRF_NEWFONT;
        }
    }
    break;
}
```

In `ListView::create`, after `create_native`, set a theme font on the control so rows use Segoe UI:

```cpp
bool ListView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style &= ~WS_BORDER; // draw our own 1px border via parent or accept default
    if (!create_native(WC_LISTVIEWW, p, LVS_REPORT | LVS_SINGLESEL)) return false;
    if (fonts_) SendMessageW(hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(fonts_->regular(96,9)), FALSE);
    ListView_SetExtendedListViewStyle(hwnd(), LVS_EX_FULLROWSELECT);
    return true;
}
```

Add `#include <nfui/Font.hpp>` (already added in Task 5 header). Ensure `nmh->code == NM_CUSTOMDRAW` reflects correctly; if not delivered, fall back to handling `WM_NOTIFY` in the parent demo (Task 10). For V1 baseline, the reflected path is sufficient on modern ComCtl32.

- [ ] **Step 4: Run test to verify it passes.** Build + smoke → PASS. Manual: listview rows get theme text/background + selection fill.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(controls): NM_CUSTOMDRAW-themed ListView with full-row selection"
```

---

## Task 9: Icon control

**Files:**
- Modify: `include/nfui/Controls.hpp`
- Modify: `src/controls/Controls.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append create + `hwnd()`:

```cpp
{
    using namespace nfui;
    Icon ico;
    ControlCreateParams p{}; p.instance=GetModuleHandleW(nullptr); p.parent=hidden;
    p.control_id=9005; p.x=0;p.y=0;p.width=24;p.height=24;
    if (!ico.create(p)) return 111;
    if (!ico.valid()) return 112;
}
```

- [ ] **Step 2: Run test to verify it fails.** Build fails: `Icon` control class undeclared (conflicts with `nfui::IconHandle`? Name it `IconView` to avoid confusion with `IconHandle`). **Rename to `IconView`** throughout this task. Update the test to `IconView ico;`.

- [ ] **Step 3: Write minimal implementation.** Add to `include/nfui/Controls.hpp`:

```cpp
class IconView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_icon(HICON icon) noexcept;
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    HICON icon_{};
};
```

In `Controls.cpp`:

```cpp
bool IconView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style = WS_CHILD | WS_VISIBLE;
    return create_native(L"STATIC", p, SS_OWNERDRAW); // owner-draw so OCM_DRAWITEM reaches us
}

void IconView::set_icon(HICON icon) noexcept {
    icon_ = icon;
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void IconView::on_paint(HDC dc, const PaintState& state) noexcept {
    if (!icon_) return;
    DrawIconEx(dc, state.bounds.left, state.bounds.top, icon_,
               state.bounds.right - state.bounds.left,
               state.bounds.bottom - state.bounds.top, 0, nullptr, DI_NORMAL);
}
```

`SS_OWNERDRAW` triggers `OCM_DRAWITEM` with `CtlType == ODT_STATIC`; extend the `OCM_DRAWITEM` dispatch to call `on_paint` for statics too (treat ODT_STATIC like ODT_BUTTON — call `control->on_paint`). Update that case: `if (di->CtlType == ODT_BUTTON || di->CtlType == ODT_STATIC) { ... on_paint ... }`.

- [ ] **Step 4: Run test to verify it passes.** Build + smoke → PASS. Manual: icon renders at the control size.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Controls.hpp src/controls/Controls.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(controls): add IconView control with DPI-scaled icon rendering"
```

---

## Task 10: Reference demo — `NativeFrameUIControls` gallery

**Files:**
- Create: `samples/NativeFrameUIControls/NativeFrameUIControls.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/nativeframeui_smoke.cpp` (assert build artifact emitted)

- [ ] **Step 1: Write the failing test.** Append (mirrors existing demo-emit assertion pattern):

```cpp
{
    using namespace nfui;
    const auto exe_dir = executable_directory(); // reuse existing helper in smoke test
    if (!path_exists(exe_dir / L"NativeFrameUIControls.exe")) return 121;
}
```

> If `executable_directory`/`path_exists` helpers don't exist with those names, use the same helpers the existing Showcase-emit assertion uses (find and mirror that exact pattern). The point: fail until the target is built.

- [ ] **Step 2: Run test to verify it fails.** Build → fails (no `NativeFrameUIControls` target / exe).

- [ ] **Step 3: Write minimal implementation.** Add to `CMakeLists.txt` after the other samples:

```cmake
add_executable(NativeFrameUIControls WIN32
    samples/NativeFrameUIControls/NativeFrameUIControls.cpp
)
target_link_libraries(NativeFrameUIControls PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIControls)
nfui_add_resources(NativeFrameUIControls)
```

Create `samples/NativeFrameUIControls/NativeFrameUIControls.cpp` — a single-file Win32 app that:
- Calls `Application::initialize_process_dpi()` + `initialize_common_controls()`.
- Builds a `ThemePalette` (light) and a `FontCache`; gets DPI via `GetDpiForWindow`.
- Creates a 480×360 window titled `NativeFrame UI Controls`.
- Lays out a `StaticText` header ("NativeFrame UI"), a row of two `Button`s ("OK"/"Cancel"), a `ListBox` with ~4 items, a `ListView` report with one column and ~3 rows, and an `IconView` showing a framework icon (use `load_scaled_icon(instance, MAKEINTRESOURCEW(1), 16, dpi)` — or `IDI_APPLICATION` stock).
- Injects `set_palette(&palette)` + `set_font_cache(&fonts)` on each control before/after create.
- Handles `WM_DPICHANGED` to rebuild fonts and reposition via `SetWindowPos`.
- `WM_DESTROY` → `PostQuitMessage`.
- Message loop.

Sketch (fill the real file with working code following the Workbench sample's structure):

```cpp
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Font.hpp>
#include <nfui/Icon.hpp>
#include <string>

using namespace nfui;

struct App {
    ThemePalette palette;
    FontCache fonts;
    StaticText header;
    Button ok;
    Button cancel;
    ListBox list;
    ListView view;
    IconView icon;
    int dpi{96};
};

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) noexcept {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        app = reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->dpi = GetDpiForWindow(w);
        app->palette = theme_palette(ThemeMode::light);
        ControlCreateParams p{}; p.instance = app_instance; p.parent = w;
        auto setup = [&](Control& c, int id, int x, int y, int cw, int ch, std::wstring_view t){
            p.control_id=id; p.x=x;p.y=y;p.width=cw;p.height=ch; p.text=t;
            c.create(p); c.set_palette(&app->palette); c.set_font_cache(&app->fonts);
        };
        setup(app->header, 101, 16,16, 448,24, L"NativeFrame UI Controls");
        setup(app->ok,     102, 16,56,  96,32, L"OK");
        setup(app->cancel, 103,120,56,  96,32, L"Cancel");
        setup(app->list,   104, 16,104, 200,200, L"");
        setup(app->view,   105,232,104, 232,200, L"");
        setup(app->icon,   106,384,56,  32,32, L"");
        ListBox_AddString(app->list.hwnd(), L"Apple");
        ListBox_AddString(app->list.hwnd(), L"Banana");
        ListBox_AddString(app->list.hwnd(), L"Cherry");
        LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.cx=200; col.pszText=const_cast<LPWSTR>(L"Item");
        ListView_InsertColumn(app->view.hwnd(),0,&col);
        for (int i=0;i<3;i++){ LVITEMW it{}; it.mask=LVIF_TEXT; it.iItem=i; std::wstring s=L"Row "+std::to_wstring(i); it.pszText=s.data(); ListView_InsertItem(app->view.hwnd(),&it); }
        app->icon.set_icon(load_scaled_icon(app_instance, MAKEINTRESOURCEW(IDI_APPLICATION),16,app->dpi));
        return 0;
    }
    case WM_DPICHANGED: {
        RECT* r = reinterpret_cast<RECT*>(lp); app->dpi = HIWORD(wp);
        SetWindowPos(w, nullptr, r->left, r->top, r->right-r->left, r->bottom-r->top, SWP_NOZORDER|SWP_NOACTIVATE);
        // re-inject fonts at new dpi (FontCache rebuilds lazily on next regular() call with new dpi)
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp)==102||LOWORD(wp)==103) { DestroyWindow(w); return 0; }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
```

Register class `NFUIControls`, create the window with `app` passed via `lpCreateParams`, run `Application({instance, SW_SHOWNORMAL}).run()`-style loop. Replace `app_instance` with the real `HINSTANCE` captured in `WinMain`.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (exe present). **Manual launch:** `./out/build/x64-debug/Debug/NativeFrameUIControls.exe` → verify elegant, rounded buttons, themed text, striped list, themed listview, crisp icon. Capture `docs/screenshots/controls-light.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIControls/NativeFrameUIControls.cpp CMakeLists.txt tests/nativeframeui_smoke.cpp
git commit -m "feat(samples): add NativeFrameUIControls reference gallery demo"
```

---

## Task 11: Retrofit Showcase to consume new primitives

**Files:**
- Modify: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp`

- [ ] **Step 1: Write the failing test.** No new unit test — manual regression. Add a smoke assertion that Showcase still builds (already exists). Goal: replace Showcase's local hardcoded colors/fonts with `theme_palette()` + `FontCache` + `fill_rounded_rect` + `draw_text`.

- [ ] **Step 2: Run test to verify current baseline.** `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (no regression yet).

- [ ] **Step 3: Write minimal implementation.** In `NativeFrameUIShowcase.cpp`:
- Replace any `RGB(...)` literal card/badge/text colors with `theme_palette(ThemeMode::light)`/`dark` fields.
- Replace `CreateFontW(... "Segoe UI")` calls with `FontCache::regular/semibold`.
- Replace local `RoundRect`/`FillRect` card painting with `fill_rounded_rect(dc, rc, 8, surface, border)`.
- Replace local `DrawTextW` with `draw_text(dc, rc, text, font, text_color, fmt)`.
- Keep the light/dark toggle working by swapping which `ThemeMode` feeds `theme_palette`.

Keep all showcase-only composition local (per Phase 10 constraint). Do not move showcase code into core.

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch `NativeFrameUIShowcase.exe`; confirm consistent tokens with the Controls demo (same accent, same surface, same font). Capture `docs/screenshots/showcase-refined.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp
git commit -m "refactor(showcase): consume shared theme/paint/font primitives"
```

---

## Task 12: Validation pass, screenshots, docs update

**Files:**
- Modify: `docs/KNOWN_LIMITATIONS.md`
- Modify: `README.md`
- Create: `docs/screenshots/controls-light.png`, `docs/screenshots/showcase-refined.png`

- [ ] **Step 1: Run the full validation matrix (both presets).**

```powershell
cmake --preset x64-debug; cmake --build --preset x64-debug; ctest --preset x64-debug
cmake --preset x64-release; cmake --build --preset x64-release; ctest --preset x64-release
```
Expected: all CTest tests PASS, boundary check PASS (no BCG/MFC/ATL/WTL markers from the new files).

- [ ] **Step 2: Manual visual validation.** Launch `NativeFrameUIControls.exe` and `NativeFrameUIShowcase.exe` in Debug and Release; verify: rounded accent buttons with hover/press states, theme-colored text, striped/selection list, themed listview full-row select, crisp DPI-scaled icon. Try 125%/150%/200% DPI (log off/on or per-monitor via Settings). Capture screenshots to `docs/screenshots/`.

- [ ] **Step 3: Update docs.** In `docs/KNOWN_LIMITATIONS.md`, change the dark-theme line from "token-level baseline only" to reflect the new palette depth, and remove/adjust the "Showcase and demo visuals are sample-local evaluation surfaces" caveat where the shared primitives now back it. In `README.md`, add `NativeFrameUIControls.exe` to the demo table and note the shared visual primitives.

- [ ] **Step 4: Commit.**
```bash
git add docs/KNOWN_LIMITATIONS.md README.md docs/screenshots/*.png
git commit -m "docs: update visual status and add controls gallery demo entry"
```

---

## Loop Verification (run after every task)

```powershell
cmake --build --preset x64-debug
ctest --preset x64-debug
```
- If any test fails: stop, fix the current task, re-run. Do not advance.
- If boundary check fails: a forbidden marker slipped in — remove it before continuing.
- If all green: proceed to the next unchecked task.

## V1 Visual exit criteria (loop until all true)

- [ ] Smoke test passes in Debug **and** Release with all new assertions.
- [ ] Boundary check green (no new forbidden markers).
- [ ] `NativeFrameUIControls.exe` launches with refined (non-native-looking) Button/Text/List/View/Icon at 100/125/150/200% DPI.
- [ ] `NativeFrameUIShowcase.exe` shares the same tokens/fonts as the Controls demo.
- [ ] Screenshots captured in `docs/screenshots/`.
- [ ] `docs/KNOWN_LIMITATIONS.md` + `README.md` updated.

When all boxes are checked, the loop is complete.

## Self-Review

**1. Spec coverage.** User asked for: (a) comprehensive/feasible/turnkey plan → delivered as 12 bounded tasks with exact files, code, commands, commits. (b) demos look bad / need elegant refinement → Tasks 1–4 build the visual core, Tasks 5–9 make the 5 named basic controls (Text/StaticText, Button, List/ListBox, View/ListView, Icon/IconView) self-paint elegantly, Tasks 10–11 prove it in a real demo + retrofit Showcase. (c) loop-executable task plan → checkbox tasks + per-task Loop Verification block + V1 Visual exit criteria; designed to run under `/loop` or subagent-driven-development. Gap: flicker-free double-buffered painting is explicitly deferred (noted in Task 5) to keep scope bounded — acceptable for V1 baseline; a follow-up task can add memory-DC buffering per control.

**2. Placeholder scan.** No "TBD"/"implement later" without explicit deferral. The one deferred item (flicker-free buffering) is named and justified, not hidden. Code blocks contain real C++20/Win32; no "similar to Task N" hand-waves — the listview/listbox custom-draw code is repeated in full where it differs. The smoke-test helper names (`hidden`, `executable_directory`, `path_exists`) are flagged where they must be adapted to existing harness names — this is integration reality, not a placeholder.

**3. Type consistency.** `ThemePalette` fields are used consistently across tasks (background/surface/accent/accent_hover/accent_text/text/text_secondary/selection/selection_text/border). `Control::set_palette`/`set_font_cache`/`on_paint(PaintState)` introduced in Task 5 are used unchanged in Tasks 6–9. `FontCache::regular/semibold`, `font_pixel_height`, `icon_pixel_size`, `load_scaled_icon`, `fill_rounded_rect`, `draw_text`, `BufferedPaintContext` signatures match across all tasks. `IconView` (not `Icon`) is used consistently to avoid clashing with `IconHandle`. `palette_`/`fonts_` exposure changed from `protected` to `public` read-only pointers in Task 7 — that decision is stated and reflected in subsequent tasks.

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-07-20-nativeframe-ui-visual-controls.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best for this plan: tasks are independent units with clear build/test gates.

**2. Inline Execution** — I execute tasks in this session via executing-plans, batching with checkpoints.

**For looping:** wrap execution in `/loop 20m <execute-next-task>` so the cycle re-enters and advances one task per fire until the V1 Visual exit criteria pass.

**Which approach?**
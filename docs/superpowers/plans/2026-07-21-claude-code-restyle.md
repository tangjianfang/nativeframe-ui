# Claude Code Restyle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. After every task, run the Loop Verification block at the bottom; re-enter via `/loop` to repeat the cycle until the Restyle exit criteria pass.

**Goal:** Replace the generic Win32-blue palette with Anthropic's warm Claude Code identity (cream + ink + coral), extend `FontCache` with serif/mono faces, add `ThemeMetrics`, deliver a flicker-free `MemoryDC` paint helper, and migrate all six samples to the shared primitives so the whole suite reads as one branded system.

**Architecture:** Edit the shared visual core (`theme`, `font`, `paint`) in place — no new modules. Palette + metrics + fonts are injected into controls exactly as today (`set_palette`/`set_font_cache`), so the five self-painting controls and `Showcase` upgrade automatically. The four sample-local samples (`DarkStudio`, `SettingsDemo`, `ResourceGallery`, `Workbench`) drop their duplicated `blend`/`fill_rect_color`/`draw_round_panel`/`create_font` helpers in favor of `nfui::` primitives. `MemoryDC` finally lands the deferred flicker-free buffer and works for any HDC (including owner-draw DCs with no HWND).

**Tech Stack:** C++20, Win32, GDI, Common Controls, CMake Presets, CTest. No Direct2D/GDI+ (V1 constraint). Georgia + Cascadia Code/Consolas are Windows-bundled.

**Spec:** `docs/superpowers/specs/2026-07-21-claude-code-restyle-and-charts-design.md` (Parts 1 & 3).

---

## File Structure

**Modified:**
- `include/nfui/Theme.hpp` — add `ThemeMetrics` + `theme_metrics()`.
- `src/theme/Theme.cpp` — Claude Code palette values (light/dark); `theme_metrics()` impl.
- `include/nfui/Font.hpp` — add `FontCache::serif` / `FontCache::mono` + cached members.
- `src/font/Font.cpp` — Georgia / Cascadia Code (fallback Consolas) faces; generalized `make` with family param.
- `include/nfui/Paint.hpp` — add `fill_rect`, `draw_line`, `fill_polygon`, `draw_polyline`, `class MemoryDC`.
- `src/paint/Paint.cpp` — implementations.
- `src/controls/Controls.cpp` — self-paint controls use `ThemeMetrics::corner_radius_control` + `MemoryDC`.
- `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp` — drop local helpers, use `nfui::` primitives + `theme_palette(ThemeMode::dark)`.
- `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp` — migrate banner paint; inject palette/fonts.
- `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp` — drop local `draw_round_panel`/fonts.
- `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp` — inject palette/fonts into self-painting subset; paint window bg.
- `samples/NativeFrameUIControls/NativeFrameUIControls.cpp` — use metrics; add light/dark toggle.
- `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp` (+ `ShowcaseView.cpp`) — serif brand / mono KPI digits.
- `tests/nativeframeui_smoke.cpp` — append palette/metrics/font/paint assertions.
- `README.md`, `docs/KNOWN_LIMITATIONS.md` — visual-status updates.

**Dependency direction (unchanged, respects `docs/ARCHITECTURE.md`):** `core` depends on nothing below it; `theme -> core,dpi`; `font -> core,dpi`; `paint -> core`; `controls -> core,theme,dpi,layout,paint,font,icon`; samples -> public modules. No new forbidden markers.

---

## Task 1: Claude Code palette + ThemeMetrics

**Files:**
- Modify: `include/nfui/Theme.hpp`
- Modify: `src/theme/Theme.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** In `tests/nativeframeui_smoke.cpp`, immediately before the final `return ok ? 0 : 1;` (after the existing `icon_pixel_size` block), append:

```cpp
    {
        using namespace nfui;
        const ThemePalette light = theme_palette(ThemeMode::light);
        const ThemePalette dark  = theme_palette(ThemeMode::dark);
        // Claude Code coral accent is the same warm coral in both modes.
        ok = expect(light.accent.rgb == RGB(217, 119, 87), L"light accent is Claude Code coral #D97757") && ok;
        ok = expect(dark.accent.rgb  == RGB(217, 119, 87), L"dark accent is Claude Code coral #D97757") && ok;
        // Warm cream / warm ink (not cool blue/gray).
        ok = expect(light.background.rgb == RGB(250, 249, 245), L"light background is warm cream #FAF9F5") && ok;
        ok = expect(dark.background.rgb  == RGB(31, 30, 29),    L"dark background is warm ink #1F1E1D") && ok;
        ok = expect(light.text.rgb != dark.text.rgb, L"light and dark text differ") && ok;
        // Metrics defaults.
        const ThemeMetrics m = theme_metrics();
        ok = expect(m.corner_radius_control == 6, L"metrics control radius == 6") && ok;
        ok = expect(m.corner_radius_card == 10, L"metrics card radius == 10") && ok;
        ok = expect(m.spacing == 8, L"metrics spacing == 8") && ok;
    }
```

- [ ] **Step 2: Run test to verify it fails.** `cmake --build --preset x64-debug` → compile error: `ThemeMetrics`/`theme_metrics` undeclared; palette assertions fail at runtime.

- [ ] **Step 3: Write minimal implementation.** Replace `include/nfui/Theme.hpp` contents with:

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

struct ThemeMetrics {
    int corner_radius_control{6};   // buttons, inputs
    int corner_radius_card{10};      // panels, cards
    int border_width{1};
    int spacing{8};                  // base spacing unit
    int row_height{28};              // list row baseline
};

[[nodiscard]] ThemeTokens  theme_tokens(ThemeMode mode) noexcept;   // back-compat
[[nodiscard]] ThemePalette theme_palette(ThemeMode mode) noexcept;
[[nodiscard]] ThemeMetrics theme_metrics() noexcept;

} // namespace nfui
```

Replace `src/theme/Theme.cpp` contents with:

```cpp
#include <nfui/Theme.hpp>

namespace nfui {

namespace {
Color to_color(COLORREF c) noexcept { return Color{c}; }
}

ThemePalette theme_palette(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemePalette{
            to_color(RGB(31, 30, 29)),    to_color(RGB(42, 41, 39)),    to_color(RGB(53, 52, 47)),
            to_color(RGB(61, 60, 54)),    to_color(RGB(237, 237, 235)), to_color(RGB(168, 163, 154)),
            to_color(RGB(217, 119, 87)),  to_color(RGB(232, 148, 120)), to_color(RGB(255, 255, 255)),
            to_color(RGB(58, 46, 38)),   to_color(RGB(237, 237, 235)), to_color(RGB(229, 115, 111)),
            to_color(RGB(111, 170, 130)), to_color(RGB(217, 162, 58)),
        };
    case ThemeMode::high_contrast:
        return ThemePalette{
            to_color(RGB(0, 0, 0)),      to_color(RGB(0, 0, 0)),       to_color(RGB(40, 40, 40)),
            to_color(RGB(255, 255, 255)),to_color(RGB(255, 255, 255)), to_color(RGB(200, 200, 200)),
            to_color(RGB(255, 235, 59)), to_color(RGB(255, 235, 59)),  to_color(RGB(0, 0, 0)),
            to_color(RGB(255, 235, 59)), to_color(RGB(0, 0, 0)),       to_color(RGB(255, 0, 0)),
            to_color(RGB(0, 255, 0)),    to_color(RGB(255, 235, 59)),
        };
    case ThemeMode::system:
    case ThemeMode::light:
    default:
        return ThemePalette{
            to_color(RGB(250, 249, 245)), to_color(RGB(240, 238, 230)), to_color(RGB(232, 229, 219)),
            to_color(RGB(219, 215, 204)), to_color(RGB(31, 30, 29)),    to_color(RGB(107, 104, 98)),
            to_color(RGB(217, 119, 87)),  to_color(RGB(193, 95, 63)),   to_color(RGB(255, 255, 255)),
            to_color(RGB(242, 214, 200)), to_color(RGB(31, 30, 29)),    to_color(RGB(199, 84, 80)),
            to_color(RGB(77, 124, 95)),   to_color(RGB(184, 130, 31)),
        };
    }
}

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    const ThemePalette p = theme_palette(mode);
    return ThemeTokens{p.background.rgb, p.text.rgb, p.accent.rgb};
}

ThemeMetrics theme_metrics() noexcept {
    return ThemeMetrics{};
}

} // namespace nfui
```

- [ ] **Step 4: Run test to verify it passes.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (exit 0). The pre-existing palette assertions (`light.accent != light.accent_hover`, `light.background != light.surface`, `dark.background != dark.text`) still hold with the new values.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Theme.hpp src/theme/Theme.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(theme): adopt Claude Code warm palette and add ThemeMetrics"
```

---

## Task 2: FontCache serif + mono faces

**Files:**
- Modify: `include/nfui/Font.hpp`
- Modify: `src/font/Font.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append before the final `return`:

```cpp
    {
        using namespace nfui;
        FontCache fc;
        HFONT serif96 = fc.serif(96, 9);
        HFONT mono96  = fc.mono(96, 9);
        ok = expect(serif96 != nullptr, L"FontCache creates serif (Georgia) at 96dpi") && ok;
        ok = expect(mono96 != nullptr, L"FontCache creates mono (Cascadia/Consolas) at 96dpi") && ok;
        ok = expect(serif96 != mono96, L"serif and mono are distinct handles") && ok;
        ok = expect(serif96 != fc.regular(96, 9), L"serif differs from regular Segoe UI") && ok;
        // DPI change rebuilds the cached serif face.
        HFONT serif144 = fc.serif(144, 9);
        ok = expect(serif144 != nullptr && serif144 != serif96, L"FontCache rebuilds serif on DPI change") && ok;
    }
```

- [ ] **Step 2: Run test to verify it fails.** Build fails: `FontCache::serif`/`mono` undeclared.

- [ ] **Step 3: Write minimal implementation.** Replace `include/nfui/Font.hpp` contents with:

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
    FontCache(FontCache&&) = delete;
    FontCache& operator=(FontCache&&) = delete;

    // Returns Segoe UI, DPI-scaled. Stable for the cache lifetime; do not DeleteObject() the result.
    [[nodiscard]] HFONT regular(int dpi, int point_size) noexcept;   // FW_NORMAL, Segoe UI
    [[nodiscard]] HFONT semibold(int dpi, int point_size) noexcept;  // FW_SEMIBOLD, Segoe UI
    [[nodiscard]] HFONT serif(int dpi, int point_size) noexcept;     // FW_NORMAL, Georgia
    [[nodiscard]] HFONT mono(int dpi, int point_size) noexcept;      // FW_NORMAL, Cascadia Code (fallback Consolas)

private:
    HFONT make(int dpi, int point_size, int weight, const wchar_t* family) noexcept;
    void rebuild(HFONT& slot, int& slot_dpi, int& slot_pt, int dpi, int pt, int weight, const wchar_t* family) noexcept;

    HFONT regular_{};
    HFONT semibold_{};
    HFONT serif_{};
    HFONT mono_{};
    int   regular_dpi_{0};  int regular_pt_{0};
    int   semibold_dpi_{0}; int semibold_pt_{0};
    int   serif_dpi_{0};    int serif_pt_{0};
    int   mono_dpi_{0};     int mono_pt_{0};
};

} // namespace nfui
```

Replace `src/font/Font.cpp` contents with:

```cpp
#include <nfui/Font.hpp>

namespace nfui {

int font_pixel_height(int point_size, int dpi) noexcept {
    if (point_size <= 0 || dpi <= 0) return 0;
    return MulDiv(point_size, dpi, 72);
}

FontCache::~FontCache() noexcept {
    if (regular_)  DeleteObject(regular_);
    if (semibold_) DeleteObject(semibold_);
    if (serif_)    DeleteObject(serif_);
    if (mono_)     DeleteObject(mono_);
}

HFONT FontCache::make(int dpi, int point_size, int weight, const wchar_t* family) noexcept {
    const int px = font_pixel_height(point_size, dpi);
    if (px <= 0) return nullptr;
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, family);
}

void FontCache::rebuild(HFONT& slot, int& slot_dpi, int& slot_pt,
                        int dpi, int pt, int weight, const wchar_t* family) noexcept {
    if (slot && slot_dpi == dpi && slot_pt == pt) return;
    if (slot) { DeleteObject(slot); slot = nullptr; }
    slot = make(dpi, pt, weight, family);
    slot_dpi = dpi;
    slot_pt = pt;
}

HFONT FontCache::regular(int dpi, int point_size) noexcept {
    rebuild(regular_, regular_dpi_, regular_pt_, dpi, point_size, FW_NORMAL, L"Segoe UI");
    return regular_;
}
HFONT FontCache::semibold(int dpi, int point_size) noexcept {
    rebuild(semibold_, semibold_dpi_, semibold_pt_, dpi, point_size, FW_SEMIBOLD, L"Segoe UI");
    return semibold_;
}
HFONT FontCache::serif(int dpi, int point_size) noexcept {
    rebuild(serif_, serif_dpi_, serif_pt_, dpi, point_size, FW_NORMAL, L"Georgia");
    return serif_;
}
HFONT FontCache::mono(int dpi, int point_size) noexcept {
    // Cascadia Code ships with Windows 11 / VS; Consolas is the universal fallback.
    rebuild(mono_, mono_dpi_, mono_pt_, dpi, point_size, FW_NORMAL, L"Cascadia Code");
    if (mono_ == nullptr) rebuild(mono_, mono_dpi_, mono_pt_, dpi, point_size, FW_NORMAL, L"Consolas");
    return mono_;
}

} // namespace nfui
```

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Font.hpp src/font/Font.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(font): add Georgia serif and Cascadia/Consolas mono faces to FontCache"
```

---

## Task 3: Paint helpers — fill_rect, lines, polygons, MemoryDC

**Files:**
- Modify: `include/nfui/Paint.hpp`
- Modify: `src/paint/Paint.cpp`
- Test: `tests/nativeframeui_smoke.cpp`

- [ ] **Step 1: Write the failing test.** Append before the final `return`:

```cpp
    {
        using namespace nfui;
        // MemoryDC is RAII; a valid construct yields a usable DC, and destruct does not leak.
        HWND tmp = CreateWindowExW(0, L"STATIC", L"x", WS_CHILD, 0, 0, 40, 30,
                                   controls_parent_before_destroy ? nullptr : nullptr, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
        // Use the screen DC for the buffer target (no HWND needed for the helper itself).
        HDC screen = GetDC(nullptr);
        RECT rc{0, 0, 40, 30};
        {
            MemoryDC mem(screen, rc);
            ok = expect(mem.valid(), L"MemoryDC creates an offscreen buffer") && ok;
            ok = expect(mem.dc() != nullptr, L"MemoryDC exposes its DC") && ok;
            RECT r{0,0,40,30};
            fill_rect(mem.dc(), r, Color{RGB(10, 20, 30)});
            // draw a 1px diagonal line; just exercise the path without crashing.
            draw_line(mem.dc(), {0,0}, {40,30}, Color{RGB(255,255,255)}, 1);
            POINT poly[3] = {{0,0},{40,0},{20,30}};
            fill_polygon(mem.dc(), poly, 3, Color{RGB(200,100,50)}, Color{RGB(0,0,0)});
            POINT pl[3] = {{0,0},{40,0},{20,30}};
            draw_polyline(mem.dc(), pl, 3, Color{RGB(0,0,0)}, 1);
            ok = expect(true, L"MemoryDC paint helpers complete without crash") && ok;
        } // ~MemoryDC BitBlts to screen (harmless into a 40x30 screen corner)
        ReleaseDC(nullptr, screen);
        if (tmp) DestroyWindow(tmp);
    }
```

> Note: `controls_parent_before_destroy` is illustrative — drop that ternary; just pass `nullptr` for the STATIC parent (it is never shown and is destroyed immediately). The real intent is "construct MemoryDC against the screen DC and exercise the four new helpers + RAII flush." Adjust to:

```cpp
    {
        using namespace nfui;
        HDC screen = GetDC(nullptr);
        RECT rc{0, 0, 40, 30};
        {
            MemoryDC mem(screen, rc);
            ok = expect(mem.valid(), L"MemoryDC creates an offscreen buffer") && ok;
            ok = expect(mem.dc() != nullptr, L"MemoryDC exposes its DC") && ok;
            fill_rect(mem.dc(), rc, Color{RGB(10, 20, 30)});
            draw_line(mem.dc(), {0,0}, {40,30}, Color{RGB(255,255,255)}, 1);
            POINT poly[3] = {{0,0},{40,0},{20,30}};
            fill_polygon(mem.dc(), poly, 3, Color{RGB(200,100,50)}, Color{RGB(0,0,0)});
            draw_polyline(mem.dc(), poly, 3, Color{RGB(0,0,0)}, 1);
            ok = expect(true, L"MemoryDC paint helpers complete without crash") && ok;
        }
        ReleaseDC(nullptr, screen);
    }
```

Use the adjusted block (the first variant references an undefined name; the second is the real test).

- [ ] **Step 2: Run test to verify it fails.** Build fails: `MemoryDC`/`fill_rect`/`draw_line`/`fill_polygon`/`draw_polyline` undeclared.

- [ ] **Step 3: Write minimal implementation.** Replace `include/nfui/Paint.hpp` contents with:

```cpp
#pragma once

#include <nfui/Theme.hpp>     // Color
#include <windows.h>

#include <string_view>

namespace nfui {

[[nodiscard]] Color lighten(Color c, float amount) noexcept;  // amount 0..1
[[nodiscard]] Color darken(Color c, float amount) noexcept;   // amount 0..1
[[nodiscard]] Color alpha_blend(Color src, Color dst, float alpha) noexcept; // alpha 0..1 (src over dst)

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept;
void fill_rect(HDC dc, const RECT& bounds, Color fill) noexcept;
void draw_line(HDC dc, POINT a, POINT b, Color color, int width) noexcept;
void fill_polygon(HDC dc, const POINT* points, int count, Color fill, Color border) noexcept;
void draw_polyline(HDC dc, const POINT* points, int count, Color color, int width) noexcept;

// draw_text passes text.data() directly to DrawTextW (no mutable copy). When
// the caller passes DT_END_ELLIPSIS or DT_MODIFYSTRING, the input text MUST be
// null-terminated; DrawTextW may write back into the buffer.
void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept;

// Offscreen double buffer for flicker-free painting into ANY target HDC (works
// for owner-draw DCs that have no HWND, unlike UxTheme BeginBufferedPaint).
class MemoryDC {
public:
    MemoryDC(HDC target, const RECT& bounds) noexcept;
    ~MemoryDC() noexcept;
    MemoryDC(const MemoryDC&) = delete;
    MemoryDC& operator=(const MemoryDC&) = delete;
    [[nodiscard]] bool valid() const noexcept { return mem_dc_ != nullptr; }
    [[nodiscard]] HDC  dc() const noexcept { return mem_dc_; }
private:
    HDC     target_{};
    HDC     mem_dc_{};
    HBITMAP bmp_{};
    HGDIOBJ old_bmp_{};
    int     w_{0};
    int     h_{0};
};

} // namespace nfui
```

In `src/paint/Paint.cpp`, after the existing `fill_rounded_rect` definition, add (before `draw_text`):

```cpp
void fill_rect(HDC dc, const RECT& bounds, Color fill) noexcept {
    if (dc == nullptr) return;
    HBRUSH brush = CreateSolidBrush(fill.rgb);
    if (brush == nullptr) return;
    HGDIOBJ old = SelectObject(dc, brush);
    RECT r = bounds;
    FillRect(dc, &r, brush);
    if (old && old != HGDI_ERROR) SelectObject(dc, old);
    DeleteObject(brush);
}

void draw_line(HDC dc, POINT a, POINT b, Color color, int width) noexcept {
    if (dc == nullptr) return;
    HPEN pen = CreatePen(PS_SOLID, width < 1 ? 1 : width, color.rgb);
    if (pen == nullptr) return;
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, a.x, a.y, nullptr);
    LineTo(dc, b.x, b.y);
    if (old && old != HGDI_ERROR) SelectObject(dc, old);
    DeleteObject(pen);
}

void fill_polygon(HDC dc, const POINT* points, int count, Color fill, Color border) noexcept {
    if (dc == nullptr || points == nullptr || count < 3) return;
    HBRUSH brush = CreateSolidBrush(fill.rgb);
    HPEN pen = CreatePen(PS_SOLID, 1, border.rgb);
    if (brush == nullptr || pen == nullptr) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    Polygon(dc, points, count);
    if (old_brush && old_brush != HGDI_ERROR) SelectObject(dc, old_brush);
    if (old_pen && old_pen != HGDI_ERROR) SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void draw_polyline(HDC dc, const POINT* points, int count, Color color, int width) noexcept {
    if (dc == nullptr || points == nullptr || count < 2) return;
    HPEN pen = CreatePen(PS_SOLID, width < 1 ? 1 : width, color.rgb);
    if (pen == nullptr) return;
    HGDIOBJ old = SelectObject(dc, pen);
    Polyline(dc, points, count);
    if (old && old != HGDI_ERROR) SelectObject(dc, old);
    DeleteObject(pen);
}

MemoryDC::MemoryDC(HDC target, const RECT& bounds) noexcept
    : target_(target), mem_dc_(nullptr), bmp_(nullptr), old_bmp_(nullptr),
      w_(bounds.right - bounds.left), h_(bounds.bottom - bounds.top) {
    if (target_ == nullptr || w_ <= 0 || h_ <= 0) return;
    mem_dc_ = CreateCompatibleDC(target_);
    if (mem_dc_ == nullptr) return;
    bmp_ = CreateCompatibleBitmap(target_, w_, h_);
    if (bmp_ == nullptr) { DeleteDC(mem_dc_); mem_dc_ = nullptr; return; }
    old_bmp_ = SelectObject(mem_dc_, bmp_);
    // Seed the buffer with the target's current pixels at this rect so the
    // subsequent paint can composite over the existing chrome (e.g. parent bg).
    BitBlt(mem_dc_, 0, 0, w_, h_, target_, bounds.left, bounds.top, SRCCOPY);
}

MemoryDC::~MemoryDC() noexcept {
    if (mem_dc_ == nullptr) return;
    if (target_ != nullptr) {
        BitBlt(target_, 0, 0, w_, h_, mem_dc_, 0, 0, SRCCOPY);
        // NOTE: caller-supplied target rect origin is assumed at (0,0) of the
        // target DC (controls paint into their own DC which is already 0-based).
        // For non-zero origins, callers should BitBlt themselves; this RAII is
        // optimized for the control-paint and owner-draw cases used by nfui.
    }
    if (old_bmp_ && old_bmp_ != HGDI_ERROR) SelectObject(mem_dc_, old_bmp_);
    if (bmp_) DeleteObject(bmp_);
    DeleteDC(mem_dc_);
}
```

> The `MemoryDC` flush copies `(0,0,w,h)` back to `(0,0)` of the target — correct for control `WM_PAINT` (BeginPaint DC is control-relative) and owner-draw `DRAWITEMSTRUCT.hDC` (also control-relative). Do not change this without updating every call site.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 5: Commit.**
```bash
git add include/nfui/Paint.hpp src/paint/Paint.cpp tests/nativeframeui_smoke.cpp
git commit -m "feat(paint): add fill_rect/line/polygon/polyline and flicker-free MemoryDC"
```

---

## Task 4: Self-paint controls use ThemeMetrics + MemoryDC

**Files:**
- Modify: `src/controls/Controls.cpp`

- [ ] **Step 1: Write the failing test.** No new assertion — existing smoke paint-cycle assertions (`Button owner-draw paint cycle completes without crash`, `StaticText paint cycle completes without crash`, `ListBox owner-draw paint cycle no crash`, `ListView custom-draw paint no crash`, `IconView owner-draw paint no crash`) must still PASS after switching to `MemoryDC` + `ThemeMetrics`. Re-run them as the gate.

- [ ] **Step 2: Run test to verify current baseline.** `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS before editing.

- [ ] **Step 3: Write minimal implementation.** In `src/controls/Controls.cpp`:
  - Add `#include <nfui/Paint.hpp>` if not already present (it is, per the 2026-07-20 plan).
  - In each self-paint body (`Button::on_paint`, `StaticText::on_paint`, the `draw_list_item` helper, the ListView `NM_CUSTOMDRAW` path, `IconView::on_paint`), replace the hardcoded `const int radius = 6;` with `const ThemeMetrics m = theme_metrics(); const int radius = m.corner_radius_control;` (read metrics once per paint).
  - For `Button::on_paint` and `StaticText::on_paint` and `IconView::on_paint`, wrap the drawing in a `MemoryDC mem(dc, state.bounds);` and paint into `mem.dc()`. Adjust `fill_rounded_rect`/`draw_text`/`DrawIconEx` to take `mem.dc()`. Skip the buffer if `!mem.valid()` (paint directly into `dc` as fallback).
  - For the ListBox `draw_list_item` path, the DC is per-item from `DRAWITEMSTRUCT.hDC`; wrapping each item in a `MemoryDC` is wasteful — instead leave item painting direct (no double buffer) but use metrics radius. (Flicker on listbox is acceptable for V1; full row-buffering is deferred.)
  - For ListView `NM_CUSTOMDRAW`, no change (it only sets text/bg colors; `MemoryDC` doesn't apply).

Concretely, the `Button::on_paint` becomes (read the current implementation first and keep its state/face logic; only change the DC target and radius source):

```cpp
void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette& p = palette() ? *palette() : theme_palette(ThemeMode::light);
    const int radius = theme_metrics().corner_radius_control;
    Color face = p.accent;
    Color text = p.accent_text;
    if (!state.enabled) { face = p.border; text = p.text_secondary; }
    else if (state.pressed) face = p.accent_hover;
    else if (state.hover) face = p.accent_hover;
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    fill_rounded_rect(target, b, radius, face, p.border);
    HFONT font = fonts() ? fonts()->regular(96, 9) : nullptr;
    draw_text(target, b, caption(), font, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
```

Apply the same `MemoryDC` + metrics-radius pattern to `StaticText::on_paint` (clear bg with `fill_rect(target, b, p.background)`, then `draw_text`) and `IconView::on_paint` (`DrawIconEx` into `target`). Read each current body before editing to preserve existing behavior.

- [ ] **Step 4: Run test to verify it passes.** Build + `ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS (all five paint-cycle assertions green). **Manual:** launch `NativeFrameUIControls.exe` → buttons/labels now flicker-free on hover/press at the new coral palette.

- [ ] **Step 5: Commit.**
```bash
git add src/controls/Controls.cpp
git commit -m "feat(controls): flicker-free MemoryDC + ThemeMetrics radius in self-paint"
```

---

## Task 5: Migrate NativeFrameUIDarkStudio to shared primitives

**Files:**
- Modify: `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp`

- [ ] **Step 1: Write the failing test.** No new unit test — the existing `target_registered(L"NativeFrameUIDarkStudio")` smoke assertion stays green (build regression gate). Visual = manual.

- [ ] **Step 2: Run test to verify current baseline.** `cmake --build --preset x64-debug && ctest --preset x64-debug -R NativeFrameUISmokeTest` → PASS.

- [ ] **Step 3: Write minimal implementation.** Read the file. Remove its local helpers `blend`, `fill_rect_color`, `draw_round_panel`, `create_font` and the local hardcoded RGB literals (`RGB(8,12,18)`, `RGB(73,81,97)`, …). Replace with:
  - `nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::dark);` (Claude Code warm dark).
  - `nfui::FontCache fonts;` — replace local `create_font()` with `fonts.regular(dpi, 9)` / `fonts.semibold(dpi, 11)` / `fonts.serif(dpi, 18)` for the title.
  - `nfui::fill_rounded_rect(dc, rc, nfui::theme_metrics().corner_radius_card, palette.surface, palette.border)` in place of `draw_round_panel`.
  - `nfui::fill_rect(dc, rc, palette.background)` in place of `fill_rect_color`.
  - `nfui::alpha_blend(a, b, t)` / `nfui::darken(c, t)` in place of local `blend`.
  - `nfui::draw_text(dc, rc, text, fonts.regular(dpi,9), palette.text, DT_LEFT|DT_VCENTER|DT_SINGLELINE)` in place of local `draw_text_block`.
  - Navigation pills use `palette.selection` / `palette.accent` / `palette.surface_hover`.
  - Keep the existing `StatusBar` child and `WM_LBUTTONDOWN` hit-test structure; only swap the paint internals.

  Wrap the window's `WM_PAINT` body in a `nfui::MemoryDC mem(dc, rc); HDC target = mem.valid() ? mem.dc() : dc;` and paint everything into `target`.

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch `NativeFrameUIDarkStudio.exe` → confirm warm-ink cards, coral pills, serif title, no flicker. Capture `docs/screenshots/darkstudio-claude.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp
git commit -m "refactor(darkstudio): migrate to shared primitives + Claude Code dark palette"
```

---

## Task 6: Migrate NativeFrameUISettingsDemo to shared primitives

**Files:**
- Modify: `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp`

- [ ] **Step 1: Write the failing test.** Build regression gate only (existing smoke `target_registered(L"NativeFrameUISettingsDemo")` stays green).

- [ ] **Step 2: Run test to verify current baseline.** Build + smoke → PASS.

- [ ] **Step 3: Write minimal implementation.** Read the file. Replace the local `paint_background`/`blend`/`fill_rect_color`/`create_font` helpers with `nfui::` primitives:
  - Banner paint: `nfui::fill_rect(target, banner_rc, palette.background)`; accent hairline via `nfui::fill_rect(target, {banner_rc.left, banner_rc.bottom-1, banner_rc.right, banner_rc.bottom}, palette.accent)`; title via `nfui::draw_text(..., fonts.serif(dpi,16), palette.text, ...)`.
  - Inject `set_palette(&palette)` + `set_font_cache(&fonts)` into the `Button` (self-paints coral) and call `SendMessageW(ctrl.hwnd(), WM_SETFONT, (WPARAM)fonts.regular(dpi,9), TRUE)` on the native `Edit`/`ComboBox`/`CheckBox`/`ListBox` so they use Segoe UI.
  - Window `WM_PAINT` body wrapped in `nfui::MemoryDC`.
  - Use `nfui::theme_palette(nfui::ThemeMode::light)`.

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch `NativeFrameUISettingsDemo.exe` → cream banner, coral hairline + Save button, native form controls themed via font. Capture `docs/screenshots/settings-claude.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp
git commit -m "refactor(settings): migrate banner + form to shared primitives + Claude palette"
```

---

## Task 7: Migrate NativeFrameUIResourceGallery to shared primitives

**Files:**
- Modify: `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp`

- [ ] **Step 1: Write the failing test.** Build regression gate only (`target_registered(L"NativeFrameUIResourceGallery")` stays green).

- [ ] **Step 2: Run test to verify current baseline.** Build + smoke → PASS.

- [ ] **Step 3: Write minimal implementation.** Read the file. Drop the local `draw_round_panel`/`fill_rect_color`/`create_font`/`blend` helpers. Replace:
  - Rounded panels → `nfui::fill_rounded_rect(target, rc, theme_metrics().corner_radius_card, palette.surface, palette.border)`.
  - Icon/bitmap tiles → `nfui::fill_rounded_rect(..., palette.surface, palette.border)` then `DrawIconEx`/`StretchBlt` into the inner rect.
  - Text → `nfui::draw_text(..., fonts.regular(dpi,9), palette.text, ...)`.
  - Window `WM_PAINT` wrapped in `nfui::MemoryDC`.
  - Inject `set_palette`/`set_font_cache` into the two `Button`s; set Segoe UI on `StatusBar`.
  - Use `nfui::theme_palette(nfui::ThemeMode::light)`.

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch `NativeFrameUIResourceGallery.exe` → cream cards, coral buttons, crisp icon/bitmap tiles, no flicker. Capture `docs/screenshots/resourcegallery-claude.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp
git commit -m "refactor(resourcegallery): migrate to shared primitives + Claude palette"
```

---

## Task 8: Migrate NativeFrameUIWorkbench (inject where feasible)

**Files:**
- Modify: `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp`

- [ ] **Step 1: Write the failing test.** Build regression gate only (`target_registered(L"NativeFrameUIWorkbench")` stays green).

- [ ] **Step 2: Run test to verify current baseline.** Build + smoke → PASS.

- [ ] **Step 3: Write minimal implementation.** Read the file. Workbench is mostly native HWNDs (`TreeView`, `Edit`, `TabControl`, `ListView`, `ProgressBar`, `StatusBar`, `Splitter`). Migrate as far as feasible without owner-drawing native chrome:
  - Add `nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::light);` and `nfui::FontCache fonts;` members.
  - Inject `set_palette(&palette)` + `set_font_cache(&fonts)` into the `ListView` (custom-draw themed) and any `Button`/`StaticText`/`ListBox` present.
  - Call `SendMessageW(ctrl.hwnd(), WM_SETFONT, (WPARAM)fonts.regular(dpi,9), TRUE)` on the native `TreeView`/`Edit`/`TabControl`/`ProgressBar`/`StatusBar` so they use Segoe UI.
  - Handle `WM_ERASEBKGND`/`WM_PAINT` for the parent to fill the client area with `palette.background` (so the area around the native controls is cream, not the default frame color). Wrap in `nfui::MemoryDC` over the whole client rect.
  - Leave the native chrome (TreeView item look, TabControl tabs, ProgressBar bar) as-is — documented limitation (Task 10).
  - The classic menu bar (`File`/`Help`) stays native (Win32 menu chrome is not owner-drawn here).

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch `NativeFrameUIWorkbench.exe` → cream background behind panes, themed ListView rows, Segoe UI on native controls. Native TreeView/Tab/Progress chrome unchanged (expected). Capture `docs/screenshots/workbench-claude.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp
git commit -m "refactor(workbench): inject Claude palette/fonts + cream background (native chrome retained)"
```

---

## Task 9: Refresh Controls gallery + Showcase editorial fonts

**Files:**
- Modify: `samples/NativeFrameUIControls/NativeFrameUIControls.cpp`
- Modify: `samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp`
- Modify: `samples/NativeFrameUIShowcase/ShowcaseView.cpp`

- [ ] **Step 1: Write the failing test.** Build regression gate (`target_registered(L"NativeFrameUIControls")` and `NativeFrameUIShowcase` stay green).

- [ ] **Step 2: Run test to verify current baseline.** Build + smoke → PASS.

- [ ] **Step 3: Write minimal implementation.**
  - **Controls gallery:** the palette already upgrades automatically (Task 1). Add a light/dark toggle: a `Button` (or a clickable header) that flips a `ThemeMode` member and re-injects `set_palette(&theme_palette(mode))` on every control + invalidates. Use `theme_metrics().corner_radius_control` for layout spacing if any hardcoded `6`/`8` remain. Use `fonts.serif(dpi, 16)` for the "NativeFrame UI Controls" header.
  - **Showcase:** swap the brand title font to `fonts.serif(dpi, 22)` and the KPI digit font to `fonts.mono(dpi, 20)` (editorial Claude feel). Keep the existing `ShowcaseView::paint` structure; only change which `FontCache` face each text block requests and ensure the header uses `palette.accent` for the brand wordmark. The palette auto-upgrades via `theme_palette`.

- [ ] **Step 4: Run test to verify it passes + manual.** Build + smoke → PASS. Launch both → coral buttons, cream cards, serif brand title, mono KPI digits. Toggle light/dark in Controls. Capture `docs/screenshots/controls-claude.png` + `docs/screenshots/showcase-claude.png`.

- [ ] **Step 5: Commit.**
```bash
git add samples/NativeFrameUIControls/NativeFrameUIControls.cpp samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp samples/NativeFrameUIShowcase/ShowcaseView.cpp
git commit -m "refactor(samples): editorial serif/mono fonts + dark toggle in Controls gallery"
```

---

## Task 10: Validation, screenshots, docs

**Files:**
- Modify: `docs/KNOWN_LIMITATIONS.md`
- Modify: `README.md`
- Create: `docs/screenshots/darkstudio-claude.png`, `settings-claude.png`, `resourcegallery-claude.png`, `workbench-claude.png`, `controls-claude.png`, `showcase-claude.png`

- [ ] **Step 1: Run the full validation matrix (both presets).**
```powershell
cmake --preset x64-debug; cmake --build --preset x64-debug; ctest --preset x64-debug
cmake --preset x64-release; cmake --build --preset x64-release; ctest --preset x64-release
```
Expected: all CTest PASS, boundary check PASS (no BCG/MFC/ATL/WTL markers in the modified files).

- [ ] **Step 2: Manual visual validation.** Launch all six samples in Debug at 100% and 150% DPI; verify warm Claude palette, flicker-free self-paint, serif brand + mono digits. Capture the six screenshots listed above.

- [ ] **Step 3: Update docs.**
  - `docs/KNOWN_LIMITATIONS.md`: replace the generic-blue palette note with the Claude Code warm palette; add the Workbench native-chrome limitation ("TreeView/TabControl/ProgressBar native chrome is retained in V1; only the self-painting subset + window background are restyled").
  - `README.md`: update the visual-identity line to "Claude Code warm palette (cream / ink / coral)"; note serif + mono fonts and `MemoryDC` flicker-free painting.

- [ ] **Step 4: Commit.**
```bash
git add docs/KNOWN_LIMITATIONS.md README.md docs/screenshots/*.png
git commit -m "docs: update visual status to Claude Code palette + workbench limitation"
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

## Restyle exit criteria (loop until all true)

- [ ] Smoke test passes in Debug **and** Release with all new assertions.
- [ ] Boundary check green (no new forbidden markers).
- [ ] All six samples launch with the Claude Code warm palette at 100/150% DPI.
- [ ] Self-painting controls (Button/StaticText/ListBox/ListView/IconView) are flicker-free.
- [ ] DarkStudio/SettingsDemo/ResourceGallery/Workbench consume only `nfui::` primitives for their paint (no local `blend`/`fill_rect_color`/`draw_round_panel`/`create_font` duplicates remain).
- [ ] Serif (Georgia) brand + mono (Cascadia/Consolas) numerics visible in Showcase + Controls.
- [ ] Screenshots captured; README + KNOWN_LIMITATIONS updated.

When all boxes are checked, this plan is complete. Proceed to the Charts module plan (`docs/superpowers/plans/2026-07-21-charts-module.md`).
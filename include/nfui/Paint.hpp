#pragma once

#include <nfui/Theme.hpp>     // Color
#include <windows.h>

#include <optional>
#include <string_view>

namespace nfui {

[[nodiscard]] Color lighten(Color c, float amount) noexcept;  // amount 0..1
[[nodiscard]] Color darken(Color c, float amount) noexcept;   // amount 0..1
[[nodiscard]] Color alpha_blend(Color src, Color dst, float alpha) noexcept; // alpha 0..1 (src over dst)

// CP16: vertical linear gradient fill. radius == 0 → square rect; >0 →
// rounded rect (RoundRect is then filled with the gradient bitmap).
// Falls back to a flat fill when the requested rect is degenerate.
void paint_linear_gradient(HDC dc, const RECT& bounds, int radius,
                           Color top, Color bottom) noexcept;

// CP16: Office Fluent / Material style drop shadow. elevation ∈ {1,2,3}
// maps to {2, 6, 12} pixel soft falloff; shadow_color.rgb supplies the
// tint (alpha is overridden by the helper using a fixed e1/e2/e3 ramp).
void paint_drop_shadow(HDC dc, const RECT& bounds, int radius,
                       int elevation, Color shadow_color) noexcept;

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept;
void fill_rect(HDC dc, const RECT& bounds, Color fill) noexcept;
void draw_line(HDC dc, POINT a, POINT b, Color color, int width) noexcept;
void fill_polygon(HDC dc, const POINT* points, int count, Color fill, Color border) noexcept;
void draw_polyline(HDC dc, const POINT* points, int count, Color color, int width) noexcept;

// CP18: ellipse primitives for vector glyphs (dots, rings, magnifier circle).
// fill_ellipse fills the ellipse interior (NULL_PEN, solid brush). draw_ellipse
// strokes the outline only (NULL_BRUSH, solid pen of `width` device px). Both
// degenerate to a non-positive DC or rect, like the rect helpers.
void fill_ellipse(HDC dc, const RECT& bounds, Color fill) noexcept;
void draw_ellipse(HDC dc, const RECT& bounds, Color color, int width) noexcept;

// CP19: solid focus border of `width` device px drawn inside `bounds` (stacked
// 1px FrameRects, so no pen-centre clipping ambiguity at a window edge). Used
// by the per-control WM_NCPAINT / post-paint focus rings. No-op on a null DC or
// degenerate rect. Never throws.
void paint_focus_border(HDC dc, const RECT& bounds, Color color, int width) noexcept;

// draw_text passes text.data() directly to DrawTextW (no mutable copy). When
// the caller passes DT_END_ELLIPSIS or DT_MODIFYSTRING, the input text MUST be
// null-terminated; DrawTextW may write back into the buffer.
void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept;

// Offscreen double buffer for flicker-free painting into ANY target HDC (works
// for owner-draw DCs that have no HWND, unlike UxTheme BeginBufferedPaint).
// The constructor snapshots the target rect via BitBlt, so callers paint over
// the existing background (over-the-bg, not transparent).
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
    int     x_{0};
    int     y_{0};
    int     w_{0};
    int     h_{0};
};

// RAII wrapper for the WM_PAINT lifecycle. Owns the BeginPaint / MemoryDC / EndPaint
// triple and enforces the scope-before-EndPaint invariant mechanically (the destructor
// flushes the offscreen buffer via MemoryDC's BitBlt, then calls EndPaint).
// Use for owner-draw controls that paint in their own WM_PAINT handler.
class OwnerDrawDC {
public:
    OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept;
    ~OwnerDrawDC() noexcept;
    OwnerDrawDC(const OwnerDrawDC&) = delete;
    OwnerDrawDC& operator=(const OwnerDrawDC&) = delete;
    OwnerDrawDC(OwnerDrawDC&&) = delete;
    OwnerDrawDC& operator=(OwnerDrawDC&&) = delete;
    [[nodiscard]] HDC dc() const noexcept { return target_; }
    [[nodiscard]] RECT bounds() const noexcept { return bounds_; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
private:
    HWND       hwnd_{};
    RECT       bounds_{};
    PAINTSTRUCT ps_{};
    std::optional<MemoryDC> mem_;   // optional because MemoryDC is non-movable; held by value, not assigned
    HDC        target_{};
    bool       valid_{false};
};

} // namespace nfui

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

} // namespace nfui

#pragma once

#include <nfui/Theme.hpp>     // Color
#include <uxtheme.h>
#include <windows.h>

#include <string_view>

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
    HPAINTBUFFER handle_{};
};

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept;
void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept;

} // namespace nfui

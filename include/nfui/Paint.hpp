#pragma once

#include <nfui/Theme.hpp>     // Color
#include <windows.h>

#include <string_view>

namespace nfui {

[[nodiscard]] Color lighten(Color c, float amount) noexcept;  // amount 0..1
[[nodiscard]] Color darken(Color c, float amount) noexcept;   // amount 0..1
[[nodiscard]] Color alpha_blend(Color src, Color dst, float alpha) noexcept; // alpha 0..1 (src over dst)

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept;
// draw_text passes text.data() directly to DrawTextW (no mutable copy). When
// the caller passes DT_END_ELLIPSIS or DT_MODIFYSTRING, the input text MUST be
// null-terminated; DrawTextW may write back into the buffer.
void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept;

} // namespace nfui

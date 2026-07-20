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
                  clamp_byte(mix(GetBValue(src.rgb), GetBValue(dst.rgb))))};
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

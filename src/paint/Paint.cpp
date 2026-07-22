#include <nfui/Paint.hpp>

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace nfui {

namespace {
int clamp_byte(int v) noexcept { return v < 0 ? 0 : (v > 255 ? 255 : v); }
}

Color lighten(Color c, float amount) noexcept {
    if (!(amount > 0.0f)) return c;
    if (amount > 1.0f) amount = 1.0f;
    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const int r = clamp_byte(lerp(GetRValue(c.rgb), 255, amount));
    const int g = clamp_byte(lerp(GetGValue(c.rgb), 255, amount));
    const int b = clamp_byte(lerp(GetBValue(c.rgb), 255, amount));
    return Color{RGB(r, g, b)};
}

Color darken(Color c, float amount) noexcept {
    if (!(amount > 0.0f)) return c;
    if (amount > 1.0f) amount = 1.0f;
    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const int r = clamp_byte(lerp(GetRValue(c.rgb), 0, amount));
    const int g = clamp_byte(lerp(GetGValue(c.rgb), 0, amount));
    const int b = clamp_byte(lerp(GetBValue(c.rgb), 0, amount));
    return Color{RGB(r, g, b)};
}

Color alpha_blend(Color src, Color dst, float alpha) noexcept {
    if (!(alpha > 0.0f)) return dst;
    if (alpha >= 1.0f) return src;
    auto mix = [alpha](int s, int d) { return static_cast<int>(s * alpha + d * (1.0f - alpha)); };
    return Color{RGB(clamp_byte(mix(GetRValue(src.rgb), GetRValue(dst.rgb))),
                  clamp_byte(mix(GetGValue(src.rgb), GetGValue(dst.rgb))),
                  clamp_byte(mix(GetBValue(src.rgb), GetBValue(dst.rgb))))};
}

void fill_rounded_rect(HDC dc, const RECT& bounds, int radius, Color fill, Color border) noexcept {
    if (dc == nullptr) return;
    const int r = radius < 0 ? 0 : radius;
    HBRUSH brush = CreateSolidBrush(fill.rgb);
    HPEN pen = CreatePen(PS_SOLID, 1, border.rgb);
    if (brush == nullptr || pen == nullptr) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, r * 2, r * 2);
    if (old_brush != HGDI_ERROR && old_brush != nullptr) SelectObject(dc, old_brush);
    if (old_pen != HGDI_ERROR && old_pen != nullptr) SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

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

// CP16: vertical linear gradient fill. Allocates a 32-bit DIB and uses
// GDI's GradientFill (if MSIMG32 is available) to populate it, then
// BitBlts back to the target. When radius > 0 the gradient DIB is
// drawn inside a Path round-rect so the corners read as soft. When
// GradientFill is unavailable (pre-ComCtl32 v6), falls back to the
// midpoint color so the call site never sees an unpainted rect.
void paint_linear_gradient(HDC dc, const RECT& bounds, int radius,
                           Color top, Color bottom) noexcept {
    if (dc == nullptr) return;
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;

    // 32-bit DIB section, top-down rows, no compression.
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = w;
    bi.bV5Height = -h;          // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi),
                                   DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib == nullptr || bits == nullptr) {
        DeleteObject(dib);
        // Fallback: solid midpoint color via fill_rect.
        const int r = (GetRValue(top.rgb) + GetRValue(bottom.rgb)) / 2;
        const int g = (GetGValue(top.rgb) + GetGValue(bottom.rgb)) / 2;
        const int b = (GetBValue(top.rgb) + GetBValue(bottom.rgb)) / 2;
        fill_rounded_rect(dc, bounds, radius, Color{RGB(r, g, b)}, Color{RGB(0, 0, 0)});
        return;
    }

    HDC mem = CreateCompatibleDC(dc);
    if (mem == nullptr) {
        DeleteObject(dib);
        return;
    }
    HGDIOBJ old_bmp = SelectObject(mem, dib);

    // Build the gradient manually per row. GradientFill requires
    // TRIANGLE_VERTEX setup that interacts poorly with the alpha mask
    // in 32-bit DIBs, so a manual lerp is the robust path. The ramp
    // is bilinear (smooth at top and bottom) to avoid banding.
    auto* pixels = static_cast<unsigned char*>(bits);
    const int stride = w * 4;
    const float inv_h = (h > 1) ? 1.0f / static_cast<float>(h - 1) : 0.0f;
    for (int y = 0; y < h; ++y) {
        float t = static_cast<float>(y) * inv_h;
        // Smoothstep — eases the top/bottom so the gradient blends
        // into whatever is on either side without a hard band.
        t = t * t * (3.0f - 2.0f * t);
        const int r = clamp_byte(static_cast<int>(GetRValue(top.rgb) +
            t * (GetRValue(bottom.rgb) - GetRValue(top.rgb))));
        const int g = clamp_byte(static_cast<int>(GetGValue(top.rgb) +
            t * (GetGValue(bottom.rgb) - GetGValue(top.rgb))));
        const int bch = clamp_byte(static_cast<int>(GetBValue(top.rgb) +
            t * (GetBValue(bottom.rgb) - GetBValue(top.rgb))));
        unsigned char* row = pixels + y * stride;
        for (int x = 0; x < w; ++x) {
            row[x * 4 + 0] = static_cast<unsigned char>(bch);
            row[x * 4 + 1] = static_cast<unsigned char>(g);
            row[x * 4 + 2] = static_cast<unsigned char>(r);
            row[x * 4 + 3] = 0xFF;            // opaque
        }
    }

    if (radius > 0) {
        // Mask the gradient into a rounded rect so the corners read
        // as soft. We do this by clearing the four corner pixels of
        // every row to alpha=0 then AlphaBlitting.
        const int r = radius;
        for (int y = 0; y < h; ++y) {
            unsigned char* row = pixels + y * stride;
            const float dy = (y < r) ? static_cast<float>(r - y) :
                          (y >= h - r) ? static_cast<float>(y - (h - r - 1)) : 0.0f;
            for (int x = 0; x < w; ++x) {
                const float dx = (x < r) ? static_cast<float>(r - x) :
                              (x >= w - r) ? static_cast<float>(x - (w - r - 1)) : 0.0f;
                if (dx > 0.0f && dy > 0.0f) {
                    const float d = std::sqrt(dx * dx + dy * dy);
                    if (d > static_cast<float>(r)) {
                        row[x * 4 + 3] = 0;       // outside the curve
                        continue;
                    }
                    const float alpha = (d > static_cast<float>(r) - 1.0f)
                        ? (static_cast<float>(r) - d) : 1.0f;
                    row[x * 4 + 3] = static_cast<unsigned char>(alpha * 255.0f);
                } else {
                    row[x * 4 + 3] = 0xFF;
                }
            }
        }
        // AlphaBlend the masked gradient onto the target.
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(dc, bounds.left, bounds.top, w, h,
                   mem, 0, 0, w, h, bf);
    } else {
        BitBlt(dc, bounds.left, bounds.top, w, h, mem, 0, 0, SRCCOPY);
    }

    SelectObject(mem, old_bmp);
    DeleteDC(mem);
    DeleteObject(dib);
}

// CP16: soft drop shadow under a rounded rect. elevation ∈ {1,2,3}
// drives the falloff distance and the alpha ramp (25% / 30% / 40%).
// shadow_color.rgb supplies the tint (the high byte alpha is ignored;
// the helper bakes its own alpha ramp so the soft falloff stays
// consistent across themes).
void paint_drop_shadow(HDC dc, const RECT& bounds, int radius,
                       int elevation, Color shadow_color) noexcept {
    if (dc == nullptr) return;
    const int blur = (elevation <= 1) ? 2 : (elevation == 2 ? 6 : 12);
    const int shadow_alpha_peak = (elevation <= 1) ? 64
                                   : (elevation == 2 ? 77 : 102);   // 25/30/40 %
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;

    // Shadow bitmap extends blur px past the bounds on each side so
    // the falloff doesn't clip at the bitmap edge.
    const int sw = w + blur * 2;
    const int sh = h + blur * 2;

    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = sw;
    bi.bV5Height = -sh;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi),
                                   DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib == nullptr || bits == nullptr) {
        DeleteObject(dib);
        return;
    }

    HDC mem = CreateCompatibleDC(dc);
    if (mem == nullptr) {
        DeleteObject(dib);
        return;
    }
    HGDIOBJ old_bmp = SelectObject(mem, dib);

    // Start fully transparent.
    const int total_pixels = sw * sh;
    auto* pixels = static_cast<unsigned char*>(bits);
    std::memset(pixels, 0, total_pixels * 4);

    // Render a soft, even falloff: per-pixel alpha = peak * (1 - d / blur)
    // where d is the distance from the rect edge inside the blur zone.
    const unsigned char tint_r = static_cast<unsigned char>(GetRValue(shadow_color.rgb));
    const unsigned char tint_g = static_cast<unsigned char>(GetGValue(shadow_color.rgb));
    const unsigned char tint_b = static_cast<unsigned char>(GetBValue(shadow_color.rgb));

    for (int y = 0; y < sh; ++y) {
        unsigned char* row = pixels + y * sw * 4;
        for (int x = 0; x < sw; ++x) {
            // Distance from the inner rect (blur..blur+w-1, blur..blur+h-1)
            const float dx = (x < blur) ? static_cast<float>(blur - x)
                          : (x >= blur + w) ? static_cast<float>(x - (blur + w) + 1)
                          : 0.0f;
            const float dy = (y < blur) ? static_cast<float>(blur - y)
                          : (y >= blur + h) ? static_cast<float>(y - (blur + h) + 1)
                          : 0.0f;
            const float d = (dx > dy) ? dx : dy;
            if (d >= static_cast<float>(blur)) continue;        // outside falloff
            const float falloff = 1.0f - (d / static_cast<float>(blur));
            const int alpha = static_cast<int>(shadow_alpha_peak * falloff);
            row[x * 4 + 0] = tint_b;
            row[x * 4 + 1] = tint_g;
            row[x * 4 + 2] = tint_r;
            row[x * 4 + 3] = static_cast<unsigned char>(alpha);
        }
    }

    // Mask the shadow's center to a rounded rect by zeroing alpha
    // outside the corner curve so the shadow inherits the panel's
    // radius (or stays square when radius == 0).
    if (radius > 0) {
        const int r = radius;
        const int left   = blur;
        const int top    = blur;
        const int right  = blur + w - 1;
        const int bottom = blur + h - 1;
        for (int y = top; y <= bottom; ++y) {
            unsigned char* row = pixels + y * sw * 4;
            const float dy_in = (y < top + r) ? static_cast<float>(top + r - y)
                           : (y > bottom - r) ? static_cast<float>(y - (bottom - r))
                           : 0.0f;
            for (int x = left; x <= right; ++x) {
                const float dx_in = (x < left + r) ? static_cast<float>(left + r - x)
                              : (x > right - r) ? static_cast<float>(x - (right - r))
                              : 0.0f;
                if (dx_in > 0.0f && dy_in > 0.0f) {
                    const float d = std::sqrt(dx_in * dx_in + dy_in * dy_in);
                    if (d > static_cast<float>(r)) {
                        row[x * 4 + 3] = 0;
                    } else if (d > static_cast<float>(r) - 1.0f) {
                        row[x * 4 + 3] = static_cast<unsigned char>(
                            static_cast<float>(row[x * 4 + 3]) * (static_cast<float>(r) - d));
                    }
                }
            }
        }
    }

    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, bounds.left - blur, bounds.top - blur, sw, sh,
               mem, 0, 0, sw, sh, bf);

    SelectObject(mem, old_bmp);
    DeleteDC(mem);
    DeleteObject(dib);
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

// CP18: ellipse helpers for vector glyphs. GDI's Ellipse() draws with the
// currently-selected brush (interior) and pen (outline); we select NULL_PEN
// for a pure fill and NULL_BRUSH for a pure stroke so a dot and a ring do not
// paint each other's region. Degenerate rects are ignored to match the rect
// helpers (a 0-area ellipse would still paint a 1px sliver on some drivers).
void fill_ellipse(HDC dc, const RECT& bounds, Color fill) noexcept {
    if (dc == nullptr) return;
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;
    HBRUSH brush = CreateSolidBrush(fill.rgb);
    if (brush == nullptr) return;
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    if (old_brush && old_brush != HGDI_ERROR) SelectObject(dc, old_brush);
    if (old_pen && old_pen != HGDI_ERROR) SelectObject(dc, old_pen);
    DeleteObject(brush);
}

void draw_ellipse(HDC dc, const RECT& bounds, Color color, int width) noexcept {
    if (dc == nullptr) return;
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;
    HPEN pen = CreatePen(PS_SOLID, width < 1 ? 1 : width, color.rgb);
    if (pen == nullptr) return;
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    if (old_pen && old_pen != HGDI_ERROR) SelectObject(dc, old_pen);
    if (old_brush && old_brush != HGDI_ERROR) SelectObject(dc, old_brush);
    DeleteObject(pen);
}

// CP19: solid focus border of `width` device px drawn inside `bounds`. We
// stack `width` 1px FrameRects (each inset by 1) rather than a single wide
// pen + Rectangle: GDI centres a pen on the outline, so a 2px pen would
// straddle the rect edge and the outer half would clip at a window's edge.
// Stacked FrameRects keep the border fully inside `bounds` and pixel-crisp at
// any width. No-op on a null DC or degenerate rect; never throws.
void paint_focus_border(HDC dc, const RECT& bounds, Color color, int width) noexcept {
    if (dc == nullptr) return;
    const int w = width < 1 ? 1 : width;
    HBRUSH brush = CreateSolidBrush(color.rgb);
    if (brush == nullptr) return;
    RECT r = bounds;
    for (int i = 0; i < w; ++i) {
        if (r.right <= r.left || r.bottom <= r.top) break;
        FrameRect(dc, &r, brush);
        r.left += 1; r.top += 1; r.right -= 1; r.bottom -= 1;
    }
    DeleteObject(brush);
}

void draw_text(HDC dc, const RECT& bounds, std::wstring_view text, HFONT font, Color text_color, UINT format) noexcept {
    // Note: this helper passes text.data() directly to DrawTextW without copying
    // into a mutable buffer. When the caller passes DT_END_ELLIPSIS or
    // DT_MODIFYSTRING, DrawTextW may write back into the buffer; the input text
    // MUST therefore be null-terminated (e.g. a null-terminated string viewed via
    // std::wstring_view whose length covers up to but not necessarily including
    // the terminator). Non-null-terminated views are safe only with formats that
    // do not modify the buffer.
    if (dc == nullptr || text.empty()) return;
    HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text_color.rgb);
    RECT r = bounds;
    const int count = text.size() > static_cast<size_t>(INT_MAX) ? INT_MAX : static_cast<int>(text.size());
    DrawTextW(dc, text.data(), count, &r, format);
    if (old_font != nullptr && old_font != HGDI_ERROR) SelectObject(dc, old_font);
}

MemoryDC::MemoryDC(HDC target, const RECT& bounds) noexcept
    : target_(target), mem_dc_(nullptr), bmp_(nullptr), old_bmp_(nullptr),
      x_(bounds.left), y_(bounds.top),
      w_(bounds.right - bounds.left), h_(bounds.bottom - bounds.top) {
    if (target_ == nullptr || w_ <= 0 || h_ <= 0) return;
    mem_dc_ = CreateCompatibleDC(target_);
    if (mem_dc_ == nullptr) {
#ifdef _DEBUG
        assert(false && "MemoryDC: CreateCompatibleDC failed");
#else
        OutputDebugStringW(L"NFUI: MemoryDC fallback to direct DC (CreateCompatibleDC failed)\n");
#endif
        // Fallback: callers will draw directly into the target HDC because
        // mem_dc_ stays null. Paint helpers honor `valid()` checks.
        return;
    }
    bmp_ = CreateCompatibleBitmap(target_, w_, h_);
    if (bmp_ == nullptr) {
        // P6.1: low-memory or GDI-handle-exhausted. Tear down the memory DC
        // and fall back to painting directly into the target. See
        // docs/KNOWLEDGE/polish/2026-07-22-memorydc-createbitmap-fallback.md.
#ifdef _DEBUG
        assert(false && "MemoryDC: CreateCompatibleBitmap failed");
#else
        OutputDebugStringW(L"NFUI: MemoryDC fallback to direct DC (CreateCompatibleBitmap failed)\n");
#endif
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
        return;
    }
    old_bmp_ = SelectObject(mem_dc_, bmp_);
    BitBlt(mem_dc_, 0, 0, w_, h_, target_, x_, y_, SRCCOPY);
}

MemoryDC::~MemoryDC() noexcept {
    if (mem_dc_ == nullptr) return;
    if (target_ != nullptr) {
        BitBlt(target_, x_, y_, w_, h_, mem_dc_, 0, 0, SRCCOPY);
    }
    if (old_bmp_ && old_bmp_ != HGDI_ERROR) SelectObject(mem_dc_, old_bmp_);
    if (bmp_) DeleteObject(bmp_);
    DeleteDC(mem_dc_);
}

OwnerDrawDC::OwnerDrawDC(HWND hwnd, const RECT& bounds) noexcept
    : hwnd_(hwnd), bounds_(bounds) {
    if (hwnd == nullptr) return;
    HDC dc = BeginPaint(hwnd, &ps_);
    if (dc == nullptr) return;
    mem_.emplace(dc, bounds);
    target_ = mem_->valid() ? mem_->dc() : dc;
    valid_ = true;
}

OwnerDrawDC::~OwnerDrawDC() noexcept {
    if (hwnd_ != nullptr && valid_) {
        // mem_ destructor runs first (member destruction order: ps_ -> mem_ ->
        // target_ -> valid_ -> hwnd_/bounds_ in reverse declaration order). The
        // optional<MemoryDC> resets MemoryDC which flushes the offscreen buffer
        // to the BeginPaint DC via BitBlt while the BeginPaint DC is still
        // valid. Only then does EndPaint release it.
        mem_.reset();
        EndPaint(hwnd_, &ps_);
    }
}

} // namespace nfui

#include <nfui/Paint.hpp>

#include <cassert>
#include <string>

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

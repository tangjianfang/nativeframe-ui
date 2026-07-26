// Implementation of the AA-aware chart paint primitives. GdiplusContext's
// method bodies live in their own translation unit (internal/GdiplusContext.cpp)
// inside nfui_charts_aa, so we don't need the old TU-folding include here any
// more — that was a workaround for when both files shipped as one TU inside
// nfui_charts. The header still declares GdiplusContext with inline static
// storage so the token / ref-count remain shared across the program.

#include "internal/ChartsPaint.hpp"
#include "internal/GdiplusContext.hpp"

#include <nfui/Paint.hpp>

// gdiplus.h requires COM interface declarations (IStream, HRESULT, ...)
// which WIN32_LEAN_AND_MEAN strips from <windows.h>. Include <objbase.h>
// first so the metafile + image headers compile cleanly.
#include <objbase.h>
#include <gdiplus.h>

#include <vector>

namespace nfui::charts_internal {

namespace {

// Cheap helper to translate nfui::Color (COLORREF) -> Gdiplus::Color with
// full opacity. Sits in an unnamed namespace so it doesn't leak.
inline Gdiplus::Color to_gp_color(Color c) noexcept {
    return Gdiplus::Color(255,
                          GetRValue(c.rgb),
                          GetGValue(c.rgb),
                          GetBValue(c.rgb));
}

// CP40: Win32 POINT is `LONG[2]` and Gdiplus::PointF is `REAL[2]` (float).
// Both occupy 8 bytes but their bit representation differs — feeding POINT
// to DrawLines / DrawBeziers via reinterpret_cast (the CP28-era code) made
// GDI+ draw to garbage coordinates and silently dropped the stroke. We
// must allocate a buffer and copy each coordinate through a real numeric
// conversion. The buffer is reserved once per draw call; the cost is
// negligible vs. the per-pixel antialias work.
template <typename SourcePoint>
std::vector<Gdiplus::PointF> to_point_f_buffer(const SourcePoint* src,
                                               int count) noexcept {
    std::vector<Gdiplus::PointF> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.emplace_back(static_cast<Gdiplus::REAL>(src[i].x),
                         static_cast<Gdiplus::REAL>(src[i].y));
    }
    return out;
}

} // namespace

void draw_polyline_aa(HDC hdc,
                      const POINT* points,
                      int count,
                      Color color,
                      int width_px) noexcept {
    if (hdc == nullptr || points == nullptr || count < 2) return;
    const int width = width_px < 1 ? 1 : width_px;

    // CP40: correct the POINT -> PointF conversion. The prior code
    // reinterpret_cast was silently broken because POINT's bit layout is
    // LONG[2] while PointF is REAL[2]. We try GDI+ first for a smooth
    // AA stroke; if GDI+ is inactive, allocation fails, or the draw call
    // returns a non-Ok status, fall back to the GDI polyline so the chart
    // still renders on every DC (including the visual-audit PrintWindow
    // path that has historically broken GDI+).
    if (GdiplusContext::active()) {
        Gdiplus::Graphics graphics(hdc);
        if (graphics.GetLastStatus() == Gdiplus::Status::Ok) {
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            // PixelOffsetModeHalf nudges the stroke half a pixel so
            // integer-aligned lines don't ghost between two rows of
            // device pixels at integer scales.
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::Pen pen(to_gp_color(color),
                             static_cast<Gdiplus::REAL>(width));
            if (pen.GetLastStatus() == Gdiplus::Status::Ok) {
                const auto buf = to_point_f_buffer(points, count);
                if (graphics.DrawLines(&pen, buf.data(), count) ==
                    Gdiplus::Status::Ok) {
                    return;  // AA succeeded — skip the GDI overdraw.
                }
            }
        }
    }

    // Fallback for inactive GDI+ or any failure above.
    draw_polyline(hdc, points, count, color, width);
}

void draw_beziers_aa(HDC hdc,
                     const POINT* points,
                     int count,
                     Color color,
                     int width_px) noexcept {
    if (hdc == nullptr || points == nullptr || count < 4) return;
    // PolyBezier / DrawBeziers require (count - 1) % 3 == 0 — i.e.
    // chained cubic Bezier segments with shared endpoints, matching the
    // contract of catmull_rom_to_bezier (1 + 3*(n-1) points).
    if (((count - 1) % 3) != 0) return;
    const int width = width_px < 1 ? 1 : width_px;

    // CP40: same conversion fix as draw_polyline_aa. We additionally skip
    // the GDI overdraw when GDI+ succeeds, so windowed renders stay smooth
    // instead of being doubled on top of the AA stroke.
    if (GdiplusContext::active()) {
        Gdiplus::Graphics graphics(hdc);
        if (graphics.GetLastStatus() == Gdiplus::Status::Ok) {
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::Pen pen(to_gp_color(color),
                             static_cast<Gdiplus::REAL>(width));
            if (pen.GetLastStatus() == Gdiplus::Status::Ok) {
                const auto buf = to_point_f_buffer(points, count);
                if (graphics.DrawBeziers(&pen, buf.data(), count) ==
                    Gdiplus::Status::Ok) {
                    return;  // AA succeeded — skip the GDI overdraw.
                }
            }
        }
    }

    // Reliable GDI fallback path.
    HPEN pen = CreatePen(PS_SOLID, width, color.rgb);
    if (pen == nullptr) return;
    HGDIOBJ prev_pen = SelectObject(hdc, pen);
    HGDIOBJ prev_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    PolyBezier(hdc, points, count);
    SelectObject(hdc, prev_brush);
    SelectObject(hdc, prev_pen);
    DeleteObject(pen);
}

void fill_circles_aa(HDC hdc,
                     const POINT* centers,
                     int count,
                     int radius_px,
                     Color color) noexcept {
    if (hdc == nullptr || centers == nullptr || count <= 0 || radius_px <= 0) return;
    const LONG r = static_cast<LONG>(radius_px);

    // CP40: try GDI+ for an AA-soft fill. Skip the GDI overdraw when GDI+
    // succeeds so windowed markers stay smooth instead of being doubled on
    // top of the AA fill. Coordinates use the same numeric conversion as
    // draw_polyline_aa (the loop body inlined the conversion already).
    if (GdiplusContext::active()) {
        Gdiplus::Graphics graphics(hdc);
        if (graphics.GetLastStatus() == Gdiplus::Status::Ok) {
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::SolidBrush brush(to_gp_color(color));
            if (brush.GetLastStatus() == Gdiplus::Status::Ok) {
                const Gdiplus::REAL gr =
                    static_cast<Gdiplus::REAL>(radius_px);
                bool all_ok = true;
                for (int i = 0; i < count; ++i) {
                    const POINT& p = centers[i];
                    if (graphics.FillEllipse(
                            &brush,
                            static_cast<Gdiplus::REAL>(p.x) - gr,
                            static_cast<Gdiplus::REAL>(p.y) - gr,
                            gr * 2.0f,
                            gr * 2.0f) != Gdiplus::Status::Ok) {
                        all_ok = false;
                        break;
                    }
                }
                if (all_ok) return;
            }
        }
    }

    // Reliable GDI fallback path — guarantees marker visibility on every
    // DC the chart views paint to.
    HBRUSH brush = CreateSolidBrush(color.rgb);
    if (brush == nullptr) return;
    HGDIOBJ prev_brush = SelectObject(hdc, brush);
    HGDIOBJ prev_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int i = 0; i < count; ++i) {
        const POINT& p = centers[i];
        Ellipse(hdc, p.x - r, p.y - r, p.x + r, p.y + r);
    }
    SelectObject(hdc, prev_pen);
    SelectObject(hdc, prev_brush);
    DeleteObject(brush);
}

} // namespace nfui::charts_internal

// Public antialiasing entry points. Live in nfui_charts_aa rather than
// nfui_charts so the GDI+ runtime dependency stays out of consumers that
// only want the GDI fallback path. Declared in <nfui/Charts.hpp> alongside
// the rest of the nfui public surface.
namespace nfui {

bool initialize_chart_aa() noexcept {
    return charts_internal::GdiplusContext::initialize();
}

void shutdown_chart_aa() noexcept {
    charts_internal::GdiplusContext::shutdown();
}

} // namespace nfui

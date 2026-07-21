// Implementation of the AA-aware chart paint primitives. TU-folded so the
// GdiplusContext translation unit links cleanly into nfui_charts: when this
// file is built, ChartsPaint.cpp gets its own object that already references
// GdiplusContext's static members. We #include the .cpp at the top instead
// of the .hpp because GdiplusContext owns the inline static storage; touching
// it from a separate TU would require separate storage definitions.

#include "internal/GdiplusContext.cpp"
#include "internal/ChartsPaint.hpp"

#include <nfui/Paint.hpp>

// gdiplus.h requires COM interface declarations (IStream, HRESULT, ...)
// which WIN32_LEAN_AND_MEAN strips from <windows.h>. Include <objbase.h>
// first so the metafile + image headers compile cleanly.
#include <objbase.h>
#include <gdiplus.h>

#include <memory>

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

} // namespace

void draw_polyline_aa(HDC hdc,
                      const POINT* points,
                      int count,
                      Color color,
                      int width_px) noexcept {
    if (hdc == nullptr || points == nullptr || count < 2) return;
    if (!GdiplusContext::active()) {
        // No GDI+ available — degrade to the pure GDI polyline so callers
        // still get a visible stroke even before nfui::initialize_chart_aa()
        // has been called (or if startup ever fails).
        draw_polyline(hdc, points, count, color, width_px < 1 ? 1 : width_px);
        return;
    }

    std::unique_ptr<Gdiplus::Graphics> graphics(
        new Gdiplus::Graphics(hdc));
    if (graphics->GetLastStatus() != Gdiplus::Status::Ok) return;

    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    // PixelOffsetModeHalf nudges the stroke half a pixel so integer-aligned
    // lines don't ghost between two rows of device pixels at integer scales.
    graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Pen pen(to_gp_color(color), static_cast<Gdiplus::REAL>(width_px < 1 ? 1 : width_px));
    if (pen.GetLastStatus() != Gdiplus::Status::Ok) return;

    graphics->DrawLines(&pen,
                        reinterpret_cast<const Gdiplus::PointF*>(points),
                        count);
}

void draw_beziers_aa(HDC hdc,
                     const POINT* points,
                     int count,
                     Color color,
                     int width_px) noexcept {
    if (hdc == nullptr || points == nullptr || count < 4) return;
    if (!GdiplusContext::active()) {
        // GDI fallback: PolyBezier against a Solid pen. PolyBezier requires
        // (count - 1) % 3 == 0, i.e. chained cubic Bezier segments with
        // shared endpoints — the same shape catmull_rom_to_bezier emits.
        HPEN pen = CreatePen(PS_SOLID, width_px < 1 ? 1 : width_px, color.rgb);
        if (pen == nullptr) return;
        HGDIOBJ prev_pen = SelectObject(hdc, pen);
        HGDIOBJ prev_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        PolyBezier(hdc, points, count);
        SelectObject(hdc, prev_brush);
        SelectObject(hdc, prev_pen);
        DeleteObject(pen);
        return;
    }

    std::unique_ptr<Gdiplus::Graphics> graphics(
        new Gdiplus::Graphics(hdc));
    if (graphics->GetLastStatus() != Gdiplus::Status::Ok) return;

    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Pen pen(to_gp_color(color), static_cast<Gdiplus::REAL>(width_px < 1 ? 1 : width_px));
    if (pen.GetLastStatus() != Gdiplus::Status::Ok) return;

    // Gdiplus::Graphics::DrawBeziers treats each block of 4 points as one
    // cubic segment and chains them by sharing endpoints, exactly matching
    // the contract of catmull_rom_to_bezier (4*(n-1) points).
    graphics->DrawBeziers(&pen,
                          reinterpret_cast<const Gdiplus::PointF*>(points),
                          count);
}

void fill_circles_aa(HDC hdc,
                     const POINT* centers,
                     int count,
                     int radius_px,
                     Color color) noexcept {
    if (hdc == nullptr || centers == nullptr || count <= 0 || radius_px <= 0) return;
    if (!GdiplusContext::active()) {
        // GDI fallback: ellipse loop with a Solid brush and null pen so the
        // marker reads as a clean filled circle rather than a ring.
        HBRUSH brush = CreateSolidBrush(color.rgb);
        if (brush == nullptr) return;
        HGDIOBJ prev_brush = SelectObject(hdc, brush);
        HGDIOBJ prev_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
        const LONG r = static_cast<LONG>(radius_px);
        for (int i = 0; i < count; ++i) {
            const POINT& p = centers[i];
            Ellipse(hdc, p.x - r, p.y - r, p.x + r, p.y + r);
        }
        SelectObject(hdc, prev_pen);
        SelectObject(hdc, prev_brush);
        DeleteObject(brush);
        return;
    }

    std::unique_ptr<Gdiplus::Graphics> graphics(
        new Gdiplus::Graphics(hdc));
    if (graphics->GetLastStatus() != Gdiplus::Status::Ok) return;

    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::SolidBrush brush(to_gp_color(color));
    if (brush.GetLastStatus() != Gdiplus::Status::Ok) return;

    const Gdiplus::REAL r = static_cast<Gdiplus::REAL>(radius_px);
    for (int i = 0; i < count; ++i) {
        const POINT& p = centers[i];
        graphics->FillEllipse(&brush,
                              static_cast<Gdiplus::REAL>(p.x) - r,
                              static_cast<Gdiplus::REAL>(p.y) - r,
                              r * 2.0f,
                              r * 2.0f);
    }
}

} // namespace nfui::charts_internal

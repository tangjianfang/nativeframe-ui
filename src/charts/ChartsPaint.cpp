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
    const int width = width_px < 1 ? 1 : width_px;

    // CP28: Gdiplus::Graphics::DrawLines can bind to the visual-audit
    // PrintWindow memory DC and report Status::Ok yet silently produce
    // no pixels (same failure mode as DrawBeziers). We always overdraw
    // with the GDI polyline so the line is visible on every DC, including
    // the 32bpp memory DCs the visual-audit PrintWindow path produces.
    // CP28+: revisit once GDI+ is confirmed to render reliably on PrintWindow
    // memory DCs; then skip the GDI overdraw when GDI+ reports Status::Ok so
    // windowed renders stay smooth without doubling the stroke.
    if (GdiplusContext::active()) {
        std::unique_ptr<Gdiplus::Graphics> graphics(
            new Gdiplus::Graphics(hdc));
        if (graphics->GetLastStatus() == Gdiplus::Status::Ok) {
            graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            // PixelOffsetModeHalf nudges the stroke half a pixel so
            // integer-aligned lines don't ghost between two rows of
            // device pixels at integer scales.
            graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::Pen pen(to_gp_color(color),
                             static_cast<Gdiplus::REAL>(width));
            if (pen.GetLastStatus() == Gdiplus::Status::Ok) {
                graphics->DrawLines(
                    &pen,
                    reinterpret_cast<const Gdiplus::PointF*>(points),
                    count);
            }
        }
    }

    // Reliable GDI overdraw — guarantees a visible polyline on every DC
    // the chart views can paint to, including the 32bpp memory DCs the
    // visual-audit PrintWindow path produces.
    draw_polyline(hdc, points, count, color, width);
}

void draw_beziers_aa(HDC hdc,
                     const POINT* points,
                     int count,
                     Color color,
                     int width_px) noexcept {
    if (hdc == nullptr || points == nullptr || count < 4) return;
    const int width = width_px < 1 ? 1 : width_px;

    // CP28: Gdiplus::Graphics::DrawBeziers binds to the visual-audit
    // PrintWindow memory DC and reports Status::Ok, yet silently produces
    // no pixels — the empty-spline defect CP27 caught. GDI+ is still
    // attempted first for native HWND owners that want a smooth AA stroke,
    // but its result is always overdrawn with the GDI path so PrintWindow
    // captures and windowed renders converge on the same visible stroke.
    // CP28+: revisit once GDI+ is confirmed to paint reliably on PrintWindow
    // memory DCs; then skip the GDI overdraw when GDI+ reports Status::Ok so
    // windowed spline renders stay smooth without doubling the stroke.
    if (GdiplusContext::active()) {
        std::unique_ptr<Gdiplus::Graphics> graphics(
            new Gdiplus::Graphics(hdc));
        if (graphics->GetLastStatus() == Gdiplus::Status::Ok) {
            graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::Pen pen(to_gp_color(color),
                             static_cast<Gdiplus::REAL>(width));
            if (pen.GetLastStatus() == Gdiplus::Status::Ok) {
                graphics->DrawBeziers(
                    &pen,
                    reinterpret_cast<const Gdiplus::PointF*>(points),
                    count);
            }
        }
    }

    // Reliable GDI path — overdraws the GDI+ attempt so the spline is
    // visible on every DC the chart views can render to.
    HPEN pen = CreatePen(PS_SOLID, width, color.rgb);
    if (pen == nullptr) return;
    HGDIOBJ prev_pen = SelectObject(hdc, pen);
    HGDIOBJ prev_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    // PolyBezier requires (count - 1) % 3 == 0 — i.e. chained cubic
    // Bezier segments with shared endpoints, matching the contract of
    // catmull_rom_to_bezier (1 + 3*(n-1) points).
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

    // CP28: Gdiplus::Graphics::FillEllipse silently drops pixels on the
    // visual-audit PrintWindow memory DC (same root cause as DrawBeziers
    // / DrawLines). Try GDI+ first for an AA-soft fill, then always
    // overdraw with a GDI Ellipse loop so the marker dots are visible on
    // every DC the chart views paint to.
    // CP28+: revisit once GDI+ is confirmed to paint reliably on PrintWindow
    // memory DCs; then skip the GDI overdraw when GDI+ reports Status::Ok so
    // windowed marker dots stay smooth without doubling the fill.
    if (GdiplusContext::active()) {
        std::unique_ptr<Gdiplus::Graphics> graphics(
            new Gdiplus::Graphics(hdc));
        if (graphics->GetLastStatus() == Gdiplus::Status::Ok) {
            graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            Gdiplus::SolidBrush brush(to_gp_color(color));
            if (brush.GetLastStatus() == Gdiplus::Status::Ok) {
                const Gdiplus::REAL gr =
                    static_cast<Gdiplus::REAL>(radius_px);
                for (int i = 0; i < count; ++i) {
                    const POINT& p = centers[i];
                    graphics->FillEllipse(
                        &brush,
                        static_cast<Gdiplus::REAL>(p.x) - gr,
                        static_cast<Gdiplus::REAL>(p.y) - gr,
                        gr * 2.0f,
                        gr * 2.0f);
                }
            }
        }
    }

    // Reliable GDI overdraw — guarantees the line-chart marker dots are
    // visible on the 32bpp memory DC the visual-audit path captures.
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

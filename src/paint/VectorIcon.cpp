#include <nfui/VectorIcon.hpp>

#include <nfui/Paint.hpp>

namespace nfui {

namespace {

constexpr int kGrid = 16;   // glyphs are authored on a 16x16 logical grid

// Map a logical-grid point into the device rect. `ox/oy` is the centred
// grid origin and `scale` the uniform device-per-logical factor.
inline POINT map_pt(int lx, int ly, int ox, int oy, float scale) noexcept {
    return POINT{ static_cast<LONG>(ox + static_cast<float>(lx) * scale),
                  static_cast<LONG>(oy + static_cast<float>(ly) * scale) };
}

// Centre of a logical circle defined by its bounding box (lx0,ly0)-(lx1,ly1)
// and its device-pixel size, returned as a RECT for fill_ellipse/draw_ellipse.
inline RECT map_ellipse(int lx0, int ly0, int lx1, int ly1,
                        int ox, int oy, float scale) noexcept {
    const int left   = static_cast<int>(ox + static_cast<float>(lx0) * scale);
    const int top    = static_cast<int>(oy + static_cast<float>(ly0) * scale);
    const int right  = static_cast<int>(ox + static_cast<float>(lx1) * scale);
    const int bottom = static_cast<int>(oy + static_cast<float>(ly1) * scale);
    return RECT{ left, top, right, bottom };
}

} // namespace

void draw_vector_icon(HDC dc, IconKind kind, const RECT& bounds,
                      Color color, int stroke_width) noexcept {
    if (dc == nullptr || kind == IconKind::none) return;
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;

    // Largest aspect-preserving square, centred. Glyphs are designed on a
    // square grid so a non-square cell letterboxes rather than stretches.
    const int side = (w < h) ? w : h;
    const float scale = static_cast<float>(side) / static_cast<float>(kGrid);
    const int ox = bounds.left + (w - side) / 2;
    const int oy = bounds.top + (h - side) / 2;
    const int sw = stroke_width < 1 ? 1 : stroke_width;

    switch (kind) {
    case IconKind::chevron_down: {
        const POINT tri[3] = {
            map_pt(3, 5, ox, oy, scale), map_pt(13, 5, ox, oy, scale),
            map_pt(8, 12, ox, oy, scale),
        };
        fill_polygon(dc, tri, 3, color, color);
        break;
    }
    case IconKind::chevron_up: {
        const POINT tri[3] = {
            map_pt(3, 11, ox, oy, scale), map_pt(13, 11, ox, oy, scale),
            map_pt(8, 4, ox, oy, scale),
        };
        fill_polygon(dc, tri, 3, color, color);
        break;
    }
    case IconKind::chevron_right: {
        const POINT tri[3] = {
            map_pt(5, 3, ox, oy, scale), map_pt(5, 13, ox, oy, scale),
            map_pt(12, 8, ox, oy, scale),
        };
        fill_polygon(dc, tri, 3, color, color);
        break;
    }
    case IconKind::chevron_left: {
        const POINT tri[3] = {
            map_pt(11, 3, ox, oy, scale), map_pt(11, 13, ox, oy, scale),
            map_pt(4, 8, ox, oy, scale),
        };
        fill_polygon(dc, tri, 3, color, color);
        break;
    }
    case IconKind::check: {
        const POINT pts[3] = {
            map_pt(3, 8, ox, oy, scale), map_pt(7, 12, ox, oy, scale),
            map_pt(13, 4, ox, oy, scale),
        };
        draw_polyline(dc, pts, 3, color, sw);
        break;
    }
    case IconKind::close: {
        draw_line(dc, map_pt(4, 4, ox, oy, scale), map_pt(12, 12, ox, oy, scale), color, sw);
        draw_line(dc, map_pt(12, 4, ox, oy, scale), map_pt(4, 12, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::plus: {
        draw_line(dc, map_pt(8, 3, ox, oy, scale), map_pt(8, 13, ox, oy, scale), color, sw);
        draw_line(dc, map_pt(3, 8, ox, oy, scale), map_pt(13, 8, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::minus: {
        draw_line(dc, map_pt(3, 8, ox, oy, scale), map_pt(13, 8, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::search: {
        // Ring (stroke) + diagonal handle.
        draw_ellipse(dc, map_ellipse(3, 3, 11, 11, ox, oy, scale), color, sw);
        draw_line(dc, map_pt(10, 10, ox, oy, scale), map_pt(14, 14, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::gear: {
        // "Sliders" settings glyph: three rails with knobs.
        draw_line(dc, map_pt(2, 5, ox, oy, scale),  map_pt(14, 5, ox, oy, scale),  color, sw);
        draw_line(dc, map_pt(2, 8, ox, oy, scale),  map_pt(14, 8, ox, oy, scale),  color, sw);
        draw_line(dc, map_pt(2, 11, ox, oy, scale), map_pt(14, 11, ox, oy, scale), color, sw);
        fill_ellipse(dc, map_ellipse(10, 4, 12, 6, ox, oy, scale), color);   // knob rail 1
        fill_ellipse(dc, map_ellipse(4, 7, 6, 9, ox, oy, scale),   color);   // knob rail 2
        fill_ellipse(dc, map_ellipse(8, 10, 10, 12, ox, oy, scale), color);  // knob rail 3
        break;
    }
    case IconKind::info: {
        // Ring + dot + stem of the "i".
        draw_ellipse(dc, map_ellipse(2, 2, 14, 14, ox, oy, scale), color, sw);
        fill_ellipse(dc, map_ellipse(7, 4, 9, 6, ox, oy, scale), color);    // dot
        draw_line(dc, map_pt(8, 7, ox, oy, scale), map_pt(8, 12, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::warning: {
        // Triangle outline (closed polyline) + exclamation line + dot.
        const POINT tri[4] = {
            map_pt(8, 2, ox, oy, scale), map_pt(15, 14, ox, oy, scale),
            map_pt(1, 14, ox, oy, scale), map_pt(8, 2, ox, oy, scale),
        };
        draw_polyline(dc, tri, 4, color, sw);
        draw_line(dc, map_pt(8, 6, ox, oy, scale), map_pt(8, 10, ox, oy, scale), color, sw);
        fill_ellipse(dc, map_ellipse(7, 11, 9, 13, ox, oy, scale), color);  // exclamation dot
        break;
    }
    case IconKind::dot: {
        fill_ellipse(dc, map_ellipse(5, 5, 11, 11, ox, oy, scale), color);
        break;
    }
    case IconKind::hamburger: {
        draw_line(dc, map_pt(3, 4, ox, oy, scale),  map_pt(13, 4, ox, oy, scale),  color, sw);
        draw_line(dc, map_pt(3, 8, ox, oy, scale),  map_pt(13, 8, ox, oy, scale),  color, sw);
        draw_line(dc, map_pt(3, 12, ox, oy, scale), map_pt(13, 12, ox, oy, scale), color, sw);
        break;
    }
    case IconKind::none:
        break;
    }
}

} // namespace nfui
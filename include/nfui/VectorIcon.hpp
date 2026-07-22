#pragma once

// CP18: lightweight vector icon system. Common UI glyphs drawn as GDI
// primitives (Polygon / Polyline / Line / Ellipse) — NOT a TrueType icon
// font and NOT GDI+. Glyphs are defined on a 16x16 logical grid and scaled
// into the target rect at draw time, so they stay crisp at any DPI and any
// cell size. Colour is caller-resolved from the ThemePalette (see the role
// table in docs/KNOWLEDGE/polish/2026-07-23-cp18-vector-icons.md), matching
// the existing paint-helper convention (callers resolve palette → Color).
//
// Layering: lives in the paint module (paint may depend on theme), so it can
// sit alongside fill_polygon / draw_polyline which it composes. Controls get
// it transitively via Paint.hpp.

#include <nfui/Theme.hpp>     // Color
#include <windows.h>

namespace nfui {

enum class IconKind {
    none,
    chevron_down,    // filled triangle — ComboBox dropdown, expanded state
    chevron_up,
    chevron_left,
    chevron_right,
    check,           // stroke — checked indicator, confirm
    close,           // stroke X — dismiss / close
    plus,            // stroke — add
    minus,           // stroke — remove / collapse
    search,          // ring + handle — magnifier
    gear,            // sliders — settings
    info,            // ring + i — informational
    warning,         // triangle + ! — caution
    dot,             // filled disc — status / bullet
    hamburger,       // three lines — menu
};

// Draw `kind` centred inside `bounds`, scaled from its 16x16 logical grid to
// the largest aspect-preserving square that fits (then centred). `color` is
// the glyph colour (caller-resolved from the palette); `stroke_width` is in
// device pixels (caller scales via DpiScale::logical_to_pixels). Filled
// glyphs (chevrons, dot) ignore stroke_width. A degenerate rect or
// IconKind::none is a no-op. Never throws.
void draw_vector_icon(HDC dc, IconKind kind, const RECT& bounds,
                      Color color, int stroke_width) noexcept;

} // namespace nfui
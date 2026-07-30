// CP-B3: ComponentGallery interaction-state matrix.
//
// The gallery's job is to answer "what does control X look like in state Y,
// in theme Z?". Live HWNDs can only ever show ONE state at a time — you
// cannot screenshot a hovered, a pressed, and a focused ComboBox
// simultaneously — so the audit's "no interaction-state matrix" finding
// cannot be closed with real controls alone.
//
// This header paints compact *thumbnails* of each control class in each
// ControlState, using exactly the same `nfui::state_palette()` resolver that
// the self-painted CP-A2/A3 controls consume. The matrix is therefore not a
// hand-drawn mock: every fill / border / ink colour in a cell comes from the
// framework's own state resolver, so a regression in the resolver shows up
// here immediately, in all three themes.
//
// Header-only + inline so the sample stays a single translation unit (same
// pattern as DialogTour's TourStage.hpp).
//
// UI thread only. Every entry point is noexcept — these run inside WM_PAINT.

#pragma once

#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <windows.h>

namespace gallery {

// The seven control classes the audit called out as leaking native grey
// chrome. Each gets one matrix row.
enum class MatrixControl {
    check_box,
    radio_button,
    edit,
    combo_box,
    list_view,
    tree_view,
    tab_control,
};

inline constexpr nfui::ControlState matrix_states[] = {
    nfui::ControlState::rest,
    nfui::ControlState::hover,
    nfui::ControlState::pressed,
    nfui::ControlState::focused,
    nfui::ControlState::disabled,
    nfui::ControlState::error,
};

// Short lower-case column headers. `rest` (not `default`) mirrors the
// enumerator name — `default` is a C++ keyword and cannot be an enumerator.
inline constexpr const wchar_t* matrix_state_labels[] = {
    L"rest", L"hover", L"pressed", L"focus", L"disabled", L"error",
};

inline constexpr MatrixControl matrix_controls[] = {
    MatrixControl::check_box,
    MatrixControl::radio_button,
    MatrixControl::edit,
    MatrixControl::combo_box,
    MatrixControl::list_view,
    MatrixControl::tree_view,
    MatrixControl::tab_control,
};

inline constexpr const wchar_t* matrix_control_labels[] = {
    L"CheckBox",
    L"RadioButton",
    L"Edit",
    L"ComboBox",
    L"ListView",
    L"TreeView",
    L"TabControl",
};

inline constexpr std::size_t matrix_state_count   = sizeof(matrix_states) / sizeof(matrix_states[0]);
inline constexpr std::size_t matrix_control_count = sizeof(matrix_controls) / sizeof(matrix_controls[0]);

namespace detail {
// RECT member subtraction yields `long`; int constants in the matrix code
// need to round-trip through this helper so std::min / std::max can resolve
// a common overload (std::min<long> / std::min<int> are distinct templates
// and the compiler refuses to pick one for a mixed-type call).
[[nodiscard]] inline int imin(long a, int b) noexcept {
    return static_cast<int>(a < b ? a : b);
}
[[nodiscard]] inline int imax(long a, int b) noexcept {
    return static_cast<int>(a > b ? a : b);
}
[[nodiscard]] inline int imin(int a, int b) noexcept { return a < b ? a : b; }
[[nodiscard]] inline int imax(int a, int b) noexcept { return a > b ? a : b; }

[[nodiscard]] inline RECT inset_rect(const RECT& r, int dx, int dy) noexcept {
    RECT out{r.left + dx, r.top + dy, r.right - dx, r.bottom - dy};
    if (out.right < out.left) out.right = out.left;
    if (out.bottom < out.top) out.bottom = out.top;
    return out;
}

inline void fill_bar(HDC dc, int x, int y, int w, int h, nfui::Color color) noexcept {
    if (w <= 0 || h <= 0) return;
    const RECT r{x, y, x + w, y + h};
    nfui::fill_rect(dc, r, color);
}

// Cell-local colour resolution. `pressed` resolves to a mostly-accent fill
// (see state_palette in src/theme/Theme.cpp), so the ink has to swap to
// accent_text or the glyph disappears into the fill.
struct Cell {
    const nfui::ThemePalette* base{};
    nfui::StatePalette        sp{};
    nfui::ControlState        state{nfui::ControlState::rest};
    const nfui::DpiScale*     dpi{};

    [[nodiscard]] int px(int logical) const noexcept {
        return dpi != nullptr ? dpi->logical_to_pixels(logical) : logical;
    }
    [[nodiscard]] nfui::Color ink() const noexcept {
        return state == nfui::ControlState::pressed && base != nullptr ? base->accent_text
                                                                       : sp.foreground;
    }
    // Placeholder "text" bars inside a thumbnail: readable but clearly not
    // real glyphs, so the eye reads shape + state rather than trying to
    // parse 5-pixel-tall lettering.
    [[nodiscard]] nfui::Color ghost() const noexcept {
        return nfui::alpha_blend(ink(), sp.background, 0.55f);
    }
    [[nodiscard]] nfui::Color hairline() const noexcept {
        return nfui::alpha_blend(sp.border, sp.background, 0.7f);
    }
};

inline void draw_check_glyph(HDC dc, const RECT& box, nfui::Color color, int width) noexcept {
    const int w = box.right - box.left;
    const int h = box.bottom - box.top;
    if (w <= 3 || h <= 3) return;
    POINT pts[3]{
        {box.left + w / 5,     box.top + h / 2},
        {box.left + w * 2 / 5, box.bottom - h / 4},
        {box.right - w / 5,    box.top + h / 4},
    };
    nfui::draw_polyline(dc, pts, 3, color, width);
}

inline void paint_check_box(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const int pad  = c.px(6);
    const int side = (detail::imin)(cell.bottom - cell.top - pad * 2, c.px(18));
    if (side <= 0) return;
    const int top  = cell.top + ((cell.bottom - cell.top) - side) / 2;
    const RECT box{cell.left + pad, top, cell.left + pad + side, top + side};

    // A checked box reads the state best: the fill carries the state colour
    // and the tick carries the ink colour.
    const nfui::Color fill = c.state == nfui::ControlState::disabled ? c.sp.background : c.sp.accent;
    const nfui::Color tick = c.state == nfui::ControlState::disabled ? c.sp.foreground
                                                                     : (c.base != nullptr ? c.base->accent_text
                                                                                          : c.sp.foreground);
    nfui::fill_rounded_rect(dc, box, c.px(3), fill, c.sp.border);
    draw_check_glyph(dc, detail::inset_rect(box, c.px(2), c.px(2)), tick, (detail::imax)(1, c.px(2)));

    // Caption stand-in.
    const int label_x = box.right + c.px(6);
    const int label_w = cell.right - c.px(6) - label_x;
    fill_bar(dc, label_x, top + side / 2 - c.px(3), (detail::imax)(0, label_w), c.px(3), c.ghost());
}

inline void paint_radio_button(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const int pad  = c.px(6);
    const int side = (detail::imin)(cell.bottom - cell.top - pad * 2, c.px(18));
    if (side <= 0) return;
    const int top  = cell.top + ((cell.bottom - cell.top) - side) / 2;
    const RECT ring{cell.left + pad, top, cell.left + pad + side, top + side};

    nfui::fill_ellipse(dc, ring, c.sp.background);
    nfui::draw_ellipse(dc, ring, c.sp.border, (detail::imax)(1, c.px(1)));
    const RECT dot = detail::inset_rect(ring, side / 4, side / 4);
    nfui::fill_ellipse(dc, dot,
                       c.state == nfui::ControlState::disabled ? c.sp.foreground : c.sp.accent);

    const int label_x = ring.right + c.px(6);
    const int label_w = cell.right - c.px(6) - label_x;
    fill_bar(dc, label_x, top + side / 2 - c.px(3), (detail::imax)(0, label_w), c.px(3), c.ghost());
}

inline void paint_edit(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const int pad = c.px(6);
    const int h   = (detail::imin)(cell.bottom - cell.top - pad * 2, c.px(22));
    if (h <= 0) return;
    const int top = cell.top + ((cell.bottom - cell.top) - h) / 2;
    const RECT box{cell.left + pad, top, cell.right - pad, top + h};
    nfui::fill_rounded_rect(dc, box, c.px(4), c.sp.background, c.sp.border);

    // Caret + text run. The caret is accent-coloured on focus so the focused
    // cell is distinguishable from rest even in a greyscale print.
    const int text_y = top + h / 2 - c.px(2);
    fill_bar(dc, box.left + c.px(5), text_y, c.px(3), c.px(4),
             c.state == nfui::ControlState::focused ? c.sp.accent : c.ghost());
    fill_bar(dc, box.left + c.px(11), text_y, (box.right - box.left) / 2, c.px(3), c.ghost());
}

inline void paint_combo_box(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const int pad = c.px(6);
    const int h   = (detail::imin)(cell.bottom - cell.top - pad * 2, c.px(22));
    if (h <= 0) return;
    const int top = cell.top + ((cell.bottom - cell.top) - h) / 2;
    const RECT box{cell.left + pad, top, cell.right - pad, top + h};
    nfui::fill_rounded_rect(dc, box, c.px(4), c.sp.background, c.sp.border);

    fill_bar(dc, box.left + c.px(6), top + h / 2 - c.px(2), (box.right - box.left) / 2, c.px(3), c.ghost());

    // Chevron: framework combo boxes draw their own, so the thumbnail must
    // too — a native arrow here would be exactly the grey chrome the audit
    // flagged.
    const int cx = box.right - c.px(9);
    const int cy = top + h / 2;
    const int r  = c.px(4);
    POINT chevron[3]{{cx - r, cy - r / 2}, {cx + r, cy - r / 2}, {cx, cy + r}};
    nfui::fill_polygon(dc, chevron, 3,
                       c.state == nfui::ControlState::disabled ? c.ghost() : c.ink(),
                       c.state == nfui::ControlState::disabled ? c.ghost() : c.ink());
}

inline void paint_list_view(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const RECT box = detail::inset_rect(cell, c.px(6), c.px(5));
    if (box.right <= box.left || box.bottom <= box.top) return;
    nfui::fill_rounded_rect(dc, box, c.px(4), c.sp.background, c.sp.border);

    const int header_h = (detail::imax)(c.px(7), (box.bottom - box.top) / 4);
    const RECT header{box.left + 1, box.top + 1, box.right - 1, box.top + header_h};
    nfui::fill_rect(dc, header, nfui::alpha_blend(c.sp.border, c.sp.background, 0.45f));
    fill_bar(dc, header.left + c.px(4), header.top + header_h / 2 - c.px(1),
             (box.right - box.left) / 3, c.px(2), c.ghost());

    const int rows_top = header.bottom + c.px(2);
    const int row_h    = (detail::imax)(c.px(5), (box.bottom - rows_top - c.px(2)) / 2);
    // Row 0 is the selection row so every cell shows the selected-row chrome
    // in that state (the audit called out the grey header + grey selection).
    const RECT sel{box.left + 1, rows_top, box.right - 1, (detail::imin)(box.bottom - 1, rows_top + row_h)};
    if (sel.bottom > sel.top) {
        nfui::fill_rect(dc, sel,
                        c.state == nfui::ControlState::disabled
                            ? nfui::alpha_blend(c.sp.border, c.sp.background, 0.35f)
                            : c.sp.accent);
        fill_bar(dc, sel.left + c.px(4), (sel.top + sel.bottom) / 2 - c.px(1),
                 (box.right - box.left) / 2, c.px(2),
                 c.state == nfui::ControlState::disabled
                     ? c.ghost()
                     : (c.base != nullptr ? c.base->accent_text : c.sp.foreground));
    }
    const int row1_y = sel.bottom + c.px(3);
    if (row1_y + c.px(2) < box.bottom - 1) {
        fill_bar(dc, box.left + c.px(4), row1_y, (box.right - box.left) * 2 / 5, c.px(2), c.ghost());
    }
}

inline void paint_tree_view(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const RECT box = detail::inset_rect(cell, c.px(6), c.px(5));
    if (box.right <= box.left || box.bottom <= box.top) return;
    nfui::fill_rounded_rect(dc, box, c.px(4), c.sp.background, c.sp.border);

    const int x0 = box.left + c.px(5);
    const int y0 = box.top + c.px(5);
    const int g  = c.px(4);

    // Disclosure chevron — framework-drawn, never the native +/- box.
    POINT arrow[3]{{x0, y0}, {x0 + g, y0 + g / 2}, {x0, y0 + g}};
    nfui::fill_polygon(dc, arrow, 3, c.ink(), c.ink());
    fill_bar(dc, x0 + g + c.px(3), y0 + g / 2 - c.px(1), (box.right - box.left) / 3, c.px(2), c.ink());

    // Two indented children; the first one carries the selection chrome.
    const int child_x = x0 + c.px(10);
    const int child_y = y0 + c.px(9);
    const RECT sel{child_x - c.px(2), child_y - c.px(2),
                   (detail::imin)(box.right - c.px(4), child_x + (box.right - box.left) / 2),
                   child_y + c.px(6)};
    if (sel.bottom < box.bottom && sel.right > sel.left) {
        nfui::fill_rounded_rect(dc, sel, c.px(3),
                                c.state == nfui::ControlState::disabled
                                    ? nfui::alpha_blend(c.sp.border, c.sp.background, 0.35f)
                                    : c.sp.accent,
                                c.state == nfui::ControlState::disabled
                                    ? nfui::alpha_blend(c.sp.border, c.sp.background, 0.35f)
                                    : c.sp.accent);
        fill_bar(dc, child_x, child_y + c.px(1), (box.right - box.left) / 3, c.px(2),
                 c.state == nfui::ControlState::disabled
                     ? c.ghost()
                     : (c.base != nullptr ? c.base->accent_text : c.sp.foreground));
    }
    const int child2_y = child_y + c.px(9);
    if (child2_y + c.px(2) < box.bottom - c.px(3)) {
        fill_bar(dc, child_x, child2_y, (box.right - box.left) / 4, c.px(2), c.ghost());
    }
}

inline void paint_tab_control(HDC dc, const RECT& cell, const Cell& c) noexcept {
    const RECT box = detail::inset_rect(cell, c.px(6), c.px(5));
    if (box.right <= box.left || box.bottom <= box.top) return;

    const int tab_h = (detail::imax)(c.px(9), (box.bottom - box.top) / 3);
    const int tab_w = (detail::imin)((box.right - box.left) / 2 - c.px(2), c.px(26));
    if (tab_w <= 0) return;

    const RECT body{box.left, box.top + tab_h, box.right, box.bottom};
    nfui::fill_rounded_rect(dc, body, c.px(4), c.sp.background, c.sp.border);

    const RECT active{box.left, box.top, box.left + tab_w, box.top + tab_h + c.px(2)};
    const RECT idle{active.right + c.px(2), box.top + c.px(2),
                    (detail::imin)(box.right, active.right + c.px(2) + tab_w), box.top + tab_h + c.px(2)};

    nfui::fill_rounded_rect(dc, idle, c.px(3),
                            nfui::alpha_blend(c.sp.border, c.sp.background, 0.4f), c.sp.border);
    nfui::fill_rounded_rect(dc, active, c.px(3), c.sp.background, c.sp.border);
    // Active-tab accent underline — the single strongest signal that the
    // strip is themed rather than native.
    fill_bar(dc, active.left + c.px(3), active.bottom - c.px(2),
             (detail::imax)(0, tab_w - c.px(6)), c.px(2),
             c.state == nfui::ControlState::disabled ? c.ghost() : c.sp.accent);
    fill_bar(dc, active.left + c.px(4), active.top + c.px(3),
             (detail::imax)(0, tab_w - c.px(8)), c.px(2), c.ghost());

    fill_bar(dc, body.left + c.px(5), body.top + c.px(5), (box.right - box.left) / 2, c.px(2), c.ghost());
}

} // namespace detail

// Paint one matrix cell. `cell` is in device pixels; every internal metric is
// derived from `dpi` so the thumbnail scales with Per-Monitor DPI V2.
inline void paint_matrix_cell(HDC dc,
                              const RECT& cell,
                              MatrixControl control,
                              nfui::ControlState state,
                              const nfui::ThemePalette& base,
                              const nfui::DpiScale& dpi) noexcept {
    if (dc == nullptr || cell.right <= cell.left || cell.bottom <= cell.top) {
        return;
    }

    detail::Cell c{};
    c.base  = &base;
    c.sp    = nfui::state_palette(base, state);
    c.state = state;
    c.dpi   = &dpi;

    // Cell plate: a faint recessed tile so adjacent states read as separate
    // samples instead of one continuous strip.
    nfui::fill_rounded_rect(dc, cell, dpi.logical_to_pixels(6),
                            nfui::alpha_blend(base.background, base.surface, 0.45f),
                            nfui::alpha_blend(base.border, base.surface, 0.35f));

    switch (control) {
    case MatrixControl::check_box:    detail::paint_check_box(dc, cell, c);    break;
    case MatrixControl::radio_button: detail::paint_radio_button(dc, cell, c); break;
    case MatrixControl::edit:         detail::paint_edit(dc, cell, c);         break;
    case MatrixControl::combo_box:    detail::paint_combo_box(dc, cell, c);    break;
    case MatrixControl::list_view:    detail::paint_list_view(dc, cell, c);    break;
    case MatrixControl::tree_view:    detail::paint_tree_view(dc, cell, c);    break;
    case MatrixControl::tab_control:  detail::paint_tab_control(dc, cell, c);  break;
    }

    // Cross-cutting state affordances drawn on top of every thumbnail so a
    // reader can identify the state without reading the column header.
    if (state == nfui::ControlState::focused) {
        nfui::paint_focus_border(dc, cell, c.sp.accent, (detail::imax)(1, dpi.logical_to_pixels(2)));
    } else if (state == nfui::ControlState::error) {
        nfui::paint_focus_border(dc, cell, base.danger, (detail::imax)(1, dpi.logical_to_pixels(2)));
    }
}

} // namespace gallery

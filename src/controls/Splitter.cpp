#include <nfui/Controls/Splitter.hpp>
#include <nfui/Paint.hpp>

namespace nfui {
bool Splitter::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams splitter_params = params;
    // P1.4: drop SS_GRAYRECT; Splitter paints itself in palette.border (or the
    // consumer-supplied surface_brush). WS_CLIPCHILDREN keeps neighbour paint
    // from leaving streaks when the splitter is dragged. CP5A: not SS_OWNERDRAW
    // so we keep the base WM_PAINT path; hover is tracked manually via the
    // subclass hooks below because HoverState is gated on owner-draw.
    splitter_params.style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    return create_native(L"STATIC", splitter_params, 0);
}
void Splitter::set_ratio(double ratio) noexcept {
    if (ratio < 0.0) ratio_ = 0.0;
    else if (ratio > 1.0) ratio_ = 1.0;
    else ratio_ = ratio;
}
double Splitter::ratio() const noexcept { return ratio_; }

void Splitter::set_orientation(SplitterOrientation orientation) noexcept {
    if (orientation_ == orientation) return;
    orientation_ = orientation;
    // Cursor and any future per-orientation visual rely on this; force a
    // re-evaluation by re-applying the cursor if the mouse is currently over
    // us (we are tracking hovering_ ourselves).
    if (hovering_ && hwnd() != nullptr) {
        SetCursor(LoadCursorW(nullptr,
            orientation_ == SplitterOrientation::Horizontal ? IDC_SIZENS : IDC_SIZEWE));
    }
}

void Splitter::set_dragging(bool dragging) noexcept {
    if (dragging_ == dragging) return;
    dragging_ = dragging;
    // CP7: invalidate so the dragging colour (palette.accent) shows up on the
    // next paint cycle. Without this the bar stayed visually idle until the
    // next unrelated invalidate (which for a STATIC under a captive mouse
    // can be never). Cleared state also invalidates so the hit-line vanishes
    // and the bar returns to its hover/idle appearance.
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Splitter::on_subclass_mouse_move([[maybe_unused]] LPARAM lparam) noexcept {
    if (hwnd() == nullptr) return;
    // CP5A: STATIC controls are not owner-draw so the base HoverState stays
    // dormant. Track hover manually so paint + cursor respond to enter.
    if (!hovering_) {
        hovering_ = true;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
    SetCursor(LoadCursorW(nullptr,
        orientation_ == SplitterOrientation::Horizontal ? IDC_SIZENS : IDC_SIZEWE));
}

void Splitter::on_subclass_mouse_leave() noexcept {
    if (hovering_) {
        hovering_ = false;
        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }
}

void Splitter::on_paint(HDC dc, const PaintState& state) noexcept {
    // Dragging > hover > idle. All three derive from the palette so theme
    // transitions stay smooth (no hardcoded RGB corners). See P6.1 chrome
    // audit; CP5A layers hover paint on top of that; CP7 layers the drag
    // hit-line on top of that.
    (void)state; // hovering_/dragging_ tracked here, not in PaintState
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    Color fill;
    if (dragging_) {
        fill = p.accent;
    } else if (hovering_) {
        fill = p.surface_hover;
    } else {
        fill = style_.surface_brush.value_or(p.border);
    }
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, fill);
    if (dragging_) {
        // CP7: centre hit-line stripe so users can see exactly where the
        // drag axis sits, even on splitters thinner than 4 px. For a Vertical
        // splitter (cursor IDC_SIZEWE, drag axis horizontal) we draw a
        // horizontal line; for Horizontal (cursor IDC_SIZENS, drag axis
        // vertical) we draw a vertical line. Width = 2 px in
        // `p.accent_hover` so it contrasts the accent fill without forcing
        // a palette lookup that might be unset on legacy themes.
        const int w = paint_bounds.right - paint_bounds.left;
        const int h = paint_bounds.bottom - paint_bounds.top;
        if (orientation_ == SplitterOrientation::Vertical) {
            const int cy = paint_bounds.top + h / 2;
            RECT line{ paint_bounds.left, cy - 1, paint_bounds.right, cy + 1 };
            fill_rect(target, line, p.accent_hover);
        } else {
            const int cx = paint_bounds.left + w / 2;
            RECT line{ cx - 1, paint_bounds.top, cx + 1, paint_bounds.bottom };
            fill_rect(target, line, p.accent_hover);
        }
    }
}
} // namespace nfui

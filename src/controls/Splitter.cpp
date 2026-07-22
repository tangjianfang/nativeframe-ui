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
    // audit; CP5A layers hover paint on top of that.
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
}
} // namespace nfui

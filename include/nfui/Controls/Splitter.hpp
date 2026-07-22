#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

enum class SplitterOrientation {
    // Vertical drag-handle (the splitter is taller than it is wide). Mouse drag
    // resizes the left/right neighbour. Cursor: IDC_SIZEWE.
    Vertical,
    // Horizontal drag-handle (the splitter is wider than it is tall). Mouse
    // drag resizes the top/bottom neighbour. Cursor: IDC_SIZENS.
    Horizontal,
};

class Splitter : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_ratio(double ratio) noexcept;
    [[nodiscard]] double ratio() const noexcept;
    [[nodiscard]] bool is_dragging() const noexcept { return dragging_; }
    // Toggles the dragging state and invalidates the HWND so the new colour
    // (palette.accent vs. idle) repaints immediately. CP7: prior versions
    // assigned `dragging_` without invalidating, so the bar stayed visually
    // idle until the next paint cycle (which for a STATIC in a mouse loop
    // could be many seconds). Consumers should call this from their
    // WM_LBUTTONDOWN / WM_LBUTTONUP handlers.
    void set_dragging(bool dragging) noexcept;
    void set_orientation(SplitterOrientation orientation) noexcept;
    [[nodiscard]] SplitterOrientation orientation() const noexcept { return orientation_; }
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P1.4: Splitter paints itself in palette.border so a drag-handle chrome
    // picks up the Claude palette without subclassing the parent for
    // WM_CTLCOLORSTATIC. CP5A: hover paints palette.surface_hover and dragging
    // paints palette.accent so the user can tell the bar is interactive.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
    // CP5A: Splitter is not owner-draw so the base HoverState tracker stays
    // dormant. We track hover manually so the cursor + paint colour flip on
    // mouse enter/leave. SetCursor lives here (not in on_paint) so the cursor
    // updates the moment the mouse crosses the control, not the next time the
    // OS repaints.
    void on_subclass_mouse_move(LPARAM lparam) noexcept override;
    void on_subclass_mouse_leave() noexcept override;
    // CP17: advance the drag-hit-line pulse phase and request an async repaint
    // while dragging; stop the timer once drag ends.
    void on_animation_tick(unsigned long long now_ms) noexcept override;
private:
    double ratio_{0.5};
    bool dragging_{false};
    bool hovering_{false};
    SplitterOrientation orientation_{SplitterOrientation::Vertical};
    FrameStyle style_{};
    float pulse_phase_{0.0f};   // CP17: drag hit-line breathing phase
};

} // namespace nfui

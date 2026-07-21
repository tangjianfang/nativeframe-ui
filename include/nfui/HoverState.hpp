#pragma once

#include <windows.h>

namespace nfui {

// Tracks hover / pressed state for an owner-draw or custom-draw control.
// Lifts the WM_MOUSEMOVE + TrackMouseEvent + WM_MOUSELEAVE + InvalidateRect(FALSE)
// pattern out of any specific component so siblings can share it.
class HoverState {
public:
    void attach(HWND hwnd) noexcept;
    void detach() noexcept;

    [[nodiscard]] bool hover() const noexcept { return hover_; }
    [[nodiscard]] bool pressed() const noexcept { return pressed_; }

    void on_mouse_move(HWND hwnd) noexcept;
    void on_mouse_leave() noexcept;
    void on_lbutton_down() noexcept;
    void on_lbutton_up() noexcept;

private:
    bool hover_{false};
    bool pressed_{false};
    bool tracking_{false};
};

} // namespace nfui
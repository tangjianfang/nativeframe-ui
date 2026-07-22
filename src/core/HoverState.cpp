#include <nfui/HoverState.hpp>

namespace nfui {

void HoverState::attach(HWND) noexcept {
    tracking_ = false;
    hover_ = false;
    pressed_ = false;
}

void HoverState::detach() noexcept {
    tracking_ = false;
    hover_ = false;
    pressed_ = false;
}

void HoverState::on_mouse_move(HWND hwnd) noexcept {
    if (hover_) {
        return;
    }
    hover_ = true;
    if (hwnd != nullptr && !tracking_) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = static_cast<DWORD>(sizeof(tme));
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        if (TrackMouseEvent(&tme) == FALSE) {
            // P6.1: TrackMouseEvent failed (window mid-destroy, etc.). Fall
            // back to a posted WM_MOUSELEAVE so the leave state resets on the
            // next message-loop tick. See
            // docs/KNOWLEDGE/polish/2026-07-22-hoverstate-track-mouse-fallback.md.
            PostMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
            // Don't mark tracking_ true — we never registered.
        } else {
            tracking_ = true;
        }
    }
    if (hwnd != nullptr) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void HoverState::on_mouse_leave() noexcept {
    if (hover_) {
        hover_ = false;
    }
    tracking_ = false;
}

void HoverState::on_lbutton_down() noexcept {
    pressed_ = true;
}

void HoverState::on_lbutton_up() noexcept {
    pressed_ = false;
}

} // namespace nfui
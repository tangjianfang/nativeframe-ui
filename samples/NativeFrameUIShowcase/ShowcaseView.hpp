#pragma once

#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Theme.hpp>

#include <windows.h>

class ShowcaseView {
public:
    void set_theme_mode(nfui::ThemeMode mode) noexcept;
    [[nodiscard]] nfui::ThemeMode theme_mode() const noexcept;
    void toggle_theme() noexcept;

    void set_dpi(int dpi) noexcept;
    [[nodiscard]] int dpi() const noexcept;

    void set_client_rect(const RECT& rect) noexcept;

    [[nodiscard]] int hovered_card() const noexcept;
    [[nodiscard]] int selected_navigation() const noexcept;

    [[nodiscard]] bool on_mouse_move(POINT point) noexcept;
    [[nodiscard]] bool on_left_button_down(POINT point) noexcept;
    [[nodiscard]] bool clear_hover() noexcept;

    // Keyboard focus across the painted chrome. Tab order:
    //   0 = theme toggle button
    //   1..5 = sidebar nav rows (Overview..Settings)
    //   6 = notification bell button
    // Shift+Tab reverses. Enter / Space activate the focused affordance.
    // focus_index == -1 means no focus.
    [[nodiscard]] int focus_index() const noexcept { return focus_index_; }
    void set_focus_index(int index) noexcept;
    void cycle_focus(bool reverse) noexcept;
    [[nodiscard]] bool activate_focused() noexcept;
    [[nodiscard]] RECT focused_rect() const noexcept;

    void paint(HDC hdc, nfui::FontCache& fonts) const noexcept;

private:
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};
    nfui::DpiScale dpi_scale_{96};
    RECT client_rect_{0, 0, 1280, 840};
    int hovered_card_{-1};
    int selected_navigation_{0};
    int focus_index_{-1};  // 0=toggle, 1..5=nav, 6=notification
};

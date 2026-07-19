#pragma once

#include <nfui/Dpi.hpp>
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

    void paint(HDC hdc) const;

private:
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};
    nfui::DpiScale dpi_scale_{96};
    RECT client_rect_{0, 0, 1280, 840};
    int hovered_card_{-1};
    int selected_navigation_{0};
};

#pragma once

#include <nfui/Controls/Control.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Animation.hpp>

#include <optional>

namespace nfui {

struct ButtonStyle {
    std::optional<int>   corner_radius;
    std::optional<Color> border_color;
    std::optional<Color> focus_ring_color;  // CP9 a11y: inset keyboard-focus ring; defaults to accent_text
    std::optional<int>   horizontal_padding;
    std::optional<int>   vertical_padding;
    std::optional<bool>  use_semibold;
};

class Button : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(ButtonStyle style) noexcept { style_ = style; }
    [[nodiscard]] const ButtonStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    // CP17: advance the hover cross-fade and request an async repaint while
    // the animation is still running; stop the timer once it settles.
    void on_animation_tick(unsigned long long now_ms) noexcept override;
    // CP17: a palette change mid-fade would otherwise keep interpolating the
    // OLD palette's accent/accent_hover (the endpoints were stamped at hover
    // start). Cancel so the next paint recomputes the face from the live
    // palette — during a theme cross-fade this makes the hovered button track
    // the interpolated palette instead of snapping at fade end.
    void on_palette_changed() noexcept override;
private:
    // CP17: discrete face for a given state, used both to seed the cross-fade
    // endpoints and as the fallback when animation is suppressed (HC / system
    // animations off / pressed / disabled).
    [[nodiscard]] Color face_for(const PaintState& state, const ThemePalette& p, bool hc) const noexcept;
    ButtonStyle style_{};
    ColorAnimation hover_anim_{};
    bool last_hover_{false};
};

} // namespace nfui
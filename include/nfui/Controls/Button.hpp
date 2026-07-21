#pragma once

#include <nfui/Controls/Control.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct ButtonStyle {
    std::optional<int>   corner_radius;
    std::optional<Color> border_color;
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
private:
    ButtonStyle style_{};
};

} // namespace nfui
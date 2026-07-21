#pragma once

#include <nfui/Controls/Control.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct TextStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<int>   horizontal_padding;
    std::optional<int>   vertical_padding;
    std::optional<bool>  use_semibold;
};

class StaticText : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(TextStyle style) noexcept { style_ = style; }
    [[nodiscard]] const TextStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
private:
    TextStyle style_{};
};

} // namespace nfui

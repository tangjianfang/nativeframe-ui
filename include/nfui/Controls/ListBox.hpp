#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct ListStyle {
    std::optional<int> row_height_extra;
    std::optional<int> horizontal_padding;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
    std::optional<bool> rounded_selection;
};

class ListBox : public Control {
    friend class Control;
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(ListStyle style) noexcept { style_ = style; }
    [[nodiscard]] const ListStyle& style() const noexcept { return style_; }
protected:
    void draw_item(DRAWITEMSTRUCT* di) noexcept;
private:
    ListStyle style_{};
};

} // namespace nfui

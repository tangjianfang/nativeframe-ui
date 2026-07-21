#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct IconViewStyle {
    std::optional<int>  pixel_size;
    std::optional<bool> preserve_aspect;
};

class IconView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_icon(HICON icon) noexcept;
    void set_style(IconViewStyle style) noexcept { style_ = style; }
    [[nodiscard]] const IconViewStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    HICON icon_{};
    IconViewStyle style_{};
};

} // namespace nfui
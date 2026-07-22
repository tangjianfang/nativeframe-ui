#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class StatusBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }

protected:
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
    void on_paint(HDC dc, const PaintState& state) noexcept override;

private:
    FrameStyle style_{};
};

} // namespace nfui

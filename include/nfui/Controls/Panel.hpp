#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class Panel : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P1.4: Panel paints itself with palette.surface (or the consumer-supplied
    // surface_brush) rather than relying on SS_WHITERECT, so the background
    // tracks the Claude palette across themes.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
private:
    FrameStyle style_{};
};

} // namespace nfui

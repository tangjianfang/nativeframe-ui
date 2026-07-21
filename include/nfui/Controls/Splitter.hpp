#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class Splitter : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_ratio(double ratio) noexcept;
    [[nodiscard]] double ratio() const noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
private:
    double ratio_{0.5};
    FrameStyle style_{};
};

} // namespace nfui
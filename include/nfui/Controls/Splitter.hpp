#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class Splitter : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_ratio(double ratio) noexcept;
    [[nodiscard]] double ratio() const noexcept;
    [[nodiscard]] bool is_dragging() const noexcept { return dragging_; }
    void set_dragging(bool dragging) noexcept { dragging_ = dragging; }
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P1.4: Splitter paints itself in palette.border so a drag-handle chrome
    // picks up the Claude palette without subclassing the parent for
    // WM_CTLCOLORSTATIC.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
private:
    double ratio_{0.5};
    bool dragging_{false};
    FrameStyle style_{};
};

} // namespace nfui

#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class ProgressBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P1.4: ProgressBar paints itself by polling PBM_GETRANGE + PBM_GETPOS.
    // The track uses palette.surface with palette.border hairline and a
    // rounded radius sourced from theme_metrics().corner_radius_control
    // (matches Button); the filled portion uses style_.bar_color or
    // palette.accent as a fallback. Disabled uses palette.text_secondary
    // for a fill that clears WCAG AA contrast against the track. Avoids
    // relying on themed PBS_SMOOTH chrome, which is fixed in modern ComCtl32.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
private:
    FrameStyle style_{};
};

} // namespace nfui

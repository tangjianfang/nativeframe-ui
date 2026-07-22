#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class Tooltip : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // CP6: re-applies chrome_text / chrome_bg through TTM_SETTIPTEXTCOLOR /
    // TTM_SETTIPBKCOLOR whenever the palette is swapped (Control::set_palette
    // already routes to this hook and invalidates). Also called once at create
    // time so the initial palette is honoured.
    void on_palette_changed() noexcept override;
private:
    FrameStyle style_{};
};

} // namespace nfui
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
    // P6.1: self-paint chrome with palette.surface so theme transitions are
    // clean; SB_SETBKCOLOR is unsupported in ComCtl32 v6 (see
    // docs/KNOWLEDGE/polish/2026-07-22-statusbar-theme-color.md).
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
    void on_subclass_mouse_move([[maybe_unused]] LPARAM lparam) noexcept override {}
    void on_subclass_mouse_leave() noexcept override {}
private:
    FrameStyle style_{};
};

} // namespace nfui

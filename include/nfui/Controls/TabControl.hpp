#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class TabControl : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }

    // CP8A: forward TCM_SETPADDING so consumers can tune horizontal / vertical
    // padding around each tab's icon and label. The native TabControl default
    // is approximately (cx=6, cy=3) logical pixels. Pass already-DPI-scaled
    // device pixels — the control does not multiply by DPI itself (matches
    // every other layout helper that takes the wrapper's HWND as a child of a
    // DPI-aware window).  Returns true when the underlying HWND accepts the
    // message (false when the HWND has not been created yet, or when running
    // on ComCtl32 v4.0–4.69 which does not implement TCM_SETPADDING).
    [[nodiscard]] bool set_padding(int cx, int cy) noexcept;
private:
    FrameStyle style_{};
};

} // namespace nfui
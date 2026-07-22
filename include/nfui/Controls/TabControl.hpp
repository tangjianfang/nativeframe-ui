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
protected:
    // CP20: invalidate so the chrome subclass repaints tabs against the new
    // palette. Per-tab colour resolution happens inside handle_custom_draw
    // (reads palette + FrameStyle on every paint cycle).
    void on_palette_changed() noexcept override;
private:
    // CP20: NM_CUSTOMDRAW chrome subclass. Self-paints tab fills + captions
    // with palette + FrameStyle colours so the tab strip stops reading as an
    // OS-default grey island. TabControl's NM_CUSTOMDRAW payload is the
    // universal NMCUSTOMDRAW (the SDK does not define an NMCTCUSTOMDRAW; the
    // item-level extension structures are reserved for ListView / TreeView /
    // Tooltip / Toolbar), so dwItemSpec carries the tab index directly.
    void paint_tab(NMCUSTOMDRAW* cd) noexcept;
    LRESULT handle_custom_draw(NMCUSTOMDRAW* cd) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept;
    friend class Control;
    FrameStyle style_{};
};

} // namespace nfui

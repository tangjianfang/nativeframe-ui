#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

// CP42: self-painted tooltip. The native TOOLTIPS_CLASS window is retained for
// its hover tracking, delay timing, and positioning machinery (the public API
// stays TTM_ADDTOOL / TTM_UPDATETIPTEXT compatible), but every visible pixel
// is drawn from the injected ThemePalette: rounded surface fill, hairline
// border, palette text in the shared UI font. This removes the dark-mode
// bleach where native ComCtl32 chrome ignored the application theme, and keeps
// tip text metrics identical to the rest of the UI at any DPI.
//
// Balloon mode (FrameStyle::balloon) opts back into native paint — the tail
// shape cannot be reproduced by the flat self-paint path — with colours still
// applied through TTM_SETTIPTEXTCOLOR / TTM_SETTIPBKCOLOR.
class Tooltip : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(FrameStyle style) noexcept { style_ = style; }
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // CP6: re-applies chrome_text / chrome_bg through TTM_SETTIPTEXTCOLOR /
    // TTM_SETTIPBKCOLOR whenever the palette is swapped (Control::set_palette
    // already routes to this hook and invalidates). Also called once at create
    // time so the initial palette is honoured. The native colour path covers
    // the balloon fallback; the self-paint path reads the palette directly.
    void on_palette_changed() noexcept override;
private:
    [[nodiscard]] bool balloon_mode() const noexcept;
    // CP42: recomputes the tip window size from the current text, measured in
    // the shared UI font at the window's own DPI. Called after any TTM_*
    // message that can change text or wrap width, and whenever the window is
    // repositioned (a DPI transition changes the metrics). No-op when the
    // measured size already matches, so WM_WINDOWPOSCHANGED cannot recurse.
    void resize_to_text() noexcept;
    void paint_tip(HDC dc, const RECT& bounds) noexcept;
    static LRESULT CALLBACK paint_subclass_proc(HWND hwnd,
                                                UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam,
                                                UINT_PTR subclass_id,
                                                DWORD_PTR ref_data) noexcept;
    FrameStyle style_{};
};

} // namespace nfui
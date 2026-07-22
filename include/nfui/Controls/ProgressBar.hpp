#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class ProgressBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    // CP24-B: invalidate after style mutation so the chrome repaints with
    // the new tokens. Matches Control::set_palette and StatusBar::set_style.
    void set_style(FrameStyle style) noexcept;
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P1.4: ProgressBar paints itself by polling PBM_GETRANGE + PBM_GETPOS.
    // The track uses palette.surface with palette.border hairline and a
    // rounded radius sourced from theme_metrics().corner_radius_control
    // (matches Button); the filled portion uses style_.bar_color or
    // palette.accent as a fallback. Disabled uses palette.text_secondary
    // for a fill that clears WCAG AA contrast against the track. Avoids
    // relying on themed PBS_SMOOTH chrome, which is fixed in modern ComCtl32.
    //
    // CP24-B: a chrome subclass intercepts WM_PAINT and paints the full
    // themed bar (track + filled portion + optional value text). Returning
    // 0 short-circuits the base's DefSubclassProc chain, so PBS_SMOOTH
    // never paints pale grey chrome on top of our themed fill — even when
    // the caller adds PBS_SMOOTH or PBM_SETBARCOLOR. on_paint is the
    // fallback path: any caller that dispatches paint directly (rare)
    // routes to paint_chrome so the visual contract holds.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }

    // CP24-B: chrome paint helper. Same rationale as StatusBar::paint_chrome
    // — exposed to the per-instance subclass proc, which is a free function
    // and cannot reach `palette()` / `fonts()` directly because those are
    // protected on Control.
    void paint_chrome(HDC dc) noexcept;
private:
    // CP24-B: per-instance chrome subclass. SetWindowSubclass dispatches in
    // REVERSE install order (last-installed-runs-first); installed after the
    // base, our chrome proc intercepts WM_PAINT FIRST and short-circuits
    // the native pass. For non-WM_PAINT messages (sizing / WM_SETFONT /
    // WM_NCDESTROY) we fall through to DefSubclassProc, which reaches the
    // base chain entry.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
    FrameStyle style_{};
};

} // namespace nfui
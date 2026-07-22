#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Controls/FrameTypes.hpp>

namespace nfui {

class StatusBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    // CP23: invalidate after style mutation so the chrome repaints with the
    // new surface/foreground tokens. Matches the pattern in
    // Control::set_palette.
    void set_style(FrameStyle style) noexcept;
    [[nodiscard]] const FrameStyle& style() const noexcept { return style_; }
protected:
    // P6.1: self-paint chrome with palette.surface so theme transitions are
    // clean; SB_SETBKCOLOR is unsupported in ComCtl32 v6 (see
    // docs/KNOWLEDGE/polish/2026-07-22-statusbar-theme-color.md).
    // CP1: chained DefSubclassProc after on_paint so the OS still draws part
    // text / size grip on top of our palette background.
    // CP23: chrome subclass (visual_subclass_proc) intercepts WM_PAINT and
    // paints the full themed bar — background, top hairline, part text,
    // size grip — so the native ComCtl32 pass no longer draws pale grey
    // chrome on top of the themed background. on_paint is the fallback path:
    // any caller that dispatches paint directly (rare) routes to
    // paint_chrome so the visual contract holds.
    //
    // CP23 scope: paint_chrome paints part 0 only. Multi-part status bars
    // set up via SB_SETPARTS + SB_SETTEXTW(part, ...) will only show the
    // first part's text in our paint. Callers that need multi-part chrome
    // should either keep the bar single-part or extend StatusBar with a
    // multi-part paint path; for the framework samples (Workbench, etc.)
    // the single-part model is sufficient.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
    void on_subclass_mouse_move([[maybe_unused]] LPARAM lparam) noexcept override {}
    void on_subclass_mouse_leave() noexcept override {}

    // CP23: chrome paint helper exposed to the per-instance subclass proc
    // (which is a free function and cannot reach `palette()`/`fonts()`
    // directly because those are protected on Control). Public-but-naming-
    // underscored so the symbol stays out of the stable wrapper surface.
    void paint_chrome(HDC dc) noexcept;
private:
    // CP23: per-instance chrome subclass. SetWindowSubclass dispatches in
    // REVERSE install order — the LAST installed subclass runs FIRST. Our
    // chrome subclass is installed AFTER create_native's base subclass, so
    // our proc runs first for every message and short-circuits WM_PAINT
    // (returns 0) so the native ComCtl32 status-bar chrome never paints.
    // For non-WM_PAINT messages (sizing / WM_SETFONT / SB_SETTEXTW /
    // WM_NCDESTROY) we fall through to DefSubclassProc, which reaches the
    // base chain entry.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
    FrameStyle style_{};
};

} // namespace nfui
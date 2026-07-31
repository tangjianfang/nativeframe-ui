#pragma once

#include <nfui/Controls/Control.hpp>
#include <nfui/Theme.hpp>

namespace nfui {

// CP-A4: standalone scrollbar wrapper. Samples that need a scrollable
// panel — rather than the system ListView / TreeView / Edit scrollbars —
// instantiate this and feed it absolute pixel positions via
// set_range / set_position. The wrapper paints a transparent track and a
// 4px rounded thumb (palette.accent at 60% alpha), expanding to 8px on
// hover. Native scrollbar arrow buttons are suppressed via the style
// blend so the chrome reads as a single draggable thumb rather than the
// 1990s four-button look.
//
// The thumb paint routine is also reused by ListView / TreeView / Edit
// scrollbars via NM_CUSTOMDRAW if a future CP exposes it (deferred to
// per-demo plans — standalone Scrollbar is the entry point).
class Scrollbar : public Control {
public:
    // Pass `vertical == true` for a vertical scrollbar; `false` for a
    // horizontal one. style/position behaviour is otherwise symmetrical.
    [[nodiscard]] bool create(const ControlCreateParams& params,
                              bool vertical = true) noexcept;

    // Range / position helpers — wrap SetScrollRange / SetScrollPos / GetScrollPos
    // so callers do not juggle the (min, max) packing.
    void set_range(int min, int max) noexcept;
    void set_position(int pos) noexcept;
    [[nodiscard]] int position() const noexcept { return position_; }
    [[nodiscard]] int min() const noexcept { return min_; }
    [[nodiscard]] int max() const noexcept { return max_; }
    [[nodiscard]] bool vertical() const noexcept { return vertical_; }

    // CP-B19: static helper invoked by ListView / TreeView / Edit custom-draw
    // subclass procs to paint the themed thumb into an arbitrary track rect.
    // The forwarding contract (see docs/CHROME_QA.md):
    //   - `target` is the destination HDC (control's CDDS_POSTPAINT HDC).
    //   - `track` is the band along the right / bottom edge where the thumb
    //     lives, in client coordinates of the host control.
    //   - `position`, `min`, `max` mirror SetScrollPos / GetScrollPos.
    //   - `palette` is the active ThemePalette (passed explicitly so this
    //     helper is independent of the Scrollbar instance).
    // No native SCROLLBAR HWND is involved; the helper paints into the
    // target DC directly. See Scrollbar::paint_chrome for the matching
    // standalone-instance implementation.
    static void paint_thumb_into(HDC target, const RECT& track, bool vertical,
                                  int position, int min, int max,
                                  const ThemePalette& palette) noexcept;

protected:
    // CP-A4: react to palette changes by invalidating so the thumb's
    // accent + 60% alpha blend re-resolves against the new palette.
    void on_palette_changed() noexcept override;
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    // CP-A4: keep the thumb's "hovered" state in lockstep with the
    // mouse so the chrome proc can expand the thumb width on hover.
    void on_subclass_mouse_move(LPARAM lparam) noexcept override;
    void on_subclass_mouse_leave() noexcept override;

private:
    // CP-A4: chrome subclass owns the full paint so the themed track +
    // thumb replaces the native SCROLLBAR chrome (which is a grey
    // 4-button layout on every modern theme that never matches
    // palette.accent). Native SCROLLBAR is also flaky on dark themes
    // because the WS_SCROLLCHILDREN back-fill uses CLR_DEFAULT which
    // leaks through the new thumb colour.
    void paint_chrome(HDC dc) noexcept;
    // CP-A4: hit-test helper for WM_LBUTTONDOWN. Returns true if the
    // click landed on the thumb rect (false = bare track, no-op).
    [[nodiscard]] bool hit_test_thumb(POINT pt) const noexcept;
    // CP-A4: thumb geometry in client coordinates.
    [[nodiscard]] RECT thumb_rect() const noexcept;

    // CP-A4: chrome subclass proc. SetWindowSubclass dispatches in
    // reverse install order; ours runs FIRST on WM_PAINT and returns 0
    // so the native SCROLLBAR pass never paints. For all other
    // messages it falls through to DefSubclassProc.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;

    // CP-A4: live state mirrored from the SB_CTL so the chrome subclass
    // proc can answer queries without re-shelling into SendMessageW
    // on every paint.
    int position_{0};
    int min_{0};
    int max_{100};
    bool vertical_{true};
    bool hovered_thumb_{false};
    bool dragging_{false};
};

} // namespace nfui

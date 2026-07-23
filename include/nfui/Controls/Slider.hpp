#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

// CP25: slider chrome style overrides. All fields optional; missing values
// fall back to the injected ThemePalette tokens so consumers can leave the
// style default-constructed and still get a palette-driven thumb/track.
//
//   track_color  : un-filled portion of the rail (default: palette.border)
//   filled_color : rail from low edge to thumb (default: palette.accent)
//   thumb_color  : thumb disc (default: palette.accent)
//   thumb_radius : thumb radius in DIPs (default: theme_metrics().corner_radius_control)
struct SliderStyle {
    std::optional<Color> track_color;
    std::optional<Color> filled_color;
    std::optional<Color> thumb_color;
    std::optional<int>   thumb_radius;
};

class Slider : public Control {
public:
    enum class Orientation {
        horizontal,
        vertical,
    };

    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    // Configure slider orientation AFTER construction. Defaults to horizontal
    // when the create params omit TBS_VERT. Re-applying the style invalidates
    // so the chrome repaints against the new orientation metrics.
    void set_orientation(Orientation orientation) noexcept;
    [[nodiscard]] Orientation orientation() const noexcept { return orientation_; }

    // CP25: re-style; mirrors ProgressBar::set_style.
    void set_style(SliderStyle style) noexcept;
    [[nodiscard]] const SliderStyle& style() const noexcept { return style_; }

    // Range / position helpers — wrap TBM_SETRANGE / TBM_SETPOS so callers
    // don't juggle the (low, high) packing.
    void set_range(int low, int high) noexcept;
    void set_pos(int position) noexcept;
    [[nodiscard]] int pos() const noexcept;
    [[nodiscard]] int range_low() const noexcept;
    [[nodiscard]] int range_high() const noexcept;

protected:
    // CP25: chrome subclass owns the full paint so the themed track + thumb
    // replaces the native TRACKBAR chrome (which is grey on every modern
    // theme and never matches palette.accent). on_paint is the fallback
    // path used by any caller that dispatches paint directly.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }

    // CP25: chrome paint helper exposed to the per-instance subclass proc,
    // which is a free function and cannot reach the protected palette().
    void paint_chrome(HDC dc) noexcept;

private:
    // CP25: chrome subclass proc. SetWindowSubclass dispatches in reverse
    // install order; ours runs FIRST on WM_PAINT and returns 0 so the
    // native TRACKBAR pass never paints grey chrome on top of our fill.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;

    SliderStyle style_{};
    Orientation orientation_{Orientation::horizontal};
};

} // namespace nfui

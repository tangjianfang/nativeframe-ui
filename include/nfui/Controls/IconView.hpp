#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>
#include <nfui/VectorIcon.hpp>

#include <optional>

namespace nfui {

struct IconViewStyle {
    // Fixed *logical* pixel size. Scaled to device pixels internally so the
    // icon keeps a constant physical size across DPI changes. When set,
    // overrides preserve_aspect — the resulting rect is always square.
    std::optional<int>  pixel_size;

    // Centred square fit inside the client rect. Ignored when pixel_size is
    // set (which produces its own square). Default is false for back-compat
    // with pre-CP5 samples that relied on full-rect stretching.
    std::optional<bool> preserve_aspect;

    // Background fill drawn before the icon. When unset, falls back to the
    // injected palette's background so an empty IconView matches the rest of
    // the chrome instead of leaving the system static-bg visible.
    std::optional<Color> background;

    // Logical padding around the icon (centred). Useful for grid layouts that
    // want a consistent gap between adjacent IconView instances.
    std::optional<int>  padding;
};

class IconView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_icon(HICON icon) noexcept;
    // CP18: vector glyph mode. When set, the IconView draws a theme-coloured
    // vector glyph instead of a raster HICON — useful for common UI glyphs
    // (chevrons, check, search, gear, …) that have no resource dependency.
    // Passing IconKind::none clears the glyph and falls back to the raster
    // icon (if any). `color` is the glyph colour (caller-resolved palette).
    void set_glyph(IconKind kind, Color color) noexcept;
    void set_style(IconViewStyle style) noexcept { style_ = style; }
    [[nodiscard]] const IconViewStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    HICON icon_{};
    IconKind glyph_{IconKind::none};
    Color glyph_color_{};
    IconViewStyle style_{};
};

} // namespace nfui
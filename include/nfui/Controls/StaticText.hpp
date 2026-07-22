#pragma once

#include <nfui/Controls/Control.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

// Horizontal alignment for StaticText. Matches the three visible alignments in
// the ComponentGallery / ThemeDemo samples; defaults to Left for back-compat.
enum class StaticTextAlignH {
    left,
    center,
    right,
};

// Vertical alignment. Defaults to Middle so a single-line StaticText sits on
// the visual centre of its bounds; existing callers that sized to text height
// are unaffected.
enum class StaticTextAlignV {
    top,
    middle,
    bottom,
};

struct TextStyle {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<int>   horizontal_padding;   // logical px around the text rect
    std::optional<int>   vertical_padding;     // logical px around the text rect
    std::optional<bool>  use_semibold;         // weight override (headings)
    std::optional<int>   font_size_pt;         // logical point size override (default 9)
    std::optional<StaticTextAlignH> align_h;   // default Left
    std::optional<StaticTextAlignV> align_v;   // default Middle
    std::optional<bool>  single_line;          // default true; false enables word-wrap
    std::optional<bool>  end_ellipsis;         // default true (single-line only)
};

class StaticText : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(TextStyle style) noexcept { style_ = style; }
    void set_caption(const std::wstring& text) noexcept;
    [[nodiscard]] const TextStyle& style() const noexcept { return style_; }
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
private:
    TextStyle style_{};
};

} // namespace nfui
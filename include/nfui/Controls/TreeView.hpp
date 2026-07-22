#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct TreeViewStyle {
    std::optional<Color> row_background;     // empty-area background (TVM_SETBKCOLOR)
    std::optional<Color> row_foreground;     // base item text (TVM_SETTEXTCOLOR)
    std::optional<Color> line_color;         // indent guides + dots (TVM_SETLINECOLOR)
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
};

class TreeView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(TreeViewStyle style) noexcept { style_ = style; }
    [[nodiscard]] const TreeViewStyle& style() const noexcept { return style_; }
protected:
    // CP20: re-applies the base TVM_SET*COLOR values on palette swap.
    void on_palette_changed() noexcept override;
private:
    // CP20: chrome subclass for NMTVCUSTOMDRAW. Row chrome is fully driven by
    // clrText / clrTextBk at CDDS_ITEMPREPAINT; the system honours them in
    // normal usage, and a post-paint fill_rect would erase the indent guides,
    // +/- glyph, and item text the system just rendered. Header / scrollbar
    // remain native (consistent with how ListView still uses the system
    // scrollbar); the row strip is fully palette-driven.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept;
    LRESULT handle_custom_draw(NMTVCUSTOMDRAW* cd) noexcept;

    friend class Control;
    TreeViewStyle style_{};
};

} // namespace nfui

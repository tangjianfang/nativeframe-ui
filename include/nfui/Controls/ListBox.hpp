#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <optional>

namespace nfui {

struct ListStyle {
    std::optional<int> row_height_extra;
    std::optional<int> horizontal_padding;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
    std::optional<bool> rounded_selection;
};

class ListBox : public Control {
    friend class Control;
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(ListStyle style) noexcept { style_ = style; }
    [[nodiscard]] const ListStyle& style() const noexcept { return style_; }

    // Per-row hover tracking. Called by Control::subclass_proc on WM_MOUSEMOVE /
    // WM_MOUSELEAVE for ListBox HWNDs. Invalidation is whole-control (cheap for
    // typical list sizes); per-row rect invalidation is deferred to V1.5.
    void set_hovered_row(int row) noexcept;
    [[nodiscard]] int hovered_row() const noexcept { return hovered_row_; }

    // Draws a single row into the per-row HDC supplied by OCM_DRAWITEM. Public
    // so Control::subclass_proc can dispatch here for ODT_LISTBOX; the function
    // itself is still safe to call from any code that owns a valid DRAWITEMSTRUCT.
    void draw_item(DRAWITEMSTRUCT* di) noexcept;
private:
    ListStyle style_{};
    int hovered_row_{-1};
};

} // namespace nfui

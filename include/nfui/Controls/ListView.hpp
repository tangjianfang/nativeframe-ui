#pragma once

#include <nfui/Controls.hpp>
#include <nfui/Theme.hpp>

#include <commctrl.h>
#include <optional>

namespace nfui {

struct ListViewStyle {
    std::optional<Color> row_background;
    std::optional<Color> row_foreground;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
    std::optional<bool>  full_row_select;
    std::optional<bool>  show_gridlines;
};

class ListView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_style(ListViewStyle style) noexcept { style_ = style; }
    [[nodiscard]] const ListViewStyle& style() const noexcept { return style_; }
protected:
    // Consumes NMLVCUSTOMDRAW during CDDS_ITEMPREPAINT; returns CDRF_NEWFONT or CDRF_DODEFAULT.
    LRESULT on_custom_draw_item(NMLVCUSTOMDRAW* cd) noexcept override;
private:
    friend class Control;
    ListViewStyle style_{};
};

} // namespace nfui

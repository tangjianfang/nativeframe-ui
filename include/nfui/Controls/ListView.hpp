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
    void on_palette_changed() noexcept override;
private:
    // CP20/CP21: header HWND obtained from ListView_GetHeader; themed via a
    // chrome subclass that handles WM_ERASEBKGND (band background; the SDK
    // does not define HDM_SETBKCOLOR) and CP21 NM_CUSTOMDRAW (column captions,
    // hot state, sort glyphs). The header is a separate common-control window,
    // so the chrome proc lives on its own HWND.
    void theme_header(HWND header) noexcept;
    void paint_header_item(HWND header, NMCUSTOMDRAW* cd) noexcept;
    LRESULT handle_header_custom_draw(HWND header, NMCUSTOMDRAW* cd) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept;
    static LRESULT CALLBACK header_subclass_proc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
    LRESULT handle_custom_draw(NMLVCUSTOMDRAW* cd) noexcept;

    friend class Control;
    ListViewStyle style_{};
};

} // namespace nfui

#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

class ComboBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

protected:
    void on_palette_changed() noexcept override;

private:
    void draw_item(DRAWITEMSTRUCT* draw_info) noexcept;
    void paint_chrome() noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
};

} // namespace nfui

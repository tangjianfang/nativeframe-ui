#pragma once

#include <nfui/Controls/Control.hpp>

namespace nfui {

class Edit : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

protected:
    void on_palette_changed() noexcept override;

private:
    void paint_border() noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
};

} // namespace nfui

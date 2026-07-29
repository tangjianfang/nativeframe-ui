#pragma once

#include <nfui/Controls/Control.hpp>

#include <string>

namespace nfui {

// CP-A2: self-painted single-line edit. The native EDIT class still owns caret
// + selection behaviour (we forward WM_PAINT to DefSubclassProc), but every
// visible pixel is drawn from `state_palette()` so the chrome matches the rest
// of the application across light/dark/HC. A non-empty `placeholder_` is drawn
// in text_secondary when the field is empty.
class Edit : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    void set_placeholder(std::wstring_view text) noexcept;
    [[nodiscard]] const std::wstring& placeholder() const noexcept;

protected:
    void on_palette_changed() noexcept override;

private:
    void paint_border() noexcept;
    void paint_placeholder(HDC dc) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;

    std::wstring placeholder_;
};

} // namespace nfui
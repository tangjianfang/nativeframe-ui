#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

// CP-A2: self-painted check box with a full state matrix. Owner-draw
// (`BS_OWNERDRAW`) hands the entire client area to on_paint via the reflected
// WM_DRAWITEM path; the auto-toggle/check semantics remain in the
// subclass-proc overrides so `BM_GETCHECK` / `BM_SETCHECK` and
// click / space still work.
//
// Tristate: `tristate_` lets the caller advertise "supports indeterminate"
// without changing the actual indeterminate state — when set, the visual
// indeterminate dash can be reached and re-acknowledged; when false, the
// control cycles checked ↔ unchecked (matching native BS_AUTOCHECKBOX).
class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    void set_checked(bool checked) noexcept;
    void set_indeterminate(bool indeterminate) noexcept;
    void set_tristate(bool enabled) noexcept;
    [[nodiscard]] bool checked() const noexcept { return checked_; }
    [[nodiscard]] bool indeterminate() const noexcept { return indeterminate_; }
    [[nodiscard]] bool tristate() const noexcept { return tristate_; }

protected:
    // CP31: owner-draw paint. BS_OWNERDRAW replaces the native glyph; the auto-
    // toggle/check semantics are implemented in the subclass-proc overrides so
    // BM_GETCHECK / BM_SETCHECK and mouse/keyboard clicks still work.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] LRESULT on_reflected_get_check() const noexcept override;
    void on_reflected_set_check(LPARAM state) noexcept override;
    void on_subclass_lbutton_up() noexcept override;
    void on_subclass_key_down(UINT vk, LPARAM lparam) noexcept override;

private:
    void paint_check(HDC dc, const PaintState& state, const StatePalette& sp,
                     int box_size, int radius) noexcept;
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept;
    bool checked_{false};
    bool indeterminate_{false};
    bool tristate_{false};
};

} // namespace nfui
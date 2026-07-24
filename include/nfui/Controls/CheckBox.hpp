#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    void set_checked(bool checked) noexcept;
    void set_indeterminate(bool indeterminate) noexcept;
    [[nodiscard]] bool checked() const noexcept { return checked_; }
    [[nodiscard]] bool indeterminate() const noexcept { return indeterminate_; }

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
    bool checked_{false};
    bool indeterminate_{false};
};

} // namespace nfui
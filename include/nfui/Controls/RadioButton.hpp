#pragma once

// Control + ControlCreateParams live in <nfui/Controls.hpp>. We include that
// header to get the full base-class definition needed to derive from Control.
// Safe from circular include: <nfui/Controls.hpp> only includes this header
// AFTER its `class Control` definition, so when the include chain re-enters
// Controls.hpp its `#pragma once` skips and the already-defined Control is
// visible.
#include <nfui/Controls.hpp>

namespace nfui {

class RadioButton : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    void set_checked(bool checked) noexcept;
    [[nodiscard]] bool checked() const noexcept { return checked_; }

protected:
    // CP31: owner-draw paint. BS_OWNERDRAW replaces the native glyph; the
    // selected state is managed manually in the subclass-proc overrides so
    // BM_GETCHECK / BM_SETCHECK and click / space still work.
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] LRESULT on_reflected_get_check() const noexcept override;
    void on_reflected_set_check(LPARAM state) noexcept override;
    void on_subclass_lbutton_up() noexcept override;
    void on_subclass_key_down(UINT vk, LPARAM lparam) noexcept override;

private:
    bool checked_{false};
};

} // namespace nfui
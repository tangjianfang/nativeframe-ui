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
};

} // namespace nfui
#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

} // namespace nfui
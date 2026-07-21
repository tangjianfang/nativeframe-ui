#pragma once

#include <nfui/Controls/Control.hpp>

namespace nfui {

class Edit : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

} // namespace nfui

#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

class ComboBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

} // namespace nfui

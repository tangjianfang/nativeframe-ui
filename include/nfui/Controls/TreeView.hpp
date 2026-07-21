#pragma once
#include <nfui/Controls.hpp>
namespace nfui {
class TreeView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};
}
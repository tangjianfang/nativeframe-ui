#include <nfui/Controls/RadioButton.hpp>

namespace nfui {

bool RadioButton::create(const ControlCreateParams& params) noexcept {
    return create_native(L"BUTTON", params, BS_AUTORADIOBUTTON);
}

} // namespace nfui